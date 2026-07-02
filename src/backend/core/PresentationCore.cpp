#include "PresentationCore.h"
#include "BackgroundLayer.h"
#include "OverlayLayer.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include "backend/settings/SettingsManager.h"
#include <filesystem>
#include <algorithm>
#include "AppPaths.h"
#include <fstream>
#include <sstream>
#include <windows.h>
#include <shlobj.h>
#include "NetworkStreamServer.h"

namespace ProyecThor::Core {

    class PresentationCoreImpl {
    public:
        BackgroundLayer background;
        OverlayLayer    overlay;
        BackgroundLayer preview;
    };

    PresentationCore::PresentationCore()
        : m_Impl(std::make_unique<PresentationCoreImpl>()) {}

    PresentationCore::~PresentationCore() {
        if (m_NetworkServer && m_NetworkServer->IsRunning())
            m_NetworkServer->Stop();
        DestroyFBO();
        DestroyProjectorWindow();
    }

    LibrarySelection PresentationCore::GetSelection() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        LibrarySelection sel  = m_CurrentSelection;
        m_CurrentSelection.title = "";
        m_CurrentSelection.type  = ItemType::None;
        m_CurrentSelection.contentData.clear();
        return sel;
    }

    LibrarySelection PresentationCore::PeekSelection() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_CurrentSelection;
    }

    void PresentationCore::SetLiveQuickNote(const std::string& text) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.currentText   = text;
        m_State.showText      = !text.empty();
        m_State.showQuickNote = true;
        m_State.isProjecting  = true;
        ++m_StreamVersion;
    }

    void PresentationCore::ClearQuickNote() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.currentText   = "";
        m_State.showText      = false;
        m_State.showQuickNote = false;
        ++m_StreamVersion;
    }

    // ── Nota rápida SOLO LAN ─────────────────────────────────────────────
    // A propósito NO toca currentText/showText/isProjecting: eso es lo que
    // usa la pantalla principal/proyector. Este texto vive aparte y solo lo
    // consume el SnapshotProvider de red (ver ToggleNetworkStream más abajo),
    // por lo que jamás se dibuja localmente.
    void PresentationCore::SetLiveQuickNoteLAN(const std::string& text) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.lanQuickNoteText = text;
        m_State.showLanQuickNote = !text.empty();
        ++m_StreamVersion;
    }

    void PresentationCore::ClearQuickNoteLAN() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.lanQuickNoteText = "";
        m_State.showLanQuickNote = false;
        ++m_StreamVersion;
    }

    PresentationState PresentationCore::GetState() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_State;
    }
void* PresentationCore::GetPreviewTexture() {
    return m_Impl ? m_Impl->preview.GetTextureID() : nullptr;
}

VLCBasePlayer* PresentationCore::GetPreviewPlayer() {
    return m_Impl ? m_Impl->preview.GetPlayer() : nullptr;
}

void PresentationCore::SetPreviewMedia(const std::string& path) {
    if (m_Impl) m_Impl->preview.SetVideo(path);
}

