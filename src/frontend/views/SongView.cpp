#include "SongView.h"
#include "backend/core/PresentationCore.h"
#include "UIStrings.h"
#include "LibrarySongs.h"
#include "SongBackgroundPicker.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <cstdlib>
#include "frontend/ui/bin/StyleGeneralApp.h"
#include "frontend/ui/SongPlayStats.h"
#include "frontend/ui/DesignSystem.h"

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor
//
//  El editor de letra/autor con buffers fijos (m_EditBuffer/m_AuthorBuffer),
//  la migracion de sidecars viejos ".autor.txt" y el editor popup flotante
//  (RenderEditorModal/EditorFocusCallback/OpenEditorForSong/SaveBufferToFile)
//  se retiraron con el rework del editor: ahora viven, respectivamente, en
//  SongEditView (buffers std::string dinamicos) y en SongEditView::Open
//  (migracion de ".autor.txt", movida ahi para no perder ese comportamiento).
// ─────────────────────────────────────────────────────────────────────────────
SongView::SongView()
    : m_CurrentSongTitle("")
    , m_ActiveStanzaIndex(-1)
    , m_HasRecordedCurrentSongProjection(false)
{
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderSettingsCard — primera tarjeta del grid, estilo "Intro" de
//  ProPresenter: en vez de un estilo por defecto compartido por TODA la
//  categoria Canciones, cada cancion guarda su propio estilo/fondo preferido
//  aca. Se aplica automaticamente al seleccionar la cancion (ver
//  ApplyDefaultStyleIfSet en LibrarySongs.cpp), y el operador sigue pudiendo
//  cambiarlo a mano en cualquier momento desde el selector de estilos.
// ─────────────────────────────────────────────────────────────────────────────
void SongView::RenderSettingsCard(const std::string& songFilename, ImVec2 p_min, ImVec2 p_max, bool isHovered)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 cardSize = { p_max.x - p_min.x, p_max.y - p_min.y };
    float barH = std::clamp(cardSize.y * 0.20f, 16.0f, 26.0f);

    dl->AddRectFilled(p_min, p_max, IM_COL32(54, 48, 30, 255), 10.0f);
    if (isHovered)
        dl->AddRectFilled(p_min, p_max, IM_COL32(255, 255, 255, 14), 10.0f);
    dl->AddRect(p_min, p_max, IM_COL32(255, 255, 255, 24), 10.0f, 0, 1.0f);

    std::string style = ProyecThor::Library::GetSongStyle(songFilename);
    ProyecThor::Library::SongBackground bg = ProyecThor::Library::GetSongBackground(songFilename);

    ImVec2 contentMax = { p_max.x, p_max.y - barH };
    dl->PushClipRect(p_min, contentMax, true);

    const char* title = "Ajustes";
    ImVec2 titleSz = ImGui::CalcTextSize(title);
    float  midY    = p_min.y + (cardSize.y - barH) * 0.5f;

    std::string styleLine = style.empty() ? "Estilo: (ninguno)" : ("Estilo: " + style);
    std::string bgLine    = bg.path.empty() ? "Fondo: (ninguno)" : ("Fondo: " + std::filesystem::path(bg.path).filename().string());
    ImVec2 s1 = ImGui::CalcTextSize(styleLine.c_str());
    ImVec2 s2 = ImGui::CalcTextSize(bgLine.c_str());

    float blockH = titleSz.y + 6.0f + s1.y + 2.0f + s2.y;
    float y0 = midY - blockH * 0.5f;

    dl->AddText({ p_min.x + (cardSize.x - titleSz.x) * 0.5f, y0 }, IM_COL32(232, 226, 198, 255), title);
    dl->AddText({ p_min.x + (cardSize.x - s1.x) * 0.5f, y0 + titleSz.y + 6.0f }, IM_COL32(200, 195, 170, 190), styleLine.c_str());
    dl->AddText({ p_min.x + (cardSize.x - s2.x) * 0.5f, y0 + titleSz.y + 6.0f + s1.y + 2.0f }, IM_COL32(200, 195, 170, 190), bgLine.c_str());

    dl->PopClipRect();

    ImVec2 barMin = { p_min.x, p_max.y - barH };
    dl->AddRectFilled(barMin, p_max, IM_COL32(168, 148, 44, 255), 10.0f, ImDrawFlags_RoundCornersBottom);
    const char* barLabel = "Inicio";
    ImVec2 barLabelSz = ImGui::CalcTextSize(barLabel);
    dl->AddText({ p_min.x + 8.0f, barMin.y + (barH - barLabelSz.y) * 0.5f }, IM_COL32(32, 27, 10, 255), barLabel);
}

