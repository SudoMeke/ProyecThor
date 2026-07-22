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
#include "frontend/ui/DesignSystem.h"

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
//  Fondos disponibles para el combo de "Fondo" del popup de ajustes — misma
//  carpeta que usa el tab "Fondos" (ver LayersBgTab::BgRootDir/ReloadList),
//  duplicada aca liviana (sin miniaturas ni cache de texturas, solo la lista
//  de archivos) para no encadenar SongView con esa clase.
// ─────────────────────────────────────────────────────────────────────────────
struct SongBgEntry { std::string fullPath; std::string label; bool isImage = false; };

static std::filesystem::path SongBgRootDir()
{
    std::filesystem::path dir;
#ifdef _WIN32
    wchar_t buf[MAX_PATH] = {};
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf);
    dir = std::filesystem::path(buf) / "ProyecThor";
#else
    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
    std::filesystem::path base;
    if (xdgConfig && *xdgConfig)
        base = std::filesystem::path(xdgConfig);
    else
        base = std::filesystem::path(std::getenv("HOME") ? std::getenv("HOME") : ".") / ".config";
    dir = base / "ProyecThor";
#endif
    return dir / "assets" / "backgrounds";
}

static std::vector<SongBgEntry> ListSongBackgrounds()
{
    auto isMedia = [](const std::string& ext) {
        return ext == ".mp4" || ext == ".mkv" || ext == ".avi" || ext == ".mov"
            || ext == ".jpg" || ext == ".jpeg" || ext == ".png";
    };
    auto isImageExt = [](const std::string& ext) {
        return ext == ".jpg" || ext == ".jpeg" || ext == ".png";
    };
    auto addFile = [&](const std::filesystem::directory_entry& f, const std::string& folder,
                        std::vector<SongBgEntry>& out) {
        std::string ext = f.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (!isMedia(ext)) return;
        SongBgEntry entry;
        entry.fullPath = f.path().string();
        entry.label    = folder.empty() ? f.path().stem().string() : (folder + "/" + f.path().stem().string());
        entry.isImage  = isImageExt(ext);
        out.push_back(std::move(entry));
    };

    std::vector<SongBgEntry> out;
    std::error_code ec;
    std::filesystem::path root = SongBgRootDir();
    if (!std::filesystem::exists(root, ec)) return out;

    for (const auto& e : std::filesystem::directory_iterator(root, ec))
    {
        if (e.is_directory())
        {
            std::string folder = e.path().filename().string();
            std::error_code subEc;
            for (const auto& sub : std::filesystem::directory_iterator(e.path(), subEc))
                if (sub.is_regular_file()) addFile(sub, folder, out);
        }
        else if (e.is_regular_file())
        {
            addFile(e, "", out);
        }
    }
    std::sort(out.begin(), out.end(), [](const SongBgEntry& a, const SongBgEntry& b) { return a.label < b.label; });
    return out;
}

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
//  Render
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
        m_SaveSuccess       = false;
        m_HasRecordedCurrentSongProjection = false;
    }

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
        OpenEditorForSong(selection.title, "", false);

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

    RenderEditorModal();
}

} // namespace ProyecThor::UI