void PresentationCore::StopPreviewMedia() {
    if (m_Impl) m_Impl->preview.SetSolidColor(0.0f, 0.0f, 0.0f); // internamente hace Stop()
}
    void* PresentationCore::GetProcessedBackgroundTexture(int targetW, int targetH) {
        return m_Impl ? m_Impl->background.GetProcessedTexture(targetW, targetH) : nullptr;
    }

    void* PresentationCore::GetOverlayTexture() {
        return m_Impl ? m_Impl->overlay.GetTextureID() : nullptr;
    }

    void PresentationCore::SetFSREnabled(bool enabled) {
        if (m_Impl) m_Impl->background.SetFSREnabled(enabled);
    }

    bool PresentationCore::GetFSREnabled() const {
        return m_Impl ? m_Impl->background.GetFSREnabled() : false;
    }

    void PresentationCore::SetFSRSharpness(float sharpness) {
        if (m_Impl) m_Impl->background.SetFSRSharpness(sharpness);
    }

    float PresentationCore::GetFSRSharpness() const {
        return m_Impl ? m_Impl->background.GetFSRSharpness() : 0.2f;
    }

    void PresentationCore::SetStretchToFill(bool s) {
        m_stretchToFill = s;
        if (m_Impl)
            m_Impl->background.SetStretchToFill(s);
    }

    bool PresentationCore::GetStretchToFill() const {
        return m_Impl ? m_Impl->background.GetStretchToFill() : false;
    }

    void PresentationCore::Update() {
        if (m_Impl) {
            m_Impl->background.Update();
            m_Impl->overlay.Update();
             m_Impl->preview.Update();
        }
    }

    void PresentationCore::RenderBackground(int outputW, int outputH) {
        if (m_Impl)
            m_Impl->background.Render(outputW, outputH);
    }

    void PresentationCore::RenderProjectorWindow() {
        if (m_Impl) {
            m_Impl->background.Render(m_ProjectorWidth, m_ProjectorHeight);
            m_Impl->overlay.Render();
        }
    }

    void PresentationCore::CreateProjectorWindow() {
        std::cout << "[Projector] Modo integrado: no se crea ventana nativa secundaria.\n";

        int targetIndex = ProyecThor::Settings::SettingsManager::Get().GetSettings().projection.targetMonitor;
        if (targetIndex < 0) {
            int monitorCount = 0;
            glfwGetMonitors(&monitorCount);
            targetIndex = (monitorCount > 1) ? 1 : 0;
        }

        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.targetMonitorIndex = targetIndex;
    }

    void PresentationCore::DestroyProjectorWindow() {
        m_ProjectorWindow = nullptr;
    }

    GLFWwindow* PresentationCore::GetProjectorWindow() const {
        return nullptr;
    }

    void PresentationCore::SetBackgroundMedia(const std::string& path, bool /*isVideo*/) {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_State.bgPath = path;
            m_State.bgType = PresentationState::BackgroundType::Video;
            ++m_StreamVersion;
        }
        if (m_Impl)
            m_Impl->background.SetVideo(path);
    }
