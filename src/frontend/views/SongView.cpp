#include "SongView.h"
#include "backend/core/PresentationCore.h"
#include "UIStrings.h"
#include "LibrarySongs.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <cstdlib>
#include "frontend/ui/bin/StyleGeneralApp.h"
#include "frontend/ui/SongPlayStats.h"

// Windows headers para SHGetKnownFolderPath
#ifdef _WIN32
// Windows headers para SHGetKnownFolderPath
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#endif
#ifdef _WIN32
#include <winerror.h>
#endif

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  Helper: devuelve %APPDATA%\ProyecThor\assets\songs como std::filesystem::path
//  Usa SHGetKnownFolderPath (no requiere admin, funciona con rutas Unicode).
// ─────────────────────────────────────────────────────────────────────────────
static std::filesystem::path GetSongsDirectory()
{
    std::filesystem::path result;

#ifdef _WIN32
    PWSTR pszPath = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE, nullptr, &pszPath);

    if (SUCCEEDED(hr) && pszPath)
    {
        result = std::filesystem::path(pszPath) / L"ProyecThor" / L"assets" / L"songs";
        CoTaskMemFree(pszPath);
    }
    else
    {
        const char* appdata = std::getenv("APPDATA");
        if (appdata)
            result = std::filesystem::path(appdata) / "ProyecThor" / "assets" / "songs";
        else
            result = std::filesystem::current_path() / "ProyecThor" / "assets" / "songs";
    }
#else
    const char* home = std::getenv("HOME");
    if (home)
        result = std::filesystem::path(home) / ".local" / "share" / "ProyecThor" / "assets" / "songs";
    else
        result = std::filesystem::current_path() / "ProyecThor" / "assets" / "songs";
#endif

    std::error_code ec;
    std::filesystem::create_directories(result, ec);

    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helper: ruta del antiguo archivo sidecar de autor (nombre + ".autor.txt").
