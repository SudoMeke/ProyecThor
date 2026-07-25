#include "LibraryPanel.h"

#include "biblio/LibraryHelpers.h"
#include "biblio/LibrarySidebar.h"
#include "biblio/LibrarySongs.h"
#include "biblio/LibraryVideos.h"
#include "biblio/LibraryDocuments.h"
#include "biblio/LibraryModals.h"

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <commdlg.h>
#endif
#include <imgui.h>
#include <imgui_internal.h>

#include "backend/core/PresentationCore.h"
#include "UIStrings.h"
#include "frontend/ui/UIManager.h"
#include "frontend/ui/IconRail.h"
#include "ui/DesignSystem.h"
#include "biblio/LibraryPlaylists.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>
#include <iterator>
#include <thread>
#include <chrono>
#include <system_error>
#include <algorithm>

namespace fs = std::filesystem;

using namespace ProyecThor::Library;

static const std::string k_StreamURLsFile = "/stream_urls.txt";

namespace ProyecThor::UI {

namespace {

bool TryRemoveWithRetry(const fs::path& target, int maxAttempts = 8, int delayMs = 200)
{
    std::error_code ec;
    for (int attempt = 0; attempt < maxAttempts; ++attempt)
    {
        fs::remove_all(target, ec);
        if (!ec)
            return true;

        if (attempt < maxAttempts - 1)
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }
    return false;
}

// -----------------------------------------------------------------------------
//  ImportSelectedFileToLibrary
//  Logica de copia compartida entre la rama Windows y la rama Linux de
//  ImportFile(). Recibe la ruta ya seleccionada por el usuario (via el dialogo
//  nativo en Windows o via zenity en Linux) y la copia a la carpeta que
//  corresponda segun la categoria actual de la biblioteca.
// -----------------------------------------------------------------------------
void ImportSelectedFileToLibrary(const fs::path& src, LibraryCategory category, const std::string& base)
{
    try {
        if (category == LibraryCategory::Documents) {
#ifdef _WIN32
            std::string docName = WideToUtf8(src.stem().wstring());
#else
            std::string docName = src.stem().string();
#endif
            fs::path docDir = U8Path(base + "/documents") / U8Path(docName);
            fs::create_directories(docDir);
            fs::copy(src, docDir / src.filename(),
                     fs::copy_options::overwrite_existing);
        } else {
            std::string destFolder;
            switch (category) {
                case LibraryCategory::Songs:  destFolder = base + "/songs";  break;
                case LibraryCategory::Videos: destFolder = base + "/videos"; break;
                case LibraryCategory::Images: destFolder = base + "/images"; break;
                case LibraryCategory::Bibles: destFolder = base + "/bibles"; break;
                default:                      destFolder = base + "/audio";  break;
            }
            fs::copy(src, U8Path(destFolder) / src.filename(),
                     fs::copy_options::overwrite_existing);
        }
    } catch (const std::exception& e) {
        std::cerr << "[LibraryPanel] Error al importar: " << e.what() << '\n';
    }
}

} // namespace

// =============================================================================
//  BuildContext
// =============================================================================
Library::LibraryContext LibraryPanel::BuildContext()
{
    return Library::LibraryContext{
        reinterpret_cast<int&>(m_CurrentCategory),
        reinterpret_cast<int&>(m_SideMode),
        m_Items,
        m_SelectedIndex,
        m_SearchBuffer,
        static_cast<int>(sizeof(m_SearchBuffer)),
        m_StreamURLs,
        m_SelectedURLIndex,
        m_URLInputBuffer,
        static_cast<int>(sizeof(m_URLInputBuffer)),
        m_ShowSongEditor,
        m_EditTitle,
        m_EditContent,
        m_EditAuthor,
        m_ShowRenameModal,
        m_RenameOldName,
        m_RenameExtension,
        m_RenameBuffer,
        m_RenameIsURL,
        m_RenameURLIndex,
        m_LoadedDocPath,
        m_MonitorRef,
        [this]() { RefreshList(); },
        [this]() { DeleteSelectedItem(); },
        [this]() { ImportFile(); },
        [this]() { CreateNewSong(); },
        [this](const std::string& t, const std::string& c, const std::string& a) { SaveSong(t, c, a); },
        [this]() { LoadStreamURLs(); },
        [this]() { SaveStreamURLs(); },
        [this](const std::string& f) { return LoadSongVerses(f); },
        [](const std::string& styleName) {
            Core::PresentationCore::Get().ApplyStyleByName(styleName);
        },
        m_ShowPlaylistsTab,
        m_ActivePlaylistName,
        m_ActivePlaylistIndex,
        []() { return Library::ListPlaylists(); },
        [](const std::string& name) { return Library::LoadPlaylist(name).songs; },
        [](const std::string& name) { return Library::CreatePlaylist(name); },
        [](const std::string& name) { Library::DeletePlaylist(name); },
        [](const std::string& a, const std::string& b) { return Library::RenamePlaylist(a, b); },
        [](const std::string& pl, const std::string& song) { Library::AddSongToPlaylist(pl, song); },
        [](const std::string& pl, int idx) { Library::RemoveSongFromPlaylist(pl, idx); },
        [](const std::string& pl, int idx, int delta) { Library::MovePlaylistSong(pl, idx, delta); },
        [this](const std::string& pl, int idx) { SelectPlaylistSong(pl, idx); },
        m_EditTags,
        [](const std::string& f) { return Library::GetSongTags(f); },
        [](const std::string& f, const std::vector<std::string>& t) { Library::SetSongTags(f, t); }
    };
}

// =============================================================================
//  Constructor
// =============================================================================
LibraryPanel::LibraryPanel()
{
    // Mudado desde ViewToolsPanel — ver PresentationCore::SetOClockRef y el
    // comentario de m_OClock en LibraryPanel.h.
    Core::PresentationCore::Get().SetOClockRef(&m_OClock);

    try {
        const std::string& base = GetAssetsPath();
        fs::create_directories(U8Path(base + "/songs"));
        fs::create_directories(U8Path(base + "/videos"));
        fs::create_directories(U8Path(base + "/images"));
        fs::create_directories(U8Path(base + "/bibles"));
        fs::create_directories(U8Path(base + "/documents"));
        fs::create_directories(U8Path(base + "/audio"));
    } catch (const std::exception& e) {
        std::cerr << "[LibraryPanel] Advertencia IO: " << e.what() << '\n';
    }
    RefreshList();
    LoadStreamURLs();
}

// =============================================================================
//  IO — URLs de streaming
// =============================================================================
void LibraryPanel::LoadStreamURLs()
{
    m_StreamURLs.clear();
    std::ifstream f(U8Path(GetAssetsPath() + k_StreamURLsFile));
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty() && line.rfind("http", 0) == 0)
            m_StreamURLs.push_back(line);
    }
}