void PresentationCore::StopBackgroundMedia() {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_State.bgPath     = "";
            m_State.bgType     = PresentationState::BackgroundType::SolidColor;
            m_State.bgColor[0] = 0.0f;
            m_State.bgColor[1] = 0.0f;
            m_State.bgColor[2] = 0.0f;
            ++m_StreamVersion;
        }
        if (m_Impl) m_Impl->background.SetSolidColor(0.0f, 0.0f, 0.0f);
    }

    void PresentationCore::BlockBackgroundPath(const std::string& path) {
        if (m_Impl) m_Impl->background.BlockPath(path);
    }

    void PresentationCore::UnblockBackgroundPath() {
        if (m_Impl) m_Impl->background.UnblockPath();
    }
    void PresentationCore::SetLayer0_Color(float r, float g, float b) {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_State.bgColor[0] = r;
            m_State.bgColor[1] = g;
            m_State.bgColor[2] = b;
            m_State.bgType     = PresentationState::BackgroundType::SolidColor;
            m_State.bgPath     = "";
            ++m_StreamVersion;
        }
        if (m_Impl) m_Impl->background.SetSolidColor(r, g, b);
    }

    void PresentationCore::SetOverlayMedia(const std::string& path) {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_State.overlayPath = path;
        }
        if (m_Impl) m_Impl->overlay.PlayOverlay(path);
    }

    void PresentationCore::StopOverlayMedia() {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_State.overlayPath = "";
        }
        if (m_Impl) m_Impl->overlay.StopOverlay();
    }

    void PresentationCore::UpdateTextStyle(float size, const float color[4], int align,
                                           int vAlign, const float margins[4], bool autoScale,
                                           const std::string& font) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.textSize      = size;
        m_State.textAlignment = align;
        m_State.vAlignment    = vAlign;
        m_State.autoScale     = autoScale;
        m_ActiveFontName      = font;

        for (int i = 0; i < 4; i++) {
            m_State.textColor[i] = color[i];
            if (margins) m_State.margins[i] = margins[i];
        }
        ++m_StreamVersion;
    }

    void PresentationCore::UpdateBibleStyle(float refSize, float verseSize,
                                             int hAlign, int vAlign) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.refTextSize        = refSize;
        m_State.verseTextSize      = verseSize;
        m_State.bibleTextAlignment = hAlign;
        m_State.bibleVAlignment    = vAlign;
    }

    void PresentationCore::UpdateSongStyle(int hAlign, int vAlign) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.songTextAlignment = hAlign;
        m_State.songVAlignment    = vAlign;
    }

    void PresentationCore::SetLayer2_Text(const std::string& text) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.currentText = text;
        m_State.showText    = !text.empty();
        ++m_StreamVersion;
    }

    void PresentationCore::ClearLayer2() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.currentText = "";
        m_State.showText    = false;
        ++m_StreamVersion;
    }

    void PresentationCore::SetProjecting(bool projecting) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.isProjecting = projecting;
        ++m_StreamVersion;
    }

    bool PresentationCore::IsProjecting() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_State.isProjecting;
    }

    void PresentationCore::SetTargetMonitor(int index) {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_State.targetMonitorIndex = index;
        }
        std::cout << "[Projector] Monitor objetivo: " << index << "\n";
    }

    void PresentationCore::SetProjectorSize(int w, int h) {
        m_ProjectorWidth  = w;
        m_ProjectorHeight = h;
    }

    VLCBasePlayer* PresentationCore::GetBackgroundPlayer() {
        return m_Impl ? m_Impl->background.GetPlayer() : nullptr;
    }

    VLCBasePlayer* PresentationCore::GetOverlayPlayer() {
        return m_Impl ? m_Impl->overlay.GetPlayer() : nullptr;
    }

    float PresentationCore::GetLivePosition() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_State.livePosition;
    }

    void PresentationCore::SetLivePosition(float pos) {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_State.livePosition = pos;
        }
        if (m_Impl) {
            VLCBasePlayer* player = m_Impl->background.GetPlayer();
            if (player) player->SetPosition(pos);
        }
    }

    int PresentationCore::GetLiveVolume() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_State.liveVolume;
    }

    void PresentationCore::SetLiveVolume(int volume) {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_State.liveVolume = volume;
        }
        if (m_Impl) {
            VLCBasePlayer* player = m_Impl->background.GetPlayer();
            if (player) player->SetVolume(volume);
        }
    }

    void PresentationCore::LoadFontsIntoImGui() {
        ImGuiIO& io = ImGui::GetIO();
        m_ImGuiFonts["Predeterminada"] = io.Fonts->AddFontDefault();

        const float baseFontSize = 60.0f;
        std::string fontsDir = ProyecThor::GetAssetsPath() + "/fonts";

        try {
            if (std::filesystem::exists(fontsDir)) {
                for (const auto& entry : std::filesystem::directory_iterator(fontsDir)) {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                    if (ext == ".ttf" || ext == ".otf" || ext == ".ttc") {
                        std::string fontName = entry.path().stem().string();
                        std::string fullPath = entry.path().string();
                        ImFont* font = io.Fonts->AddFontFromFileTTF(fullPath.c_str(), baseFontSize);
                        if (font)
                            m_ImGuiFonts[fontName] = font;
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[PresentationCore] Error cargando fuentes: " << e.what() << "\n";
        }
    }

    void PresentationCore::LoadSingleFontIntoImGui(const std::string& fontPath) {
        ImGuiIO& io = ImGui::GetIO();
        const float baseFontSize = 60.0f;

        std::filesystem::path p(fontPath);
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext != ".ttf" && ext != ".otf" && ext != ".ttc") return;

        std::string fontName = p.stem().string();
        if (m_ImGuiFonts.count(fontName)) return;

        ImFont* font = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), baseFontSize);
        if (font)
            m_ImGuiFonts[fontName] = font;
    }

// =============================================================================
//  Helpers privados de disco
// =============================================================================

// Devuelve la ruta al directorio themes/ en AppData — mismo que LayersStyleTab
static std::string ThemesDirPath()
{
    wchar_t buf[MAX_PATH] = {};
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf);
    std::filesystem::path dir =
        std::filesystem::path(buf) / "ProyecThor" / "themes";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir.string();
}

