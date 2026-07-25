#include "SongEditView.h"
#include "LibrarySongs.h"
#include "LibrarySongMeta.h"
#include "LibraryHelpers.h"
#include "frontend/ui/DesignSystem.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <functional>
#include <unordered_set>

namespace DS = ProyecThor::UI::DS;

namespace ProyecThor::UI {

// =============================================================================
//  InputText/InputTextMultiline atados a std::string, via
//  ImGuiInputTextFlags_CallbackResize — mismo idioma que misc/cpp/
//  imgui_stdlib.h (no vendorizado en este proyecto, asi que se replica aca en
//  lugar de agregar esa dependencia solo para esto). Reemplaza los buffers
//  fijos char[8192]/char[16384] de los editores viejos (RenderSongEditor/
//  SongView::RenderEditorModal), que este editor unificado no usa mas.
// =============================================================================
namespace {

struct StdStringCbData { std::string* str; };

int StdStringResizeCallback(ImGuiInputTextCallbackData* data)
{
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto* cb = static_cast<StdStringCbData*>(data->UserData);
        std::string* str = cb->str;
        IM_ASSERT(data->Buf == str->c_str());
        str->resize(data->BufTextLen);
        data->Buf = str->data();
    }
    return 0;
}

bool InputTextStd(const char* label, std::string* str, ImGuiInputTextFlags flags = 0)
{
    flags |= ImGuiInputTextFlags_CallbackResize;
    StdStringCbData cb{ str };
    return ImGui::InputText(label, str->data(), str->capacity() + 1, flags, StdStringResizeCallback, &cb);
}

bool InputTextMultilineStd(const char* label, std::string* str, const ImVec2& size, ImGuiInputTextFlags flags = 0)
{
    flags |= ImGuiInputTextFlags_CallbackResize;
    StdStringCbData cb{ str };
    return ImGui::InputTextMultiline(label, str->data(), str->capacity() + 1, size, flags, StdStringResizeCallback, &cb);
}

std::string TrimLine(const std::string& s)
{
    size_t start = s.find_first_not_of(" \t");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t");
    return s.substr(start, end - start + 1);
}

// Lineas de UNA diapositiva ya agrupada — mismo shape que splitLines en
// SongView.cpp. Tambien sirve como split generico de un texto completo
// (no descarta lineas en blanco), reusado por las excepciones de abajo.
std::vector<std::string> SplitSlideLines(const std::string& stanza)
{
    std::vector<std::string> lines;
    size_t sp = 0, ep = stanza.find('\n');
    while (true) {
        std::string ln = stanza.substr(sp, ep - sp);
        if (!ln.empty() && ln.back() == '\r') ln.pop_back();
        lines.push_back(ln);
        if (ep == std::string::npos) break;
        sp = ep + 1;
        ep = stanza.find('\n', sp);
    }
    return lines;
}

std::string CollapseSpaces(const std::string& s)
{
    std::string out;
    bool lastSpace = false;
    for (char c : s) {
        bool isSpace = (c == ' ' || c == '\t');
        if (isSpace && lastSpace) continue;
        out += c;
        lastSpace = isSpace;
    }
    return out;
}

// Aplica <transformLine> a cada linea NO vacia de <text>. Las lineas que
// ya eran blancas (separadores reales de estrofa) se preservan tal cual;
// las que dejan de tener contenido DESPUES de la transformacion se
// eliminan por completo (no se convierten en un salto de estrofa nuevo
// que no existia). Colapsa blancos consecutivos que puedan quedar.
std::string TransformLinesPreservingStanzas(
    const std::string& text,
    const std::function<std::string(const std::string&)>& transformLine)
{
    auto lines = SplitSlideLines(text);
    std::vector<std::string> outLines;
    for (const auto& raw : lines) {
        if (TrimLine(raw).empty()) { outLines.push_back(""); continue; }
        std::string transformed = TrimLine(transformLine(raw));
        if (transformed.empty()) continue; // la linea se cae entera
        outLines.push_back(transformed);
    }

    std::string result;
    bool prevBlank = true; // evita una linea en blanco al principio
    for (size_t i = 0; i < outLines.size(); ++i) {
        bool isBlank = outLines[i].empty();
        if (isBlank && prevBlank) continue;
        result += outLines[i];
        if (i + 1 < outLines.size()) result += "\n";
        prevBlank = isBlank;
    }
    while (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}

// Excepcion: quitar todo el texto entre parentesis (soporta anidados con
// un contador de profundidad) de una linea.
std::string StripParenthesesLine(const std::string& line)
{
    std::string stripped;
    int depth = 0;
    for (char c : line) {
        if (c == '(') { ++depth; continue; }
        if (c == ')') { if (depth > 0) --depth; continue; }
        if (depth <= 0) stripped += c;
    }
    return CollapseSpaces(stripped);
}

// Excepcion: quitar marcadores "//" (y espacios pegados) de una linea —
// convencion usada por algunos para marcar "repetir" que a veces la gente
// prefiere no ver proyectada literalmente.
std::string StripSlashMarkersLine(const std::string& line)
{
    std::string out;
    for (size_t i = 0; i < line.size(); ) {
        if (line[i] == '/' && i + 1 < line.size() && line[i + 1] == '/') {
            i += 2;
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
            continue;
        }
        out += line[i];
        ++i;
    }
    return CollapseSpaces(out);
}

// Divide en estrofas (parrafos separados por linea en blanco), cada una
// como un solo string con sus lineas unidas por '\n' (sin trailing '\n').
std::vector<std::string> SplitIntoParagraphs(const std::string& text)
{
    std::vector<std::string> paragraphs;
    std::vector<std::string> current;
    for (const auto& ln : SplitSlideLines(text)) {
        if (TrimLine(ln).empty()) {
            if (!current.empty()) {
                std::string p;
                for (size_t i = 0; i < current.size(); ++i) {
                    p += current[i];
                    if (i + 1 < current.size()) p += "\n";
                }
                paragraphs.push_back(p);
                current.clear();
            }
        } else {
            current.push_back(ln);
        }
    }
    if (!current.empty()) {
        std::string p;
        for (size_t i = 0; i < current.size(); ++i) {
            p += current[i];
            if (i + 1 < current.size()) p += "\n";
        }
        paragraphs.push_back(p);
    }
    return paragraphs;
}

// Excepcion: elimina estrofas repetidas exactas (comparando sin importar
// mayusculas ni espacios de borde), conservando solo la primera aparicion.
std::string RemoveDuplicateVerses(const std::string& text)
{
    auto paragraphs = SplitIntoParagraphs(text);
    std::vector<std::string> unique;
    std::unordered_set<std::string> seen;
    for (auto& p : paragraphs) {
        std::string key = TrimLine(p);
        for (auto& c : key) c = (char)std::tolower((unsigned char)c);
        if (!key.empty()) {
            if (seen.count(key)) continue;
            seen.insert(key);
        }
        unique.push_back(p);
    }
    std::string result;
    for (size_t i = 0; i < unique.size(); ++i) {
        result += unique[i];
        if (i + 1 < unique.size()) result += "\n\n";
    }
    return result;
}

} // namespace

// =============================================================================
//  Open
// =============================================================================
void SongEditView::Open(const std::string& filename)
{
    m_Filename = filename;
    m_FilePath = Library::GetAssetsPath() + "/songs/" + filename;

    std::ifstream file(Library::U8Path(m_FilePath), std::ios::binary);
    std::string raw;
    if (file.is_open()) {
        raw.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
    }
    std::string content = Library::NormalizeToUtf8(raw);

    Library::SongMeta meta = Library::GetSongMeta(filename);

    m_Current = EditSnapshot{};
    m_Current.lyrics       = content;
    m_Current.baseLyrics   = content;
    m_Current.title        = !meta.title.empty() ? meta.title : Library::StripExtension(filename);
    m_Current.author       = !meta.artistAuthor.empty() ? meta.artistAuthor : Library::GetSongAuthor(filename);

    // Migracion de sidecars viejos ".autor.txt" (de versiones anteriores al
    // rework del editor, ver SongView.cpp original) — si no hay autor en
    // songs_authors.ini todavia, pero existe el sidecar legado, se migra el
    // valor y se borra el sidecar para que no siga apareciendo como cancion
    // fantasma en el listado.
    if (m_Current.author.empty()) {
        std::filesystem::path legacyPath(m_FilePath);
        legacyPath.replace_extension(".autor.txt");
        std::ifstream legacyStream(legacyPath);
        if (legacyStream.is_open()) {
            std::string legacyContent((std::istreambuf_iterator<char>(legacyStream)),
                                        std::istreambuf_iterator<char>());
            legacyStream.close();
            legacyContent.erase(std::remove(legacyContent.begin(), legacyContent.end(), '\r'), legacyContent.end());
            legacyContent.erase(std::remove(legacyContent.begin(), legacyContent.end(), '\n'), legacyContent.end());
            if (!legacyContent.empty()) {
                m_Current.author = legacyContent;
                Library::SetSongAuthor(filename, legacyContent);
            }
            std::error_code ec;
            std::filesystem::remove(legacyPath, ec);
        }
    }

    m_Current.note         = meta.note;
    m_Current.copyright    = meta.copyright;
    m_Current.extra        = meta.extra;
    m_Current.linesPerSlide = meta.linesPerSlide;

    m_Undo = EditSnapshot{};
    m_Redo = EditSnapshot{};
    m_HasUndo = false;
    m_HasRedo = false;

    m_Dirty     = false;
    m_JustSaved = false;
}

// =============================================================================
//  FlushIfDirty
// =============================================================================
void SongEditView::FlushIfDirty()
{
    if (!m_Dirty) return;

    std::error_code ec;
    std::filesystem::create_directories(Library::U8Path(Library::GetAssetsPath() + "/songs"), ec);

    std::ofstream f(Library::U8Path(m_FilePath), std::ios::out | std::ios::trunc | std::ios::binary);
    if (f.is_open()) {
        f << "\xEF\xBB\xBF";
        f << m_Current.lyrics;
    }

    Library::SetSongAuthor(m_Filename, m_Current.author);

    Library::SongMeta meta;
    meta.version        = 1;
    meta.title          = m_Current.title;
    meta.artistAuthor   = m_Current.author;
    meta.note           = m_Current.note;
    meta.copyright      = m_Current.copyright;
    meta.extra          = m_Current.extra;
    meta.linesPerSlide  = m_Current.linesPerSlide;
    Library::SetSongMeta(m_Filename, meta);

    m_Dirty     = false;
    m_JustSaved = true;
    m_JustSavedAt = ImGui::GetTime();
}

// =============================================================================
//  Undo / Redo (un solo paso — ver comentario en SongEditView.h)
// =============================================================================
void SongEditView::PushUndoSnapshot()
{
    m_Undo    = m_Current;
    m_HasUndo = true;
    m_HasRedo = false;
}

void SongEditView::Undo()
{
    if (!m_HasUndo) return;
    m_Redo    = m_Current;
    m_HasRedo = true;
    m_Current = m_Undo;
    m_HasUndo = false;
    MarkDirty();
}

void SongEditView::Redo()
{
    if (!m_HasRedo) return;
    m_Undo    = m_Current;
    m_HasUndo = true;
    m_Current = m_Redo;
    m_HasRedo = false;
    MarkDirty();
}

void SongEditView::MarkDirty()
{
    m_Dirty        = true;
    m_LastEditTime = ImGui::GetTime();
}

// =============================================================================
//  ComputePreviewSlides — mismo algoritmo que LoadSongVerses (en disco), pero
//  sobre el buffer EN MEMORIA + el linesPerSlide actual, para que el preview
//  de la derecha sea fiel mientras se edita (antes de que el autoguardado
//  vuelque a disco).
// =============================================================================
std::vector<std::string> SongEditView::ComputePreviewSlides() const
{
    return Library::GroupLyricsIntoSlides(m_Current.lyrics, m_Current.linesPerSlide);
}

// =============================================================================
//  RenderTopBar — una sola fila FINA, solo iconos (calcada de la barra de
//  Holyrics): Volver, Undo, Redo, Lineas-por-diapositiva (icono que abre un
//  popup chico con 1/2/3, no tres botones siempre visibles), y un punto de
//  estado de guardado en vez de texto permanente. Sin bloques de texto ni
//  controles sueltos — cualquier detalle vive en el tooltip del icono.
// =============================================================================
void SongEditView::RenderTopBar(bool& outWantsBack)
{
    outWantsBack = false;
    const float btnW = 34.0f;

    if (DS::GlassIconButton("back", "izquierda", "<", "Volver a la biblioteca", ImVec2(btnW, DS::ButtonHeight)))
        outWantsBack = true;

    ImGui::SameLine(0.0f, 10.0f);
    ImGui::BeginDisabled(!m_HasUndo);
    if (DS::GlassIconButton("undo", "arrow_back", "\xE2\x86\xB6" /* ↶ */, "Deshacer", ImVec2(btnW, DS::ButtonHeight)))
        Undo();
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!m_HasRedo);
    if (DS::GlassIconButton("redo", "arrow_forward", "\xE2\x86\xB7" /* ↷ */, "Rehacer", ImVec2(btnW, DS::ButtonHeight)))
        Redo();
    ImGui::EndDisabled();