void LibraryPanel::SaveStreamURLs()
{
    std::ofstream f(U8Path(GetAssetsPath() + k_StreamURLsFile));
    if (!f.is_open()) return;
    for (const auto& u : m_StreamURLs) f << u << '\n';
}

// =============================================================================
//  RefreshList
// =============================================================================
void LibraryPanel::RefreshList()
{
    if (m_CurrentCategory == LibraryCategory::Audio) {
        m_Items.clear();
        ForceListUpdate() = true;
        return;
    }

    m_Items.clear();
    const std::string& base = GetAssetsPath();
    std::string path;
    switch (m_CurrentCategory) {
        case LibraryCategory::Songs:     path = base + "/songs";     break;
        case LibraryCategory::Videos:    path = base + "/videos";    break;
        case LibraryCategory::Images:    path = base + "/images";    break;
        case LibraryCategory::Bibles:    path = base + "/bibles";    break;
        case LibraryCategory::Documents: path = base + "/documents"; break;
        default: break;
    }

    try {
        fs::path fsPath = U8Path(path);
        if (fs::exists(fsPath)) {
            for (const auto& entry : fs::directory_iterator(fsPath)) {
#ifdef _WIN32
                std::string name = WideToUtf8(entry.path().filename().wstring());
#else
                std::string name = entry.path().filename().string();
#endif
                if (m_CurrentCategory == LibraryCategory::Documents) {
                    if (entry.is_directory()) m_Items.push_back(name);
                } else {
                    if (entry.is_regular_file()) m_Items.push_back(name);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[LibraryPanel] Error IO: " << e.what() << '\n';
    }

    if (m_Items.empty() && m_CurrentCategory == LibraryCategory::Songs)
        m_Items = { "Cuan_Grande_es_El.txt", "Gracia_Sublime.txt" };

    if (m_CurrentCategory == LibraryCategory::Videos)
        LoadStreamURLs();

    ForceListUpdate() = true;
}

// =============================================================================
//  DeleteSelectedItem
// =============================================================================
void LibraryPanel::DeleteSelectedItem()
{
    if (m_SelectedIndex < 0 || m_SelectedIndex >= (int)m_Items.size())
        return;

    const std::string& base = GetAssetsPath();
    std::string folder;
    switch (m_CurrentCategory) {
        case LibraryCategory::Songs:     folder = base + "/songs/";     break;
        case LibraryCategory::Videos:    folder = base + "/videos/";    break;
        case LibraryCategory::Images:    folder = base + "/images/";    break;
        case LibraryCategory::Documents: folder = base + "/documents/"; break;
        case LibraryCategory::Audio:     folder = base + "/audio/";     break;
        default:                         folder = base + "/bibles/";    break;
    }

    const std::string itemName = m_Items[m_SelectedIndex];
    const std::string fullPath = folder + itemName;

    auto& core = Core::PresentationCore::Get();
    auto  currentSelection = core.PeekSelection();

    const bool isDocumentInUse =
        (m_CurrentCategory == LibraryCategory::Documents) &&
        (!m_LoadedDocPath.empty()) &&
        (m_LoadedDocPath.rfind(fullPath, 0) == 0);

    const bool isCurrentlySelected =
        (currentSelection.title == itemName) || isDocumentInUse;

    const bool isVideoCategory = (m_CurrentCategory == LibraryCategory::Videos);

    if (isVideoCategory)
    {
        // Bloquea la ruta ANTES de detener la reproduccion. Mientras el
        // bloqueo esta activo, VLCBasePlayer::Play() ignora cualquier
        // intento de volver a abrir este archivo, sin importar quien lo
        // dispare (cola automatica, boton manual, etc.). Esto es lo que
        // evita que el video se reabra justo despues del Stop() y deje el
        // archivo bloqueado para el borrado.
        core.BlockBackgroundPath(fullPath);
        core.StopBackgroundMedia();
    }

    if (isCurrentlySelected)
    {
        core.SetProjecting(false);
        core.ClearLayer2();
    }

    if (m_CurrentCategory == LibraryCategory::Documents && isDocumentInUse)
        m_LoadedDocPath.clear();

    const bool removed = TryRemoveWithRetry(U8Path(fullPath));

    if (isVideoCategory)
        core.UnblockBackgroundPath();

    if (!removed)
    {
        std::cerr << "[LibraryPanel] No se pudo eliminar, el archivo sigue en uso: "
                  << fullPath << '\n';
        ShowFileInUseToast(itemName);
        return;
    }

    m_SelectedIndex = -1;
    if (m_CurrentCategory == LibraryCategory::Documents)
        m_LoadedDocPath.clear();

    RefreshList();
}

// =============================================================================
//  ShowFileInUseToast — arma el aviso temporal
// =============================================================================
void LibraryPanel::ShowFileInUseToast(const std::string& fileName)
{
    m_ShowFileInUseToast  = true;
    m_FileInUseToastName  = fileName;
    m_FileInUseToastTimer = 3.5f;
}

// =============================================================================
//  RenderFileInUseToast — dibuja y hace desvanecer el aviso
// =============================================================================
void LibraryPanel::RenderFileInUseToast()
{
    if (!m_ShowFileInUseToast)
        return;

    m_FileInUseToastTimer -= ImGui::GetIO().DeltaTime;
    if (m_FileInUseToastTimer <= 0.0f)
    {
        m_ShowFileInUseToast = false;
        m_FileInUseToastName.clear();
        return;
    }

    const float k_FadeInOut = 0.4f;
    float alpha = 1.0f;
    if (m_FileInUseToastTimer < k_FadeInOut)
        alpha = m_FileInUseToastTimer / k_FadeInOut;

    std::string message = "No se pudo eliminar \"" + m_FileInUseToastName + "\": el archivo esta en uso.";

    ImGuiIO& io = ImGui::GetIO();
    ImVec2   displaySize = io.DisplaySize;

    ImFont* font = ImGui::GetFont();
    // Usamos ImGui::GetFontSize() en lugar de intentar obtenerlo del objeto font
    ImVec2 textSize = font->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.0f, message.c_str());

    const float padX = 16.0f;
    const float padY = 10.0f;
    const float boxW = textSize.x + padX * 2.0f;
    const float boxH = textSize.y + padY * 2.0f;
    const float marginBottom = 32.0f;

    ImVec2 boxMin(
        (displaySize.x - boxW) * 0.5f,
        displaySize.y - marginBottom - boxH
    );
    ImVec2 boxMax(boxMin.x + boxW, boxMin.y + boxH);

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    ImU32 bgColor   = IM_COL32(35, 15, 15, static_cast<int>(230 * alpha));
    ImU32 borderCol = IM_COL32(200, 70, 70, static_cast<int>(200 * alpha));
    ImU32 textCol   = IM_COL32(255, 220, 220, static_cast<int>(255 * alpha));

    dl->AddRectFilled(boxMin, boxMax, bgColor, 8.0f);
    dl->AddRect(boxMin, boxMax, borderCol, 8.0f, 0, 1.5f);

    ImVec2 textPos(boxMin.x + padX, boxMin.y + padY);
    dl->AddText(textPos, textCol, message.c_str());
}

// =============================================================================
//  Render — ahora envuelto en DS::BeginGlassPanel/EndGlassPanel
// =============================================================================

void LibraryPanel::Render()
{
    // ── Pump incondicional ──────────────────────────────────────────────────
    // Mudado desde ViewToolsPanel junto con m_OClock/m_StreamingPanel: deben
    // seguir corriendo aunque el operador este mirando otra categoria de
    // Biblioteca (Reloj alimenta LAN/pantalla, Streaming alimenta la
    // transmision), sin importar si el grupo Red/Reloj esta activo ahora.
    m_OClock.Update();
    m_StreamingPanel.Update();

    const auto& str = ProyecThor::UI::GetUIStrings();

    if (m_CurrentCategory != m_PrevCategory)
    {
        m_AudioSelectionSet = false;
        m_PrevCategory      = m_CurrentCategory;
        m_SearchBuffer[0]   = '\0';
        RefreshList();
    }
    
    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyShift) // Solo si Shift está presionado
    {
        // Revisamos teclas del 1 al 6 (código ASCII '1' a '6')
        for (int i = 0; i < 6; ++i)
        {
            // FIX: Casteamos el entero resultante de vuelta a ImGuiKey
            if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_1 + i)))
            {
                // Convertimos el índice 0-5 a tu enum LibraryCategory
                m_CurrentCategory = static_cast<LibraryCategory>(i);
                m_SideMode = LibrarySideMode::Categories;

                // Opcional: limpiar selección o refrescar al cambiar
                m_SelectedIndex = -1;
                RefreshList();
                break;
            }
        }
    }
    
    bool visible = false;

    if (m_UIManagerRef)
    {
        visible = DS::BeginGlassPanel(str.library, m_UIManagerRef->GetGlassRenderer(),
                                      nullptr, 0, ImVec2(0.0f, 0.0f));
    }
    else
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        visible = ImGui::Begin(str.library);
        ImGui::PopStyleVar();
    }

    if (!visible)
    {
        if (m_UIManagerRef) DS::EndGlassPanel();
        else                ImGui::End();
        RenderFileInUseToast();
        return;
    }

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && m_UIManagerRef)
        m_UIManagerRef->SetActiveLeftPanel(ActiveLeftPanel::Library);

    const float k_SidebarW = IconRailThickness(true);
    const float     totalH     = ImGui::GetContentRegionAvail().y;

    // ── Sidebar izquierdo ──────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::BeginChild("##sidebar", ImVec2(k_SidebarW, totalH), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    {
        Library::LibraryContext ctx = BuildContext();
        Library::RenderCategoryButtons(ctx);
    }

    ImGui::EndChild();

    // ── Divisor vertical con gradiente ────────────────────────────────────
    ImGui::SameLine(0.f, 0.f);
    {
        ImVec2      p  = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 colTop   = IM_COL32(60, 80, 160,  0);
        ImU32 colMid   = IM_COL32(60, 80, 160, 80);
        ImU32 colBot   = IM_COL32(60, 80, 160,  0);
        float midY     = p.y + totalH * 0.5f;
        dl->AddRectFilledMultiColor(
            p,              { p.x + 1.f, midY },
            colTop, colTop, colMid, colMid);
        dl->AddRectFilledMultiColor(
            { p.x, midY },  { p.x + 1.f, p.y + totalH },
            colMid, colMid, colBot, colBot);
    }
    ImGui::SameLine(0.f, 1.0f);

    // ── Panel de contenido derecho ─────────────────────────────────────────
    // Margen unificado para TODAS las categorias (Canciones, Video, Documentos,
    // Audio). Centralizado aca para que ningun sub-panel (por ejemplo el grid
    // de Canciones/Playlists, que resetea su propio WindowPadding a 0 para
    // alinear columnas) pueda "comerse" el margen exterior del panel.
    constexpr float kContentMarginX = 18.0f;
    constexpr float kContentMarginY = 16.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kContentMarginX, kContentMarginY));
    ImGui::BeginChild("##content", ImVec2(0.f, totalH),
                      ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();

    {
        Library::LibraryContext ctx = BuildContext();

        if (m_SideMode == LibrarySideMode::Streaming)
        {
            m_StreamingPanel.RenderContent();
        }
        else if (m_SideMode == LibrarySideMode::Clock)
        {
            if (m_UIManagerRef) m_OClock.Render(m_UIManagerRef->GetGlassRenderer());
        }
        else if (m_CurrentCategory == LibraryCategory::Audio)
        {
            if (!m_AudioSelectionSet)
            {
                Core::LibrarySelection audioSel;
                audioSel.type  = Core::ItemType::Audio;
                audioSel.title = "Audio";
                Core::PresentationCore::Get().SetSelection(audioSel);
                m_AudioSelectionSet = true;
            }

            m_AudioPanel.RenderLibraryList();
        }
        else if (m_CurrentCategory == LibraryCategory::Videos)
        {
            Library::RenderVideoSection(ctx);
        }
        else if (m_CurrentCategory == LibraryCategory::Documents)
        {
            Library::RenderDocumentSection(ctx, m_DocumentView);
        }
        else
        {
            Library::RenderSideList(ctx);
        }

        Library::RenderRenameModal(ctx);
    }

    ImGui::EndChild();

    if (m_UIManagerRef) DS::EndGlassPanel();
    else                ImGui::End();

    RenderFileInUseToast();
}