// Lee un archivo .theme del disco y rellena un SavedStyle
static bool LoadThemeFromDisk(const std::string& themesDir,
                               const std::string& name,
                               SavedStyle& out)
{
    std::filesystem::path p =
        std::filesystem::path(themesDir) / (name + ".theme");
    std::ifstream f(p);
    if (!f.is_open()) return false;

    out      = SavedStyle{};
    out.name = name;

    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto sep = line.find('=');
        if (sep == std::string::npos) continue;
        std::string k = line.substr(0, sep);
        std::string v = line.substr(sep + 1);

        if      (k == "textSize")   out.size      = std::stof(v);
        else if (k == "textAlign")  out.hAlign    = std::stoi(v);
        else if (k == "vAlign")     out.vAlign    = std::stoi(v);
        else if (k == "autoScale")  out.autoScale = (std::stoi(v) != 0);
        else if (k == "font")       out.fontName  = v;
        else if (k == "textColor")
            sscanf(v.c_str(), "%f,%f,%f,%f",
                   &out.color[0], &out.color[1],
                   &out.color[2], &out.color[3]);
        else if (k == "margins")
            sscanf(v.c_str(), "%f,%f,%f,%f",
                   &out.margins[0], &out.margins[1],
                   &out.margins[2], &out.margins[3]);
    }
    return true;
}

// =============================================================================
//  SaveStyle — escribe en disco en formato compatible con LayersStyleTab
// =============================================================================
void PresentationCore::SaveStyle(const SavedStyle& style)
{
    std::string dir = ThemesDirPath();
    std::ofstream f(std::filesystem::path(dir) / (style.name + ".theme"));
    if (!f.is_open()) return;

    f << "textColor="     << style.color[0]   << "," << style.color[1]   << ","
                          << style.color[2]   << "," << style.color[3]   << "\n";
    f << "textSize="      << style.size       << "\n";
    f << "textAlign="     << style.hAlign     << "\n";
    f << "vAlign="        << style.vAlign     << "\n";
    f << "margins="       << style.margins[0] << "," << style.margins[1] << ","
                          << style.margins[2] << "," << style.margins[3] << "\n";
    f << "autoScale="     << (style.autoScale ? 1 : 0) << "\n";
    f << "font="          << style.fontName   << "\n";
    // Campos extra para compatibilidad con LayersStyleTab
    f << "refTextSize="    << style.size * 0.46f << "\n";
    f << "verseTextSize="  << style.size         << "\n";
    f << "songTextAlign="  << style.hAlign       << "\n";
    f << "songVAlign="     << style.vAlign       << "\n";
    f << "bibleTextAlign=" << style.hAlign       << "\n";
    f << "bibleVAlign="    << style.vAlign       << "\n";

    // Mantiene el mapa en memoria para acceso rápido durante la sesión
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_SavedStyles[style.name] = style;
}

// =============================================================================
//  DeleteStyle — elimina el archivo .theme del disco
// =============================================================================
void PresentationCore::DeleteStyle(const std::string& name)
{
    std::error_code ec;
    std::filesystem::remove(
        std::filesystem::path(ThemesDirPath()) / (name + ".theme"), ec);

    std::lock_guard<std::mutex> lock(m_Mutex);
    m_SavedStyles.erase(name);
}

// =============================================================================
//  GetSavedStyleNames — lee el directorio themes/ en disco
// =============================================================================
std::vector<std::string> PresentationCore::GetSavedStyleNames() const
{
    std::string dir = ThemesDirPath();
    std::vector<std::string> names;
    try {
        for (const auto& e : std::filesystem::directory_iterator(dir))
            if (e.path().extension() == ".theme")
                names.push_back(e.path().stem().string());
    } catch (...) {}
    std::sort(names.begin(), names.end());
    return names;
}

// =============================================================================
//  GetSavedStyle — lee el archivo .theme del disco
// =============================================================================
bool PresentationCore::GetSavedStyle(const std::string& name, SavedStyle& outStyle) const
{
    // Primero intenta el mapa en memoria (más rápido, válido en la sesión actual)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_SavedStyles.find(name);
        if (it != m_SavedStyles.end()) {
            outStyle = it->second;
            return true;
        }
    }
    // Si no está en memoria (arranque en frío), lee desde disco
    return LoadThemeFromDisk(ThemesDirPath(), name, outStyle);
}