    ImGui::SameLine(0.0f, 10.0f);
    if (DS::GlassIconButton("exceptions", "cards_star", "#", "Excepciones", ImVec2(btnW, DS::ButtonHeight)))
        ImGui::OpenPopup("Excepciones##exceptionsModal");

    // Modal (no un popup chico anclado al icono): siempre queda adelante,
    // centrado y con tamano fijo grande, para que no se "pierda" y sea
    // obvio que hay que elegir una opcion o cerrar con "Listo" — un popup
    // comun se podia cerrar sin querer con un click afuera y quedaba muy
    // chico/discreto para una accion que reescribe la letra.
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(520.0f, 560.0f), ImGuiCond_Appearing);

    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.07f, 0.07f, 0.08f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(1.0f, 1.0f, 1.0f, 0.14f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, DS::RadiusLarge);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 22.0f));

    if (ImGui::BeginPopupModal("Excepciones##exceptionsModal", nullptr, ImGuiWindowFlags_NoResize))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextPrimary));
        ImGui::TextUnformatted("Excepciones");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextHint));
        ImGui::TextWrapped("Ajustes opcionales para la letra. Ninguno se aplica solo: cada uno es un boton que transforma la letra actual una sola vez, cuando vos lo apretas.");
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Spacing();

        // ── Lineas por diapositiva ───────────────────────────────────────
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
        ImGui::TextUnformatted("Lineas por diapositiva");
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextHint));
        ImGui::TextWrapped("Elegi cuantas lineas entran en cada diapositiva. Se inserta una linea en blanco real en el texto (nunca cruza el limite de una estrofa). Se puede ir y volver entre 1/2/3 las veces que quieras: siempre parte de la letra sin cortes, no de la ya cortada.");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        const char* labels[3]  = { "1 linea", "2 lineas", "3 lineas" };
        const char* tips[3]    = {
            "Cada diapositiva muestra 1 sola linea de la letra.",
            "Cada diapositiva muestra 2 lineas de la letra.",
            "Cada diapositiva muestra 3 lineas de la letra."
        };
        float avail  = ImGui::GetContentRegionAvail().x;
        float gap    = 12.0f;
        float btnW2  = (avail - gap * 2.0f) / 3.0f;

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
        for (int n = 1; n <= 3; ++n) {
            if (n > 1) ImGui::SameLine(0.0f, gap);
            ImGui::PushID(n);
            if (ImGui::Button(labels[n - 1], ImVec2(btnW2, 56.0f))) {
                // El "filtro" no es un modo pegajoso: transforma la letra
                // UNA VEZ, insertando las lineas en blanco reales donde
                // corresponde. Parte SIEMPRE de baseLyrics (la letra sin
                // cortes), no de m_Current.lyrics — asi elegir 1, despues 2
                // y despues 3 siempre da el resultado correcto en vez de
                // re-cortar un texto que ya tenia lineas en blanco
                // insertadas por una aplicacion anterior (lo que antes
                // impedia UNIR lineas ya separadas).
                PushUndoSnapshot();
                std::vector<std::string> slides = Library::GroupLyricsIntoSlides(m_Current.baseLyrics, n);
                std::string newLyrics;
                for (size_t i = 0; i < slides.size(); ++i) {
                    newLyrics += slides[i];
                    if (i + 1 < slides.size()) newLyrics += "\n";
                }
                m_Current.lyrics       = newLyrics;
                m_Current.linesPerSlide = 0; // ya quedo reflejado de verdad en el texto
                MarkDirty();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                ImGui::SetTooltip("%s", tips[n - 1]);
            ImGui::PopID();
        }
        ImGui::PopStyleVar();

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1.0f, 1.0f, 1.0f, 0.06f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();

        // ── Otras excepciones (opcionales, desactivadas por defecto) ────
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
        ImGui::TextUnformatted("Otras excepciones");
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextHint));
        ImGui::TextWrapped("Totalmente opcionales: no se activan solas, las aplicas vos cuando las necesitas.");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);

        if (ImGui::Button("Quitar texto entre parentesis ( )", ImVec2(-1.0f, 40.0f))) {
            PushUndoSnapshot();
            std::string newLyrics = TransformLinesPreservingStanzas(m_Current.lyrics, StripParenthesesLine);
            m_Current.lyrics     = newLyrics;
            m_Current.baseLyrics = newLyrics;
            MarkDirty();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("Borra todo lo que este escrito entre parentesis, incluidos los parentesis.");

        ImGui::Spacing();
        if (ImGui::Button("Quitar marcadores // ", ImVec2(-1.0f, 40.0f))) {
            PushUndoSnapshot();
            std::string newLyrics = TransformLinesPreservingStanzas(m_Current.lyrics, StripSlashMarkersLine);
            m_Current.lyrics     = newLyrics;
            m_Current.baseLyrics = newLyrics;
            MarkDirty();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("Quita el simbolo // que algunos usan para marcar repeticion, por si no lo queres ver proyectado.");

        ImGui::Spacing();
        if (ImGui::Button("Eliminar versos repetidos", ImVec2(-1.0f, 40.0f))) {
            PushUndoSnapshot();
            std::string newLyrics = RemoveDuplicateVerses(m_Current.lyrics);
            m_Current.lyrics     = newLyrics;
            m_Current.baseLyrics = newLyrics;
            MarkDirty();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("Si una estrofa esta pegada dos o mas veces exactamente igual, deja solo la primera.");

        ImGui::PopStyleVar();

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1.0f, 1.0f, 1.0f, 0.06f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();

        float doneW = 120.0f;
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - doneW) * 0.5f);
        if (DS::GlassButton("Listo", ImVec2(doneW, DS::ButtonHeight)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    // ── Nombre de archivo + punto de estado de guardado, a la derecha ───────
    std::string statusTip = "Guardado";
    ImU32 dotCol = 0;
    if (m_Dirty) { dotCol = DS::TextHint; statusTip = "Guardando..."; }
    else if (m_JustSaved && (ImGui::GetTime() - m_JustSavedAt) < 1.5) { dotCol = DS::SuccessColor; statusTip = "Guardado"; }

    float nameW = ImGui::CalcTextSize(m_Filename.c_str()).x;
    float rightBlockW = nameW + (dotCol != 0 ? 16.0f : 0.0f);
    float rightX = ImGui::GetWindowWidth() - rightBlockW - 18.0f;
    if (rightX > ImGui::GetCursorPosX())
        ImGui::SameLine(rightX);
    else
        ImGui::SameLine(0.0f, 14.0f);

    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextHint));
    ImGui::TextUnformatted(m_Filename.c_str());
    ImGui::PopStyleColor();

    if (dotCol != 0) {
        ImGui::SameLine();
        ImVec2 c = ImGui::GetCursorScreenPos();
        float r = 4.0f;
        float lineH = ImGui::GetTextLineHeight();
        ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(c.x + r + 2.0f, c.y + lineH * 0.5f), r, dotCol);
        ImGui::Dummy(ImVec2(r * 2.0f + 6.0f, lineH));
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", statusTip.c_str());
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1.0f, 1.0f, 1.0f, 0.06f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

// =============================================================================
//  RenderLeftPane — el panel donde se escribe es lo mas importante de la
//  pantalla, asi que solo Titulo/Autor quedan siempre a la vista; el resto
//  de los metadatos (Nota/Derechos de autor/Extra) vive detras de un icono
//  de informacion que abre un popup chico, para no restarle espacio vertical
//  a la letra. No hay un icono "info" propio todavia en assets/icons/ui —
//  se usa el glifo "i" (ASCII, siempre renderiza) hasta que se agregue uno.
// =============================================================================
void SongEditView::RenderLeftPane(float width)
{
    ImGui::BeginChild("##editLeft", ImVec2(width, 0.0f), false);

    auto FieldInput = [&](const char* label, std::string& value) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        ImGui::SetNextItemWidth(-1.0f);
        std::string id = std::string("##") + label;
        InputTextStd(id.c_str(), &value);
        if (ImGui::IsItemActivated()) PushUndoSnapshot();
        if (ImGui::IsItemEdited())    MarkDirty();
    };

    float infoBtnW = DS::ButtonHeight;
    float titleW   = std::max(60.0f, ImGui::GetContentRegionAvail().x - infoBtnW - 8.0f);

    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
    ImGui::TextUnformatted("Titulo");
    ImGui::PopStyleColor();
    ImGui::SetNextItemWidth(titleW);
    InputTextStd("##Titulo", &m_Current.title);
    if (ImGui::IsItemActivated()) PushUndoSnapshot();
    if (ImGui::IsItemEdited())    MarkDirty();

    ImGui::SameLine(0.0f, 8.0f);
    if (DS::GlassIconButton("moreInfo", "info", "i", "Nota, derechos de autor y extra",
                            ImVec2(infoBtnW, DS::ButtonHeight)))
        ImGui::OpenPopup("moreSongInfoPopup");

    FieldInput("Autor", m_Current.author);

    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.07f, 0.07f, 0.08f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(1.0f, 1.0f, 1.0f, 0.14f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, DS::RadiusLarge);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));
    if (ImGui::BeginPopup("moreSongInfoPopup"))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
        ImGui::TextUnformatted("Mas datos de la cancion");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        ImGui::PushItemWidth(280.0f);
        FieldInput("Nota", m_Current.note);
        FieldInput("Derechos de autor", m_Current.copyright);
        FieldInput("Extra", m_Current.extra);
        ImGui::PopItemWidth();

        ImGui::Spacing();
        if (DS::GlassButton("Listo", ImVec2(90.0f, DS::ButtonHeight)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1.0f, 1.0f, 1.0f, 0.06f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    const float lyricsH = std::max(120.0f, ImGui::GetContentRegionAvail().y - 4.0f);
    InputTextMultilineStd("##lyrics", &m_Current.lyrics, ImVec2(-1.0f, lyricsH),
                          ImGuiInputTextFlags_AllowTabInput);
    if (ImGui::IsItemActivated()) PushUndoSnapshot();
    if (ImGui::IsItemEdited()) {
        // Escritura manual: lo que el usuario tipea/pega pasa a ser la
        // nueva letra "base" tambien, para que el filtro de lineas por
        // diapositiva parta siempre de lo ultimo escrito a mano.
        m_Current.baseLyrics = m_Current.lyrics;
        MarkDirty();
    }

    ImGui::EndChild();
}

// =============================================================================
//  RenderRightPane — preview en vivo (recalculado del buffer en memoria, no
//  del disco) de como van a quedar las diapositivas. El estilo/fondo por
//  verso individual se saco a proposito (ver comentario de la clase en
//  SongEditView.h): ya existe un preset de estilo/fondo por CANCION ENTERA
//  (tarjeta "Ajustes" del grid de estrofas en SongView) y tener los dos era
//  redundante — este preview es puramente informativo, sin iconos ni
//  overrides por tarjeta.
// =============================================================================
void SongEditView::RenderRightPane(float width)
{
    ImGui::BeginChild("##editRight", ImVec2(width, 0.0f), false);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Preview");
    ImGui::SameLine();
    DS::ModernSlider("##previewZoom", &m_PreviewZoom, 0.6f, 1.6f, 120.0f);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        ImGui::SetTooltip("Tamano de las tarjetas");
    ImGui::Spacing();

    std::vector<std::string> slides = ComputePreviewSlides();

    if (slides.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextHint));
        ImGui::TextWrapped("Pega la letra a la izquierda para ver el preview de las diapositivas.");
        ImGui::PopStyleColor();
        ImGui::EndChild();
        return;
    }

    ImFont* previewFont = ImGui::GetFont();
    const ImU32 textColorDefault = IM_COL32(235, 235, 238, 255);

    auto measureLines = [&](const std::vector<std::string>& lines, float size, float& outW, float& outH) {
        outW = 0.0f;
        for (const auto& ln : lines) {
            if (ln.empty()) continue;
            ImVec2 sz = previewFont->CalcTextSizeA(size, FLT_MAX, 0.0f, ln.c_str());
            outW = std::max(outW, sz.x);
        }
        outH = lines.size() * size;
    };

    // Tarjetas de tamano FIJO (16:9-ish, compactas como en el editor de
    // referencia) — antes usaban una tabla con ImGuiTableFlags_SizingStretchSame,
    // que las estiraba para llenar todo el ancho disponible y terminaban
    // gigantes con solo 1-2 por fila. Ahora se acomodan en flujo (wrap), cada
    // una a su tamano real, y el espacio sobrante simplemente queda vacio.
    float colWidth   = 190.0f * m_PreviewZoom;
    float cardHeight = 112.0f * m_PreviewZoom;
    float barH       = std::clamp(cardHeight * 0.20f, 14.0f, 22.0f);

    float uniformSize = 72.0f;
    {
        float pad_est   = std::clamp(std::min(colWidth, cardHeight) * 0.10f, 6.0f, 16.0f);
        float safeW_est = std::max(10.0f, colWidth - pad_est * 2.0f);
        float safeH_est = std::max(10.0f, cardHeight - barH - pad_est * 2.0f);
        for (const auto& s : slides) {
            auto lines = SplitSlideLines(s);
            float size = 10.0f;
            while (size < 72.0f) {
                float next = size + 1.0f, w, h;
                measureLines(lines, next, w, h);
                if (w > safeW_est || h > safeH_est) break;
                size = next;
            }
            uniformSize = std::min(uniformSize, size);
        }
    }

    const float windowVisibleX2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

    for (size_t i = 0; i < slides.size(); ++i) {
        const std::string& stanza = slides[i];
        ImGui::PushID((int)i);

        ImVec2 cardSize = ImVec2(colWidth, cardHeight);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        ImGui::BeginChild("card", cardSize, false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleColor();

        ImVec2 p_min = ImGui::GetWindowPos();
        ImVec2 p_max = ImVec2(p_min.x + cardSize.x, p_min.y + cardSize.y);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        auto lines = SplitSlideLines(stanza);

        dl->AddRectFilled(p_min, p_max, IM_COL32(42, 43, 48, 255), 10.0f);
        dl->AddRect(p_min, p_max, IM_COL32(255, 255, 255, 22), 10.0f, 0, 1.0f);

        ImVec2 textAreaMax = { p_max.x, p_max.y - barH };
        dl->PushClipRect(p_min, textAreaMax, true);

        float pad       = std::clamp(std::min(cardSize.x, cardSize.y) * 0.10f, 6.0f, 16.0f);
        float textAreaH = cardSize.y - barH;
        float blockW, blockH;
        measureLines(lines, uniformSize, blockW, blockH);
        float startY = std::max(pad, (textAreaH - blockH) * 0.5f);

        float curY = startY;
        for (const auto& ln : lines) {
            if (!ln.empty()) {
                ImVec2 sz = previewFont->CalcTextSizeA(uniformSize, FLT_MAX, 0.0f, ln.c_str());
                float lx = std::max(pad, (cardSize.x - sz.x) * 0.5f);
                dl->AddText(previewFont, uniformSize, { p_min.x + lx, p_min.y + curY }, textColorDefault, ln.c_str());
            }
            curY += uniformSize;
        }
        dl->PopClipRect();

        ImVec2 barMin = { p_min.x, p_max.y - barH };
        dl->AddRectFilled(barMin, p_max, IM_COL32(58, 60, 66, 255), 10.0f, ImDrawFlags_RoundCornersBottom);
        std::string numLbl = std::to_string(i + 1);
        ImVec2 numSz = ImGui::CalcTextSize(numLbl.c_str());
        dl->AddText({ p_min.x + 8.0f, barMin.y + (barH - numSz.y) * 0.5f }, IM_COL32(200, 200, 205, 220), numLbl.c_str());

        ImGui::EndChild();
        ImGui::PopID();

        float nextX2 = ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + cardSize.x;
        if (i + 1 < slides.size() && nextX2 < windowVisibleX2)
            ImGui::SameLine();
    }

    ImGui::EndChild();
}

// =============================================================================
//  Render
// =============================================================================
bool SongEditView::Render()
{
    bool wantsBack = false;
    RenderTopBar(wantsBack);

    float availW = ImGui::GetContentRegionAvail().x;
    float leftW  = availW * 0.45f;
    float rightW = std::max(10.0f, availW - leftW - 8.0f);

    RenderLeftPane(leftW);
    ImGui::SameLine();
    RenderRightPane(rightW);

    constexpr double kAutosaveDebounceSeconds = 1.2;
    if (m_Dirty && (ImGui::GetTime() - m_LastEditTime) > kAutosaveDebounceSeconds)
        FlushIfDirty();

    if (wantsBack) {
        FlushIfDirty();
        return false;
    }
    return true;
}

} // namespace ProyecThor::UI