// =============================================================================
//  Helpers — canciones
// =============================================================================
// Rework del editor: ya no abre un popup modal para pedir titulo/autor/
// contenido antes de crear el archivo (RenderSongEditor, retirado). En vez
// de eso, crea de una un archivo vacio con un nombre unico, lo selecciona, y
// pide (via el cue "consumir una vez" de PresentationCore) que SongView
// entre directo al editor unificado apenas la seleccion coincida —
// SongEditView permite renombrar el titulo visible desde adentro.
void LibraryPanel::CreateNewSong()
{
    const std::string base = "Nueva cancion";
    std::string filename = base + ".txt";
    int suffix = 2;
    while (fs::exists(U8Path(GetAssetsPath() + "/songs/" + filename))) {
        filename = base + " (" + std::to_string(suffix) + ").txt";
        ++suffix;
    }

    std::ofstream f(U8Path(GetAssetsPath() + "/songs/" + filename));
    if (f.is_open())
        f << "\xEF\xBB\xBF";
    f.close();

    RefreshList();

    Core::LibrarySelection s;
    s.title       = filename;
    s.type        = Core::ItemType::Song;
    s.contentData = LoadSongVerses(filename);
    Core::PresentationCore::Get().SetSelection(s);

    auto it = std::find(m_Items.begin(), m_Items.end(), filename);
    if (it != m_Items.end())
        m_SelectedIndex = (int)std::distance(m_Items.begin(), it);

    Core::PresentationCore::Get().RequestSongEditorOpen(filename);
}