// =============================================================================
//  Helpers privados de estado
// =============================================================================

static std::string CategoryStylesFilePath()
{
    return ProyecThor::GetAssetsPath() + "/../category_styles.ini";
}

static void ApplySavedStyleToState(const SavedStyle& s, PresentationState& state,
                                    std::string& activeFontName)
{
    state.textSize      = s.size;
    state.textAlignment = s.hAlign;
    state.vAlignment    = s.vAlign;
    state.autoScale     = s.autoScale;
    activeFontName      = s.fontName;
    state.selectedFont  = s.fontName;
    for (int i = 0; i < 4; i++) {
        state.textColor[i] = s.color[i];
        state.margins[i]   = s.margins[i];
    }
    state.songTextAlignment  = s.hAlign;
    state.songVAlignment     = s.vAlign;
    state.bibleTextAlignment = s.hAlign;
    state.bibleVAlignment    = s.vAlign;
}

// =============================================================================
//  SetCategoryDefaultStyle
// =============================================================================
void PresentationCore::SetCategoryDefaultStyle(ItemType category, const std::string& styleName)
{
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_CategoryDefaultStyles[static_cast<int>(category)] = styleName;
    }
    SaveCategoryStyles();
}

// =============================================================================
//  GetCategoryDefaultStyle
// =============================================================================
std::string PresentationCore::GetCategoryDefaultStyle(ItemType category) const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    auto it = m_CategoryDefaultStyles.find(static_cast<int>(category));
    if (it != m_CategoryDefaultStyles.end())
        return it->second;
    return {};
}

// =============================================================================
//  LoadCategoryStyles
// =============================================================================
void PresentationCore::LoadCategoryStyles()
{
    std::ifstream f(CategoryStylesFilePath());
    if (!f.is_open()) return;

    std::lock_guard<std::mutex> lock(m_Mutex);
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto sep = line.find('=');
        if (sep == std::string::npos) continue;
        int         key = std::stoi(line.substr(0, sep));
        std::string val = line.substr(sep + 1);
        if (!val.empty())
            m_CategoryDefaultStyles[key] = val;
    }
}

// =============================================================================
//  SaveCategoryStyles
// =============================================================================
void PresentationCore::SaveCategoryStyles() const
{
    std::ofstream f(CategoryStylesFilePath());
    if (!f.is_open()) return;

    std::lock_guard<std::mutex> lock(m_Mutex);
    for (const auto& pair : m_CategoryDefaultStyles) {
        if (!pair.second.empty())
            f << pair.first << "=" << pair.second << "\n";
    }
}

    void PresentationCore::SyncFontListFromDisk(std::vector<std::string>& outList) {
        outList.clear();
        outList.push_back("Predeterminada");

        std::string fontsDir = ProyecThor::GetAssetsPath() + "/fonts";
        try {
            if (std::filesystem::exists(fontsDir)) {
                for (const auto& entry : std::filesystem::directory_iterator(fontsDir)) {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".ttf" || ext == ".otf" || ext == ".ttc")
                        outList.push_back(entry.path().stem().string());
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[PresentationCore] Error sincronizando fuentes: " << e.what() << "\n";
        }
    }

    std::string PresentationCore::GetActiveFontName() const {
        return m_ActiveFontName;
    }

    ImFont* PresentationCore::GetImGuiFont(const std::string& fontName, float /*size*/) {
        auto it = m_ImGuiFonts.find(fontName);
        if (it != m_ImGuiFonts.end())
            return it->second;

        auto def = m_ImGuiFonts.find("Predeterminada");
        if (def != m_ImGuiFonts.end()) return def->second;
        return nullptr;
    }

// =============================================================================
//  SetSelection — aplica el estilo por defecto de la categoría leyendo disco
// =============================================================================
void PresentationCore::SetSelection(const LibrarySelection& selection)
{
    // Primero guardamos la selección bajo el lock
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_CurrentSelection = selection;
    }

    if (selection.type != ItemType::Song && selection.type != ItemType::Bible)
        return;

    // Obtenemos el nombre del estilo por defecto (bajo lock breve)
    std::string styleName;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_CategoryDefaultStyles.find(static_cast<int>(selection.type));
        if (it == m_CategoryDefaultStyles.end() || it->second.empty())
            return;
        styleName = it->second;
    }

    // Cargamos el estilo desde disco (sin lock, puede hacer I/O)
    SavedStyle s;
    if (!GetSavedStyle(styleName, s))
        return;

    // Aplicamos al estado bajo el lock
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_State.textSize      = s.size;
    m_State.textAlignment = s.hAlign;
    m_State.vAlignment    = s.vAlign;
    m_State.autoScale     = s.autoScale;
    m_ActiveFontName      = s.fontName;
    m_State.selectedFont  = s.fontName;
    for (int i = 0; i < 4; i++) {
        m_State.textColor[i] = s.color[i];
        m_State.margins[i]   = s.margins[i];
    }

    if (selection.type == ItemType::Song) {
        m_State.songTextAlignment = s.hAlign;
        m_State.songVAlignment    = s.vAlign;
    } else {
        m_State.bibleTextAlignment = s.hAlign;
        m_State.bibleVAlignment    = s.vAlign;
        m_State.refTextSize        = s.size * 0.46f;
        m_State.verseTextSize      = s.size;
    }
}