//  Ya no se usa para guardar, solo se conserva para migrar y borrar los
//  sidecars viejos que quedaron creados por versiones anteriores, evitando
//  que sigan apareciendo como canciones fantasma en el listado.
// ─────────────────────────────────────────────────────────────────────────────
static std::filesystem::path GetLegacyAuthorFilePath(const std::filesystem::path& songFile)
{
    std::filesystem::path authorFile = songFile;
    authorFile.replace_extension(".autor.txt");
    return authorFile;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor
// ─────────────────────────────────────────────────────────────────────────────
SongView::SongView()
    : m_CurrentSongTitle("")
    , m_ActiveStanzaIndex(-1)
    , m_HasRecordedCurrentSongProjection(false)
    , m_ShowEditor(false)
    , m_OpenEditorPopup(false)
    , m_EditingFilePath("")
    , m_SaveSuccess(false)
    , m_FocusStanzaPending(false)
    , m_FocusStanzaCharStart(0)
    , m_FocusStanzaCharEnd(0)
{
    std::memset(m_EditBuffer, 0, sizeof(m_EditBuffer));
    std::memset(m_AuthorBuffer, 0, sizeof(m_AuthorBuffer));
}

// ─────────────────────────────────────────────────────────────────────────────
//  SaveBufferToFile
//  Garantiza que el directorio padre exista antes de escribir.
// ─────────────────────────────────────────────────────────────────────────────
bool SongView::SaveBufferToFile()
{
    if (m_EditingFilePath.empty())
    {
        printf("[SongView] SaveBufferToFile: ruta vacia, abortando.\n");
        return false;
    }

    std::filesystem::path filePath(m_EditingFilePath);

    std::error_code ec;
    std::filesystem::create_directories(filePath.parent_path(), ec);
    if (ec)
    {
        printf("[SongView] Error creando directorios: %s\n", ec.message().c_str());
        return false;
    }

    std::ofstream file(filePath, std::ios::out | std::ios::trunc);
    if (!file.is_open())
    {
        printf("[SongView] No se pudo abrir para escritura: %s\n", m_EditingFilePath.c_str());
        return false;
    }

    file << m_EditBuffer;
    bool ok = file.good();
    file.close();

    if (ok)
        printf("[SongView] Guardado correctamente: %s\n", m_EditingFilePath.c_str());
    else
        printf("[SongView] Error al escribir en: %s\n", m_EditingFilePath.c_str());

    return ok;
}

// ─────────────────────────────────────────────────────────────────────────────
//  OpenEditorForSong
//  Centraliza la carga del archivo de letra y del autor para el editor.
//  El autor se lee desde songs_authors.ini (mismo origen que usa la
//  biblioteca), no desde un sidecar propio. Si detecta un sidecar viejo
//  (".autor.txt") de una version anterior, migra ese valor al .ini y borra
//  el sidecar para que deje de aparecer como cancion fantasma en el listado.
//  Si focusStanza es true, busca el texto de esa estrofa dentro del
//  contenido cargado y deja marcado el rango de caracteres para que
//  RenderEditorModal seleccione ese fragmento apenas se abra el editor.
// ─────────────────────────────────────────────────────────────────────────────
void SongView::OpenEditorForSong(const std::string& songTitle, const std::string& stanzaText, bool focusStanza)
{
    std::filesystem::path songsDir = GetSongsDirectory();

    std::filesystem::path songFile = songsDir / songTitle;
    if (songFile.extension() != ".txt")
        songFile.replace_extension(".txt");

    printf("[SongView] Intentando abrir: %s\n", songFile.string().c_str());

    if (!std::filesystem::exists(songFile))
    {
        std::error_code ec;
        std::filesystem::create_directories(songFile.parent_path(), ec);
        std::ofstream touch(songFile);
        touch.close();
        printf("[SongView] Archivo creado (era nuevo): %s\n", songFile.string().c_str());
    }

    std::ifstream file(songFile);
    if (!file.is_open())
    {
        printf("[SongView] ERROR: no se pudo abrir el archivo: %s\n", songFile.string().c_str());
        return;
    }

    std::string content(
        (std::istreambuf_iterator<char>(file)),
         std::istreambuf_iterator<char>()
    );
    file.close();

    content.erase(std::remove(content.begin(), content.end(), '\r'), content.end());

    strncpy(m_EditBuffer, content.c_str(), sizeof(m_EditBuffer) - 1);
    m_EditBuffer[sizeof(m_EditBuffer) - 1] = '\0';

    m_EditingFilePath = songFile.string();

    std::string authorContent = ProyecThor::Library::GetSongAuthor(songFile.filename().string());

    if (authorContent.empty())
    {
        std::filesystem::path legacyAuthorFile = GetLegacyAuthorFilePath(songFile);
        std::ifstream legacyStream(legacyAuthorFile);
        if (legacyStream.is_open())
        {
            std::string legacyContent(
                (std::istreambuf_iterator<char>(legacyStream)),
                 std::istreambuf_iterator<char>()
            );
            legacyStream.close();

            legacyContent.erase(std::remove(legacyContent.begin(), legacyContent.end(), '\r'), legacyContent.end());
            legacyContent.erase(std::remove(legacyContent.begin(), legacyContent.end(), '\n'), legacyContent.end());

            if (!legacyContent.empty())
            {
                authorContent = legacyContent;
                ProyecThor::Library::SetSongAuthor(songFile.filename().string(), authorContent);
            }

            std::error_code ec;
            std::filesystem::remove(legacyAuthorFile, ec);
        }
    }

    strncpy(m_AuthorBuffer, authorContent.c_str(), sizeof(m_AuthorBuffer) - 1);
    m_AuthorBuffer[sizeof(m_AuthorBuffer) - 1] = '\0';

    m_FocusStanzaPending   = false;
    m_FocusStanzaCharStart = 0;
    m_FocusStanzaCharEnd   = 0;

    if (focusStanza && !stanzaText.empty())
    {
        std::string normalizedStanza = stanzaText;
        normalizedStanza.erase(std::remove(normalizedStanza.begin(), normalizedStanza.end(), '\r'), normalizedStanza.end());

        size_t pos = std::string(m_EditBuffer).find(normalizedStanza);
        if (pos != std::string::npos)
        {
            m_FocusStanzaPending   = true;
            m_FocusStanzaCharStart = (int)pos;
            m_FocusStanzaCharEnd   = (int)(pos + normalizedStanza.length());
        }
    }

    m_SaveSuccess = false;
    m_ShowEditor  = true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  EditorFocusCallback
//  Callback de ImGui invocado en cada frame mientras el InputTextMultiline
//  esta activo. Si hay una estrofa pendiente de foco, fija cursor y
//  seleccion sobre el rango calculado en OpenEditorForSong y consume el
//  flag para no repetirlo en frames posteriores.
// ─────────────────────────────────────────────────────────────────────────────
int SongView::EditorFocusCallback(ImGuiInputTextCallbackData* data)
{
    SongView* self = static_cast<SongView*>(data->UserData);
    if (self && self->m_FocusStanzaPending)
    {
        data->CursorPos      = self->m_FocusStanzaCharEnd;
        data->SelectionStart = self->m_FocusStanzaCharStart;
        data->SelectionEnd   = self->m_FocusStanzaCharEnd;
        self->m_FocusStanzaPending = false;
    }
    return 0;
}

// =============================================================================
//  RenderEditorModal
// =============================================================================
void SongView::RenderEditorModal()
{
    const auto& str = ProyecThor::UI::GetUIStrings();

    if (!m_ShowEditor)
    {
        m_SaveSuccess = false;
        return;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, { 0.5f, 0.5f });
    ImGui::SetNextWindowSize({ 660.f, 600.f }, ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints({ 420.f, 360.f }, { FLT_MAX, FLT_MAX });

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    { 22.0f, 20.0f });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,  1.0f);

    ImGui::PushStyleColor(ImGuiCol_WindowBg,      { 0.039f, 0.043f, 0.063f, 0.980f });
    ImGui::PushStyleColor(ImGuiCol_Border,         { 1.000f, 1.000f, 1.000f, 0.080f });
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive,  { 0.028f, 0.031f, 0.047f, 1.000f });

    bool windowOpen = true;
    ImGui::Begin("Editor de Cancion##songWin",
                 &windowOpen,
                 ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoCollapse);

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(3);

    if (!windowOpen)
    {
        m_ShowEditor  = false;
        m_SaveSuccess = false;
        ImGui::End();
        return;
    }

    // ── Cabecera ──────────────────────────────────────────────────────────────
    std::filesystem::path fp(m_EditingFilePath);
    std::string fileName = fp.filename().string();

    ImGui::PushStyleColor(ImGuiCol_Text, { 1.0f, 1.0f, 1.0f, 0.40f });
    ImGui::TextUnformatted("Editando:");
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::TextUnformatted(fileName.c_str());

    ImGui::Spacing();

    // ── Campo de autor ────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, { 1.0f, 1.0f, 1.0f, 0.40f });
    ImGui::TextUnformatted("Autor:");
    ImGui::PopStyleColor();

    ImGui::SameLine();

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        { 1.0f, 1.0f, 1.0f, 0.040f });
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, { 1.0f, 1.0f, 1.0f, 0.070f });
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  { 1.0f, 1.0f, 1.0f, 0.100f });
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##authorInput", "Nombre del autor / compositor", m_AuthorBuffer, sizeof(m_AuthorBuffer));
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, { 1.0f, 1.0f, 1.0f, 0.060f });
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // ── Area de texto adaptativa ──────────────────────────────────────────────
    constexpr float k_ButtonAreaHeight = 74.0f;
    ImVec2 availSize = ImGui::GetContentRegionAvail();
    ImVec2 inputSize = { -1.0f, availSize.y - k_ButtonAreaHeight };

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  { 12.0f, 10.0f });
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        { 1.0f, 1.0f, 1.0f, 0.040f });
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, { 1.0f, 1.0f, 1.0f, 0.070f });
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  { 1.0f, 1.0f, 1.0f, 0.100f });

    if (m_FocusStanzaPending)
        ImGui::SetKeyboardFocusHere();

    ImGui::InputTextMultiline(
        "##editBuffer",
        m_EditBuffer,
        sizeof(m_EditBuffer),
        inputSize,
        ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackAlways,
        &SongView::EditorFocusCallback,
        this
    );

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);

    ImGui::Spacing();

    // ── Contador de caracteres ───────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, { 1.0f, 1.0f, 1.0f, 0.30f });
    ImGui::Text("%d caracteres", (int)strlen(m_EditBuffer));
    ImGui::PopStyleColor();

    if (m_SaveSuccess)
    {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, { 0.20f, 0.85f, 0.40f, 1.0f });
        ImGui::TextUnformatted("  Guardado correctamente");
        ImGui::PopStyleColor();
    }

    // ── Fila de botones ───────────────────────────────────────────────────────
    ImVec2 buttonSize = { 130.f, 36.f };

    float rightAlign = ImGui::GetWindowWidth()
                     - (buttonSize.x * 2.0f)
                     - ImGui::GetStyle().ItemSpacing.x
                     - 22.0f;

    if (rightAlign > ImGui::GetCursorPosX())
        ImGui::SameLine(rightAlign);
    else
        ImGui::NewLine();

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

    ImGui::PushStyleColor(ImGuiCol_Button,        { 1.0f, 1.0f, 1.0f, 0.050f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 1.0f, 1.0f, 1.0f, 0.090f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 1.0f, 1.0f, 1.0f, 0.130f });
    if (ImGui::Button(str.cancel, buttonSize))
    {
        m_ShowEditor  = false;
        m_SaveSuccess = false;
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button,        { 0.369f, 0.420f, 1.000f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.500f, 0.550f, 1.000f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 0.280f, 0.330f, 0.860f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_Text,          { 1.0f,   1.0f,   1.0f,   1.0f });
    if (ImGui::Button(str.save, buttonSize))
    {
        bool savedLyrics = SaveBufferToFile();
        ProyecThor::Library::SetSongAuthor(fileName, std::string(m_AuthorBuffer));
        m_SaveSuccess = savedLyrics;
    }
    ImGui::PopStyleColor(4);

    ImGui::PopStyleVar();

    ImGui::End();
}
// ─────────────────────────────────────────────────────────────────────────────
//  Render
// ─────────────────────────────────────────────────────────────────────────────
void SongView::Render()
{
    const auto& str = ProyecThor::UI::GetUIStrings();

    auto& core      = Core::PresentationCore::Get();
    auto  selection = core.PeekSelection();

    if (selection.title.empty() || selection.type != Core::ItemType::Song)
        return;

    if (m_CurrentSongTitle != selection.title)
    {
        m_CurrentSongTitle  = selection.title;
        m_ActiveStanzaIndex = -1;
        m_SaveSuccess       = false;
        m_HasRecordedCurrentSongProjection = false;
    }

    auto presentState = core.GetState();
    int  hAlign       = presentState.songTextAlignment;
    int  vAlign       = presentState.songVAlignment;
ImFont* styleFont = core.GetImGuiFont(presentState.selectedFont, presentState.textSize);
if (!styleFont) styleFont = ImGui::GetFont();

    auto TryRecordProjection = [&](bool userInitiated) {
        if (!userInitiated) return;
        if (selection.title.empty() || selection.contentData.size() < 2) return;
        if (m_ActiveStanzaIndex < 0) return;
        if (m_HasRecordedCurrentSongProjection) return;
        ProyecThor::UI::RecordSongProjection(selection.title, (int)selection.contentData.size());
        m_HasRecordedCurrentSongProjection = true;
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
        if (core.IsProjecting())
            core.SetProjecting(true);

        TryRecordProjection(true);
    }
}
    }

    // ── Header ────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.20f, 0.85f, 0.40f, 1.0f));
    ImGui::TextUnformatted(str.songLyricsDeck);
    ImGui::PopStyleColor();
    ImGui::SameLine(ImGui::GetContentRegionAvail().x
                    - ImGui::CalcTextSize(selection.title.c_str()).x - 5.0f);
    ImGui::TextDisabled("%s", selection.title.c_str());
    ImGui::Separator();

    // ── Boton limpiar pantalla ────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.65f, 0.10f, 0.10f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.80f, 0.20f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.50f, 0.05f, 0.05f, 1.0f));
    if (ImGui::Button(str.songClearScreen, ImVec2(-1, 35)))
    {
        core.ClearLayer2();
        m_ActiveStanzaIndex = -1;
        m_HasRecordedCurrentSongProjection = false;
    }
    ImGui::PopStyleColor(3);
    ImGui::Separator();

    // ── Grid de estrofas ──────────────────────────────────────────────────────
    float availWidth = ImGui::GetContentRegionAvail().x;
    int   columns    = std::max(1, static_cast<int>(availWidth / 250.0f));

    if (ImGui::BeginTable("StanzasGrid", columns, ImGuiTableFlags_SizingStretchSame))
    {
        for (size_t i = 0; i < selection.contentData.size(); ++i)
        {
            ImGui::TableNextColumn();

            const std::string& stanza     = selection.contentData[i];
            bool               isSelected = (m_ActiveStanzaIndex == static_cast<int>(i));

            ImGui::PushID((int)i);

            ImVec2 p_min    = ImGui::GetCursorScreenPos();
            ImVec2 cardSize = ImVec2(ImGui::GetContentRegionAvail().x, 110.0f);
            ImVec2 p_max    = ImVec2(p_min.x + cardSize.x, p_min.y + cardSize.y);

            if (ImGui::InvisibleButton("##select_btn", cardSize))
            {
                m_ActiveStanzaIndex = (int)i;
                core.SetLayer2_Text(stanza);
                if (core.IsProjecting())
                    core.SetProjecting(true);
                TryRecordProjection(true);
            }

            bool isHovered = ImGui::IsItemHovered();

          // ── Menu contextual ───────────────────────────────────────────────
            if (ImGui::BeginPopupContextItem("StanzaContextMenu##ctx", ImGuiPopupFlags_MouseButtonRight))
            {
                ImGui::TextDisabled("Estrofa %d", (int)i + 1);
                ImGui::Separator();

                if (ImGui::MenuItem("Proyectar esta estrofa"))
                {
                    m_ActiveStanzaIndex = (int)i;
                    core.SetLayer2_Text(stanza);
                    if (core.IsProjecting())
                        core.SetProjecting(true);

                    TryRecordProjection(true);
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Editar esta estrofa"))
                {
                    OpenEditorForSong(selection.title, stanza, true);
                }

                ImGui::EndPopup();
            }

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
                ImU32 top    = IM_COL32(14, 14, 20, 255);
                ImU32 bottom = IM_COL32(4, 4, 8, 255);
                drawList->AddRectFilledMultiColor(p_min, p_max, top, top, bottom, bottom);
            }

            if (isSelected)
                drawList->AddRectFilled(p_min, p_max, IM_COL32(40, 120, 60, 60), 10.0f);
            else if (isHovered)
                drawList->AddRectFilled(p_min, p_max, IM_COL32(255, 255, 255, 12), 10.0f);

            ImU32 borderColor = isSelected
                ? IM_COL32(80, 200, 100, 200)
                : IM_COL32(255, 255, 255, 22);
            float borderSize = isSelected ? 1.5f : 1.0f;
           drawList->AddRect(p_min, p_max, borderColor, 10.0f, 0, borderSize);

            drawList->PushClipRect(p_min, p_max, true);

// ── Padding escalado desde los margenes reales del estilo (referencia
//    1920px, igual criterio que el proyector y el preview del editor de
//    estilos) en vez de un padding fijo de 10px ────────────────────────
float cardScale = cardSize.x / 1920.0f;
float padL = std::clamp(presentState.margins[0] * cardScale, 6.0f, cardSize.x * 0.35f);
float padT = std::clamp(presentState.margins[1] * cardScale, 6.0f, cardSize.y * 0.35f);
float padR = std::clamp(presentState.margins[2] * cardScale, 6.0f, cardSize.x * 0.35f);
float padB = std::clamp(presentState.margins[3] * cardScale, 6.0f, cardSize.y * 0.35f);

float safeW = std::max(10.0f, cardSize.x - padL - padR);
float safeH = std::max(10.0f, cardSize.y - padT - padB);

// ── Partir la estrofa en lineas una sola vez ────────────────────────────
std::vector<std::string> lines;
{
    size_t sp = 0, ep = stanza.find('\n');
    while (true) {
        std::string ln = stanza.substr(sp, ep - sp);
        if (!ln.empty() && ln.back() == '\r') ln.pop_back();
        lines.push_back(ln);
        if (ep == std::string::npos) break;
        sp = ep + 1;
        ep = stanza.find('\n', sp);
    }
}

auto measureBlock = [&](float size, float& outW, float& outH) {
    outW = 0.0f;
    for (const auto& ln : lines) {
        if (ln.empty()) continue;
        ImVec2 sz = styleFont->CalcTextSizeA(size, FLT_MAX, 0.0f, ln.c_str());
        outW = std::max(outW, sz.x);
    }
    outH = lines.size() * size;
};

// ── Tamano de fuente: el del estilo elegido, escalado al tamano de la
//    tarjeta, y reducido si autoScale esta activo y no entra ───────────
float displaySize = std::max(6.0f, presentState.textSize * cardScale);

if (presentState.autoScale) {
    float w, h;
    measureBlock(displaySize, w, h);
    while (displaySize > 6.0f && (w > safeW || h > safeH)) {
        displaySize -= 1.0f;
        measureBlock(displaySize, w, h);
    }
}

float blockW, blockH;
measureBlock(displaySize, blockW, blockH);

float startY = padT;
if      (vAlign == 1) startY = std::max(padT, (cardSize.y - blockH) * 0.5f);
else if (vAlign == 2) startY = std::max(padT,  cardSize.y - blockH - padB);

ImU32 textColor = IM_COL32(
    (int)(presentState.textColor[0] * 255.0f),
    (int)(presentState.textColor[1] * 255.0f),
    (int)(presentState.textColor[2] * 255.0f),
    (int)(presentState.textColor[3] * 255.0f));

float currentY = startY;
for (const auto& line : lines)
{
    if (!line.empty())
    {
        ImVec2 lineSz = styleFont->CalcTextSizeA(displaySize, FLT_MAX, 0.0f, line.c_str());
        float localX = padL;
        if      (hAlign == 1) localX = std::max(padL, (cardSize.x - lineSz.x) * 0.5f);
        else if (hAlign == 2) localX = std::max(padL,  cardSize.x - lineSz.x - padR);

        drawList->AddText(styleFont, displaySize,
                          ImVec2(p_min.x + localX, p_min.y + currentY),
                          textColor, line.c_str());
    }
    currentY += displaySize;
}

drawList->PopClipRect();

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    RenderEditorModal();
}

} // namespace ProyecThor::UI