void LibraryPanel::SaveSong(const std::string& title, const std::string& content, const std::string& /*author*/)
{
    if (title.empty()) return;
    std::string filename = title;
    if (filename.find(".txt") == std::string::npos) filename += ".txt";

    std::ofstream f(U8Path(GetAssetsPath() + "/songs/" + filename));
    if (f.is_open()) {
        f << "\xEF\xBB\xBF";
        f << content;
        RefreshList();
    }
}

// =============================================================================
//  LoadSongVerses — delega en Library::LoadSongVerses (LibrarySongs.cpp), que
//  es la unica implementacion real (antes estaba duplicada aca). Se mantiene
//  este metodo (en vez de que los llamadores usen la funcion libre
//  directamente) para no tocar el wiring existente de ctx.loadSongVerses ni
//  la llamada de SelectPlaylistSong mas abajo.
// =============================================================================
std::vector<std::string> LibraryPanel::LoadSongVerses(const std::string& filename)
{
    return Library::LoadSongVerses(filename);
}

// =============================================================================
//  ImportFile
//  Multiplataforma: en Windows abre el dialogo nativo (OPENFILENAMEW). En
//  Linux invoca "zenity --file-selection" (requiere tener zenity instalado
//  en el sistema). En ambos casos, una vez elegido el archivo, la copia a la
//  carpeta correspondiente se hace con ImportSelectedFileToLibrary, que es
//  identica para las dos plataformas.
// =============================================================================
void LibraryPanel::SelectPlaylistSong(const std::string& playlistName, int index)
{
    Library::Playlist pl = Library::LoadPlaylist(playlistName);
    if (index < 0 || index >= (int)pl.songs.size()) return;

    const std::string& filename = pl.songs[index];

    Core::LibrarySelection s;
    s.title       = filename;
    s.type        = Core::ItemType::Song;
    s.contentData = LoadSongVerses(filename);
    Core::PresentationCore::Get().SetSelection(s);

    std::string defaultStyle =
        Core::PresentationCore::Get().GetCategoryDefaultStyle(Core::ItemType::Song);
    if (!defaultStyle.empty())
        Core::PresentationCore::Get().ApplyStyleByName(defaultStyle);

    m_ActivePlaylistName  = playlistName;
    m_ActivePlaylistIndex = index;

    auto it = std::find(m_Items.begin(), m_Items.end(), filename);
    if (it != m_Items.end())
        m_SelectedIndex = (int)std::distance(m_Items.begin(), it);
}