// =============================================================================
//  ApplyStyleByName
// =============================================================================
void PresentationCore::ApplyStyleByName(const std::string& styleName)
{
    if (styleName.empty()) return;

    // GetSavedStyle lee desde disco si no está en memoria — sin lock propio
    SavedStyle style;
    if (!GetSavedStyle(styleName, style)) return;

    std::lock_guard<std::mutex> lock(m_Mutex);
    ApplySavedStyleToState(style, m_State, m_ActiveFontName);
    ++m_StreamVersion;
}

// =============================================================================
//  ToggleNetworkStream
// =============================================================================
void PresentationCore::ToggleNetworkStream(bool enable, int port)
{
    if (enable)
    {
        if (m_NetworkServer && m_NetworkServer->IsRunning())
            return;

        m_NetworkServer = std::make_unique<NetworkStreamServer>();

        m_NetworkServer->SetSnapshotProvider([this]() -> StreamSnapshot
        {
            PresentationState st = GetState();

            StreamSnapshot snap;
            snap.isProjecting  = st.isProjecting;

            // ── Prioridad de texto para clientes de RED ────────────────────
            // Si hay una nota "Solo LAN" activa (OClock en modo Solo LAN /
            // Ambos, o cualquier otro panel que use SetLiveQuickNoteLAN),
            // se usa ese texto para el JSON de red en vez de currentText.
            // Esto NO afecta a la pantalla principal/proyector, que sigue
            // leyendo st.currentText/st.showText normalmente vía GetState().
            if (st.showLanQuickNote) {
                snap.currentText = st.lanQuickNoteText;
                snap.showText    = true;
            } else {
                snap.currentText = st.currentText;
                snap.showText    = st.showText;
            }

            snap.textSize      = st.textSize;
            snap.textAlignment = st.textAlignment;
            snap.vAlignment    = st.vAlignment;
            snap.isBgVideo     = (st.bgType == PresentationState::BackgroundType::Video);
            snap.version       = m_StreamVersion.load();
            snap.hasFrame      = m_FrameProviderActive.load();

            for (int i = 0; i < 4; i++) snap.textColor[i] = st.textColor[i];
            for (int i = 0; i < 3; i++) snap.bgColor[i]   = st.bgColor[i];

            return snap;
        });

        m_NetworkServer->SetFrameProvider([this]() -> std::vector<uint8_t>
        {
            std::lock_guard<std::mutex> lk(m_FrameMutex);
            return m_LatestFrame;
        });

        if (!m_NetworkServer->Start(port))
        {
            m_NetworkServer.reset();
            std::cerr << "[NetworkStream] No se pudo iniciar en puerto " << port << ".\n";
            return;
        }

        std::lock_guard<std::mutex> lk(m_Mutex);
        m_State.isStreamingNet = true;
        m_State.networkURL     = m_NetworkServer->GetBaseURL();
    }
    else
    {
        if (m_NetworkServer)
        {
            m_NetworkServer->Stop();
            m_NetworkServer.reset();
        }

        m_FrameProviderActive.store(false);
        {
            std::lock_guard<std::mutex> lk(m_FrameMutex);
            m_LatestFrame.clear();
        }

        std::lock_guard<std::mutex> lk(m_Mutex);
        m_State.isStreamingNet = false;
        m_State.networkURL.clear();
    }
}