// ─────────────────────────────────────────────────────────────────────────────
//  SongBgEntry/ListSongBackgrounds ahora viven en SongBackgroundPicker.h/.cpp
//  (extraidos de aca) para que SongEditView tambien pueda ofrecer el mismo
//  picker de fondos, pero por LINEA en vez de por cancion completa.
// ─────────────────────────────────────────────────────────────────────────────
//  RenderSongSettingsPopup — contenido del popup que abre la tarjeta de
//  ajustes: elegir estilo (de los guardados) y fondo (de la biblioteca de
//  Fondos) para ESTA cancion en particular.
// ─────────────────────────────────────────────────────────────────────────────
void SongView::RenderSongSettingsPopup(const std::string& songFilename)
{
    if (m_OpenSongSettingsRequest) {
        ImGui::OpenPopup("songSettingsPopup");
        m_OpenSongSettingsRequest = false;
    }

    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.07f, 0.07f, 0.08f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(1.0f, 1.0f, 1.0f, 0.14f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, DS::RadiusLarge);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));

    if (ImGui::BeginPopup("songSettingsPopup"))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
        ImGui::TextUnformatted("Preset de esta cancion");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        // ── Estilo ────────────────────────────────────────────────────────
        std::string currentStyle = ProyecThor::Library::GetSongStyle(songFilename);
        std::vector<std::string> names = Core::PresentationCore::Get().GetSavedStyleNames();

        ImGui::TextUnformatted("Estilo:");
        ImGui::SetNextItemWidth(240.0f);
        const char* preview = currentStyle.empty() ? "(ninguno)" : currentStyle.c_str();
        if (ImGui::BeginCombo("##songStyleCombo", preview))
        {
            if (ImGui::Selectable("(ninguno)", currentStyle.empty()))
                ProyecThor::Library::SetSongStyle(songFilename, "");
            for (const auto& name : names)
            {
                bool sel = (name == currentStyle);
                if (ImGui::Selectable(name.c_str(), sel))
                    ProyecThor::Library::SetSongStyle(songFilename, name);
            }
            ImGui::EndCombo();
        }

        ImGui::Spacing();
        ImGui::Spacing();

        // ── Fondo ─────────────────────────────────────────────────────────
        // Se elige de la misma biblioteca que el tab "Fondos" (assets/backgrounds),
        // no de un archivo cualquiera del disco — mismo espiritu que el combo
        // de Estilo de arriba.
        ProyecThor::Library::SongBackground bg = ProyecThor::Library::GetSongBackground(songFilename);
        std::vector<SongBgEntry> bgEntries = ListSongBackgrounds();

        ImGui::TextUnformatted("Fondo:");
        ImGui::SetNextItemWidth(240.0f);
        std::string bgPreview = bg.path.empty() ? "(ninguno)" : std::filesystem::path(bg.path).filename().string();
        if (ImGui::BeginCombo("##songBgCombo", bgPreview.c_str()))
        {
            if (ImGui::Selectable("(ninguno)", bg.path.empty()))
                ProyecThor::Library::ClearSongBackground(songFilename);
            for (const auto& entry : bgEntries)
            {
                bool sel = (entry.fullPath == bg.path);
                if (ImGui::Selectable(entry.label.c_str(), sel))
                    ProyecThor::Library::SetSongBackground(songFilename, entry.fullPath, !entry.isImage);
            }
            if (bgEntries.empty())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
                ImGui::TextWrapped("Sin fondos en la biblioteca (agregalos desde la pestaña Fondos).");
                ImGui::PopStyleColor();
            }
            ImGui::EndCombo();
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderStanzaColorBar — barra de color inferior de cada tarjeta de
//  estrofa: numero de estrofa + swatch clickeable para etiquetar con color
//  (agrupacion visual libre, ver LibrarySongs::SetStanzaColor).
// ─────────────────────────────────────────────────────────────────────────────
void SongView::RenderStanzaColorBar(const std::string& songFilename, int stanzaIndex, ImVec2 p_min, ImVec2 p_max, float barH)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 barMin = { p_min.x, p_max.y - barH };

    unsigned int colU32 = ProyecThor::Library::GetStanzaColor(songFilename, stanzaIndex);
    ImU32 barCol = colU32 != 0u ? (ImU32)colU32 : IM_COL32(58, 60, 66, 255);

    dl->AddRectFilled(barMin, p_max, barCol, 10.0f, ImDrawFlags_RoundCornersBottom);

    char numBuf[8];
    snprintf(numBuf, sizeof(numBuf), "%d", stanzaIndex + 1);
    ImVec2 numSz = ImGui::CalcTextSize(numBuf);
    ImU32  numCol = colU32 != 0u ? IM_COL32(20, 20, 22, 235) : IM_COL32(200, 200, 205, 220);
    dl->AddText({ p_min.x + 8.0f, barMin.y + (barH - numSz.y) * 0.5f }, numCol, numBuf);

    float swatchSize = std::max(10.0f, barH * 0.55f);
    ImVec2 swMin = { p_max.x - swatchSize - 6.0f, barMin.y + (barH - swatchSize) * 0.5f };
    ImVec2 swMax = { swMin.x + swatchSize, swMin.y + swatchSize };

    ImGui::SetCursorScreenPos(swMin);
    ImGui::PushID(stanzaIndex);
    bool swClicked = ImGui::InvisibleButton("##colorSwatch", { swatchSize, swatchSize });
    ImGui::PopID();

    dl->AddRectFilled(swMin, swMax, colU32 != 0u ? barCol : IM_COL32(255, 255, 255, 55), 3.0f);
    dl->AddRect(swMin, swMax, IM_COL32(0, 0, 0, 130), 3.0f, 0, 1.0f);

    if (swClicked) {
        m_ColorPickerForStanza   = stanzaIndex;
        m_OpenColorPickerRequest = true;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Render — punto de entrada. Hace el swap in-place Browse<->Edit: nunca
//  ambos a la vez, y nunca en una ventana flotante (ver comentario de la
//  clase en SongView.h). El editor unificado (m_EditView) devuelve false
//  cuando el usuario aprieta "Volver", momento en el que tambien ya hizo
//  flush de cualquier autoguardado pendiente.
// ─────────────────────────────────────────────────────────────────────────────
void SongView::Render()
{
    auto& core      = Core::PresentationCore::Get();
    auto  selection = core.PeekSelection();

    if (selection.title.empty() || selection.type != Core::ItemType::Song)
        return;

    if (m_CurrentSongTitle != selection.title)
    {
        m_CurrentSongTitle  = selection.title;
        m_ActiveStanzaIndex = -1;
        m_HasRecordedCurrentSongProjection = false;
    }

    // Cue "consumir una vez" de PresentationCore: una cancion recien creada
    // (ver Library::CreateNewSong) pide entrar directo al editor unificado,
    // sin popup, apenas la seleccion actual coincide con el archivo nuevo.
    {
        std::string pendingOpenFile;
        if (core.ConsumeSongEditorOpenRequest(pendingOpenFile) && pendingOpenFile == selection.title)
        {
            m_EditView.Open(pendingOpenFile);
            m_ShowEditor = true;
        }
    }

    if (m_ShowEditor)
    {
        if (!m_EditView.Render())
            m_ShowEditor = false;
        return;
    }

    RenderBrowseGrid();
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderBrowseGrid — grilla de estrofas (antes era el cuerpo entero de
//  Render(), separada para poder alternarla con m_EditView.Render()).
// ─────────────────────────────────────────────────────────────────────────────
void SongView::RenderBrowseGrid()
{
    auto& core      = Core::PresentationCore::Get();
    auto  selection = core.PeekSelection();

    // Estilo de preview fijo (no el estilo que el usuario eligio para la
    // proyeccion real): los estilos de proyeccion estan pensados para una
    // pantalla completa a 1920px y a tamano de tarjeta quedaban ilegibles o
    // con colores/alineacion que no funcionan en un recuadro chico. El
    // preview siempre centra, usa texto casi blanco y una fuente fija,
    // priorizando legibilidad sobre fidelidad 1:1 con la proyeccion.
    ImFont* previewFont = ImGui::GetFont();
    const ImU32 textColor = IM_COL32(235, 235, 238, 255);

    auto TryRecordProjection = [&](bool userInitiated) {
        if (!userInitiated) return;
        if (selection.title.empty() || selection.contentData.size() < 2) return;
        if (m_ActiveStanzaIndex < 0) return;
        if (m_HasRecordedCurrentSongProjection) return;
        ProyecThor::UI::RecordSongProjection(selection.title, (int)selection.contentData.size());
        m_HasRecordedCurrentSongProjection = true;
    };

    // Empuja al Stage Display la estrofa que viene despues de idx (o vacio si
    // es la ultima). Nunca se muestra al publico, solo en el Stage.
    auto PushNextStanzaText = [&](int idx) {
        int nextIdx = idx + 1;
        core.SetNextText(nextIdx < (int)selection.contentData.size()
                          ? selection.contentData[nextIdx] : "");
    };

    // ── Navegacion con teclado ────────────────────────────────────────────────
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        !selection.contentData.empty())
    {
       if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
{
    if (m_ActiveStanzaIndex < (int)selection.contentData.size() - 1)
    {
        m_ActiveStanzaIndex++;
        core.SetLayer2_Text(selection.contentData[m_ActiveStanzaIndex]);
        PushNextStanzaText(m_ActiveStanzaIndex);
        if (core.IsProjecting())
            core.SetProjecting(true);

        TryRecordProjection(true);
    }
}
if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
{
    if (m_ActiveStanzaIndex > 0)
    {
        m_ActiveStanzaIndex--;
        core.SetLayer2_Text(selection.contentData[m_ActiveStanzaIndex]);
        PushNextStanzaText(m_ActiveStanzaIndex);
        if (core.IsProjecting())
            core.SetProjecting(true);

        TryRecordProjection(true);
    }
}
    }

    // ── Barra superior: solo el slider de tamano + el nombre del archivo ────
    // Antes tenia un titulo (con un bug de idiomas que le hacia mostrar texto
    // de la Biblia) y un boton "Limpiar pantalla" redundante con el que ya
    // existe en el panel Control. Se sacan los dos: el slider queda como
    // unico control, arriba, simple. Estilo "HTML": track fino + thumb
    // circular animado (DS::ModernSlider) en vez del slider "pelado"/grueso
    // de ImGui por defecto; el label visible se cambia por un tooltip,
    // mismo patron que LPZoomSlider en Fondos/Overlays/Estilos.
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Tamano");
    ImGui::SameLine();
    DS::ModernSlider("##stanzaZoom", &m_StanzaCardZoom, 0.55f, 1.8f, 140.0f);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        ImGui::SetTooltip("Tamano de las tarjetas");

    // Editar ya no requiere click derecho sobre una estrofa: un boton fijo
    // al lado del slider abre el editor completo de la cancion.
    ImGui::SameLine();
    if (DS::GlassButton("Editar", { 90.f, DS::ButtonHeight }, DS::TextSecondary))
    {
        m_EditView.Open(selection.title);
        m_ShowEditor = true;
    }

    ImGui::SameLine();
    float titleMaxW = std::max(20.0f, ImGui::GetContentRegionAvail().x - 8.0f);
    std::string titleTrunc = selection.title;
    if (ImGui::CalcTextSize(titleTrunc.c_str()).x > titleMaxW) {
        while (!titleTrunc.empty() && ImGui::CalcTextSize((titleTrunc + "...").c_str()).x > titleMaxW)
            titleTrunc.pop_back();
        titleTrunc += "...";
    }
    float rightX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(titleTrunc.c_str()).x;
    if (rightX > ImGui::GetCursorPosX())
        ImGui::SetCursorPosX(rightX);
    ImGui::TextDisabled("%s", titleTrunc.c_str());

    ImGui::Spacing();

    // ── Grid de estrofas ──────────────────────────────────────────────────────
    float availWidth = ImGui::GetContentRegionAvail().x;
    float colWidth   = 250.0f * m_StanzaCardZoom;
    float cardHeight = 110.0f * m_StanzaCardZoom;
    int   columns    = std::max(1, static_cast<int>(availWidth / colWidth));

    auto splitLines = [](const std::string& stanza) {
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
    };

    auto measureLines = [&](const std::vector<std::string>& lines, float size, float& outW, float& outH) {
        outW = 0.0f;
        for (const auto& ln : lines) {
            if (ln.empty()) continue;
            ImVec2 sz = previewFont->CalcTextSizeA(size, FLT_MAX, 0.0f, ln.c_str());
            outW = std::max(outW, sz.x);
        }
        outH = lines.size() * size;
    };

    // Barra de color/etiqueta inferior (estilo ProPresenter): resta espacio
    // util a las tarjetas de estrofa, hay que contemplarla en el calculo del
    // tamano uniforme de fuente para que el texto no quede pegado a la barra.
    float barH = std::clamp(cardHeight * 0.20f, 16.0f, 26.0f);

    // ── Tamano de fuente UNICO para todas las tarjetas ───────────────────────
    // Antes cada tarjeta buscaba su propio maximo que entra, asi que
    // estrofas cortas quedaban enormes al lado de estrofas largas chicas —
    // se veia desprolijo. Ahora se busca, para cada estrofa, el tamano
    // maximo que le entra, y se usa el MENOR de todos esos tamanos para
    // TODAS las tarjetas: siguen siendo lo mas grandes posible, pero todas
    // iguales.
    float uniformSize = 96.0f;
    {
        float cardW_est  = std::max(10.0f, availWidth / (float)columns - 4.0f);
        float pad_est    = std::clamp(std::min(cardW_est, cardHeight) * 0.10f, 6.0f, 18.0f);
        float safeW_est  = std::max(10.0f, cardW_est  - pad_est * 2.0f);
        float safeH_est  = std::max(10.0f, cardHeight - barH - pad_est * 2.0f);

        for (const auto& stanza : selection.contentData) {
            std::vector<std::string> lines = splitLines(stanza);
            float size = 10.0f;
            while (size < 96.0f) {
                float next = size + 1.0f, w, h;
                measureLines(lines, next, w, h);
                if (w > safeW_est || h > safeH_est) break;
                size = next;
            }
            uniformSize = std::min(uniformSize, size);
        }
    }

    if (ImGui::BeginTable("StanzasGrid", columns, ImGuiTableFlags_SizingStretchSame))
    {
        // ── Tarjeta 0: ajustes de la cancion (estilo "Intro" de ProPresenter,
        //    ver RenderSettingsCard) — reemplaza el estilo por defecto de
        //    categoria: cada cancion guarda su propio preset aca. ─────────────
        {
            ImGui::TableNextColumn();
            ImGui::PushID("settingsCard");

            ImVec2 p_min    = ImGui::GetCursorScreenPos();
            ImVec2 cardSize = ImVec2(ImGui::GetContentRegionAvail().x, cardHeight);
            ImVec2 p_max    = ImVec2(p_min.x + cardSize.x, p_min.y + cardSize.y);

            if (ImGui::InvisibleButton("##settings_btn", cardSize))
                m_OpenSongSettingsRequest = true;
            bool settingsHovered = ImGui::IsItemHovered();

            RenderSettingsCard(selection.title, p_min, p_max, settingsHovered);

            ImGui::PopID();
        }

        for (size_t i = 0; i < selection.contentData.size(); ++i)
        {
            ImGui::TableNextColumn();

            const std::string& stanza     = selection.contentData[i];
            bool               isSelected = (m_ActiveStanzaIndex == static_cast<int>(i));

            ImGui::PushID((int)i);

            ImVec2 p_min    = ImGui::GetCursorScreenPos();
            ImVec2 cardSize = ImVec2(ImGui::GetContentRegionAvail().x, cardHeight);
            ImVec2 p_max    = ImVec2(p_min.x + cardSize.x, p_min.y + cardSize.y);

            if (ImGui::InvisibleButton("##select_btn", cardSize))
            {
                m_ActiveStanzaIndex = (int)i;
                core.SetLayer2_Text(stanza);
                PushNextStanzaText(m_ActiveStanzaIndex);
                if (core.IsProjecting())
                    core.SetProjecting(true);
                TryRecordProjection(true);
            }

            bool isHovered = ImGui::IsItemHovered();

            ImDrawList* drawList = ImGui::GetWindowDrawList();
// ── Fondo tipo ProPresenter: PNG oscuro si hay textura, si no
            //    un degradado procedural que imita el mismo look ──────────
            auto bgIt = StyleGeneralApp::Icons.find("song_card_bg");
            bool hasBgTexture = (bgIt != StyleGeneralApp::Icons.end() && bgIt->second.textureID != nullptr);

            if (hasBgTexture)
            {
                ImU32 tint = isSelected ? IM_COL32(255,255,255,255) : IM_COL32(205,205,205,255);
                drawList->AddImageRounded(bgIt->second.textureID, p_min, p_max,
                                          ImVec2(0,0), ImVec2(1,1), tint, 10.0f);
            }
            else
            {
                // Plano: gris solido tipo ProPresenter/OBS en vez del
                // degradado procedural anterior (quedaba mal en varios
                // estilos y no era coherente con el resto de la UI).
                drawList->AddRectFilled(p_min, p_max, IM_COL32(42, 43, 48, 255), 10.0f);
            }

            if (isSelected)
                drawList->AddRectFilled(p_min, p_max, IM_COL32(120, 120, 130, 55), 10.0f);
            else if (isHovered)
                drawList->AddRectFilled(p_min, p_max, IM_COL32(255, 255, 255, 12), 10.0f);

            ImU32 borderColor = isSelected
                ? IM_COL32(180, 182, 190, 200)
                : IM_COL32(255, 255, 255, 22);
            float borderSize = isSelected ? 1.5f : 1.0f;
           drawList->AddRect(p_min, p_max, borderColor, 10.0f, 0, borderSize);

            // El area de texto queda por ENCIMA de la barra de color inferior.
            ImVec2 textAreaMax = { p_max.x, p_max.y - barH };
            drawList->PushClipRect(p_min, textAreaMax, true);

// ── Padding fijo del preview (no el margen del estilo elegido): un
//    padding modesto y proporcional a la tarjeta, igual para las 4
//    canciones sin importar el estilo activo ───────────────────────────
float pad  = std::clamp(std::min(cardSize.x, cardSize.y) * 0.10f, 6.0f, 18.0f);
float padL = pad, padT = pad, padR = pad, padB = pad;
float textAreaH = cardSize.y - barH;

// ── Lineas de esta estrofa + el tamano UNICO calculado arriba (no un
//    maximo por tarjeta: ver uniformSize) ───────────────────────────────
std::vector<std::string> lines = splitLines(stanza);
float displaySize = uniformSize;

float blockW, blockH;
measureLines(lines, displaySize, blockW, blockH);

float startY = std::max(padT, (textAreaH - blockH) * 0.5f);

float currentY = startY;
for (const auto& line : lines)
{
    if (!line.empty())
    {
        ImVec2 lineSz = previewFont->CalcTextSizeA(displaySize, FLT_MAX, 0.0f, line.c_str());
        float localX = std::max(padL, (cardSize.x - lineSz.x) * 0.5f);

        drawList->AddText(previewFont, displaySize,
                          ImVec2(p_min.x + localX, p_min.y + currentY),
                          textColor, line.c_str());
    }
    currentY += displaySize;
}

drawList->PopClipRect();

RenderStanzaColorBar(selection.title, (int)i, p_min, p_max, barH);

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    // ── Popups disparados desde las tarjetas de arriba ───────────────────────
    RenderSongSettingsPopup(selection.title);

    if (m_OpenColorPickerRequest) {
        ImGui::OpenPopup("stanzaColorPopup");
        m_OpenColorPickerRequest = false;
    }
    if (ImGui::BeginPopup("stanzaColorPopup"))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
        ImGui::TextUnformatted("Color de la tarjeta");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        static const ImU32 kPalette[] = {
            IM_COL32(214, 84, 84, 255),   // rojo
            IM_COL32(214, 140, 64, 255),  // naranja
            IM_COL32(214, 190, 64, 255),  // amarillo/olive
            IM_COL32(96, 190, 110, 255),  // verde
            IM_COL32(74, 160, 214, 255),  // celeste
            IM_COL32(120, 110, 214, 255), // violeta
            IM_COL32(214, 90, 160, 255),  // rosa
            IM_COL32(150, 150, 158, 255), // gris neutro
        };

        for (int p = 0; p < (int)(sizeof(kPalette) / sizeof(kPalette[0])); ++p)
        {
            if (p % 4 != 0) ImGui::SameLine();
            ImGui::PushID(p);
            ImGui::PushStyleColor(ImGuiCol_Button,        ImGui::ColorConvertU32ToFloat4(kPalette[p]));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(kPalette[p]));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImGui::ColorConvertU32ToFloat4(kPalette[p]));
            if (ImGui::Button("##swatch", { 28.f, 28.f }))
            {
                ProyecThor::Library::SetStanzaColor(selection.title, m_ColorPickerForStanza, kPalette[p]);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(3);
            ImGui::PopID();
        }

        ImGui::Spacing();
        if (DS::GlassButton("Quitar color", { 130.f, DS::ButtonHeight }, DS::TextSecondary))
        {
            ProyecThor::Library::SetStanzaColor(selection.title, m_ColorPickerForStanza, 0u);
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

} // namespace ProyecThor::UI