void LibraryPanel::ImportFile()
{
#ifdef _WIN32
    wchar_t filename[MAX_PATH] = {};
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = nullptr;

    if      (m_CurrentCategory == LibraryCategory::Videos)
        ofn.lpstrFilter = L"Videos\0*.mp4;*.mkv;*.avi;*.mov\0Todos\0*.*\0";
    else if (m_CurrentCategory == LibraryCategory::Images)
        ofn.lpstrFilter = L"Imagenes\0*.jpg;*.png;*.jpeg\0Todos\0*.*\0";
    else if (m_CurrentCategory == LibraryCategory::Songs)
        ofn.lpstrFilter = L"Textos\0*.txt\0Todos\0*.*\0";
    else if (m_CurrentCategory == LibraryCategory::Documents)
        ofn.lpstrFilter = L"Documentos\0*.pdf;*.pptx;*.ppt;*.odp\0Todos\0*.*\0";
    else
        ofn.lpstrFilter = L"Todos los archivos\0*.*\0";

    ofn.lpstrFile = filename;
    ofn.nMaxFile  = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameW(&ofn)) return;

    fs::path src(filename);
#else
    std::string filter;
    switch (m_CurrentCategory) {
        case LibraryCategory::Videos:
            filter = "--file-filter=Videos | *.mp4 *.mkv *.avi *.mov";
            break;
        case LibraryCategory::Images:
            filter = "--file-filter=Imagenes | *.jpg *.jpeg *.png";
            break;
        case LibraryCategory::Songs:
            filter = "--file-filter=Textos | *.txt";
            break;
        case LibraryCategory::Documents:
            filter = "--file-filter=Documentos | *.pdf *.pptx *.ppt *.odp";
            break;
        default:
            filter = "--file-filter=Todos | *";
            break;
    }

    std::string command = "zenity --file-selection --title=\"Importar archivo\" \"" +
                          filter + "\" 2>/dev/null";

    std::string result;
    char buffer[1024];
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        std::cerr << "[LibraryPanel] No se pudo abrir el selector de archivos (zenity).\n";
        return;
    }
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        result += buffer;
    int status = pclose(pipe);

    if (status != 0 || result.empty()) return;
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    if (result.empty()) return;

    fs::path src(result);
#endif

    ImportSelectedFileToLibrary(src, m_CurrentCategory, GetAssetsPath());
    RefreshList();
}

} // namespace ProyecThor::UI