// =============================================================================
//  IsStreamingNet
// =============================================================================
bool PresentationCore::IsStreamingNet() const
{
    std::lock_guard<std::mutex> lk(m_Mutex);
    return m_State.isStreamingNet;
}

// =============================================================================
//  FBO helpers
// =============================================================================
void PresentationCore::EnsureFBO(int w, int h)
{
    if (m_FBO != 0 && m_FBOWidth == w && m_FBOHeight == h) return;

    DestroyFBO();

    glGenFramebuffers(1, &m_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

    glGenTextures(1, &m_FBOTex);
    glBindTexture(GL_TEXTURE_2D, m_FBOTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, m_FBOTex, 0);

    glGenRenderbuffers(1, &m_FBORenderBuf);
    glBindRenderbuffer(GL_RENDERBUFFER, m_FBORenderBuf);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, m_FBORenderBuf);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "[FBO] Framebuffer incompleto: " << status << "\n";

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_FBOWidth  = w;
    m_FBOHeight = h;
}

void PresentationCore::DestroyFBO()
{
    if (m_FBO)          { glDeleteFramebuffers(1,  &m_FBO);          m_FBO          = 0; }
    if (m_FBOTex)       { glDeleteTextures(1,       &m_FBOTex);      m_FBOTex       = 0; }
    if (m_FBORenderBuf) { glDeleteRenderbuffers(1,  &m_FBORenderBuf); m_FBORenderBuf = 0; }
    m_FBOWidth  = 0;
    m_FBOHeight = 0;
}

// =============================================================================
//  RenderProjectorToFBO
// =============================================================================
bool PresentationCore::RenderProjectorToFBO(int w, int h, std::vector<uint8_t>& outRGB)
{
    if (w <= 0 || h <= 0) return false;
    if (!m_Impl)          return false;

    // Si no hay nadie transmitiendo, no hacemos absolutamente nada. Esto
    // evita el costo de renderizar + leer pixeles de la GPU cuando la
    // funcion se llama "por si acaso" en cada frame del render principal.
    if (!IsStreamingNet())
        return false;

    // Limitamos la tasa de captura: la transmision en red por MJPEG no
    // necesita ir a la misma tasa de refresco que el render principal
    // (60+ fps). glReadPixels bloquea hasta que el pipeline grafico termina
    // de procesar todo lo pendiente — hacerlo cada frame es la principal
    // causa del consumo excesivo de CPU/GPU al transmitir.
    static constexpr double kMinCaptureIntervalSec = 1.0 / 15.0; // ~15 fps
    double now = glfwGetTime();
    if (now - m_LastFBOCaptureTime < kMinCaptureIntervalSec)
        return false;
    m_LastFBOCaptureTime = now;

    EnsureFBO(w, h);
    if (m_FBO == 0) return false;

    GLint prevFBO         = 0;
    GLint prevViewport[4] = {};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    glGetIntegerv(GL_VIEWPORT,            prevViewport);

    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glViewport(0, 0, w, h);

    {
        std::lock_guard<std::mutex> lk(m_Mutex);
        glClearColor(m_State.bgColor[0], m_State.bgColor[1], m_State.bgColor[2], 1.0f);
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_Impl->background.Render(w, h);
    m_Impl->overlay.Render();

    outRGB.resize(static_cast<size_t>(w) * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, outRGB.data());

    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
    glViewport(prevViewport[0], prevViewport[1],
               prevViewport[2], prevViewport[3]);

    return true;
}

} // namespace ProyecThor::Core