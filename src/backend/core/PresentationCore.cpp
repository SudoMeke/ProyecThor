#include "PresentationCore.h"
#include "BackgroundLayer.h"
#include "OverlayLayer.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include "backend/settings/SettingsManager.h"
#include <filesystem>
#include <algorithm>
#include "AppPaths.h"
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif
#include "NetworkStreamServer.h"

namespace ProyecThor::Core {

   class PresentationCoreImpl {
    public:
        // background: layer de fondo/decorativo. Por requisito de
        // producto NUNCA debe emitir audio real, sin importar el estado
        // de m_IsLiveToPublic, m_TargetMuted, ni ninguna llamada a
        // SetLiveVolume/SetLiveMute. Se construye forceSilent=true por la
        // misma razon que preview: es una garantia estructural dentro de
        // VLCBasePlayer (ver m_ForceSilent), no una convencion que
        // dependa de que el resto del codigo se comporte bien.
        BackgroundLayer background{ false };
        OverlayLayer    overlay;

        // preview: instancia separada usada por los paneles de biblioteca
        // para scrubbing/preview. Se construye forceSilentAudio=true, asi
        // que estructuralmente NUNCA puede sonar, sin importar que boton
        // de UI la toque (ver VLCBasePlayer::m_ForceSilent).
        BackgroundLayer preview{ true };
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

    void PresentationCore::SetLiveQuickNote(const std::string& text, const float* /*colorOverride*/) {
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

    void PresentationCore::SetLiveQuickNoteLAN(const std::string& text, const float* /*colorOverride*/) {
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
        if (m_Impl) m_Impl->preview.SetSolidColor(0.0f, 0.0f, 0.0f);
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

  // .cpp
void PresentationCore::SetBackgroundMedia(const std::string& path, bool /*isVideo*/, bool allowAudio) {
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.bgPath = path;
        m_State.bgType = PresentationState::BackgroundType::Video;
        ++m_State.transitionTrigger;   // NUEVO
        ++m_StreamVersion;
    }
    if (m_Impl)
        m_Impl->background.SetVideo(path, allowAudio);
}

void PresentationCore::StopBackgroundMedia() {
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.bgPath     = "";
        m_State.bgType     = PresentationState::BackgroundType::SolidColor;
        m_State.bgColor[0] = 0.0f; m_State.bgColor[1] = 0.0f; m_State.bgColor[2] = 0.0f;
        ++m_State.transitionTrigger;   // NUEVO
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
        m_State.bgColor[0] = r; m_State.bgColor[1] = g; m_State.bgColor[2] = b;
        m_State.bgType     = PresentationState::BackgroundType::SolidColor;
        m_State.bgPath     = "";
        ++m_State.transitionTrigger;   // NUEVO
        ++m_StreamVersion;
    }
    if (m_Impl) m_Impl->background.SetSolidColor(r, g, b);
}
void PresentationCore::SetOverlayMedia(const std::string& path) {
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.overlayPath = path;
        ++m_State.transitionTrigger;   // NUEVO
    }
    if (m_Impl) m_Impl->overlay.PlayOverlay(path);
}
void PresentationCore::SetBackgroundTransitionProgress(float progress) {
    if (m_Impl) m_Impl->background.SetTransitionProgress(progress);
}
void PresentationCore::StopOverlayMedia() {
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.overlayPath = "";
        ++m_State.transitionTrigger;   // NUEVO
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
    ++m_State.transitionTrigger;   // NUEVO
    ++m_StreamVersion;
}

void PresentationCore::ClearLayer2() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_State.currentText = "";
    m_State.showText    = false;
    ++m_State.transitionTrigger;   // NUEVO
    ++m_StreamVersion;
}

    void PresentationCore::SetProjecting(bool projecting) {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_State.isProjecting = projecting;
            ++m_StreamVersion;
        }

        // Unico punto que habilita/corta el audio real hacia el publico.
        // Fuera del lock: BackgroundLayer solo toca atomicos de los
        // players, no hace falta serializarlo con m_State.
        if (m_Impl)
            m_Impl->background.SetPubliclyLive(projecting);
    }

    bool PresentationCore::IsProjecting() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_State.isProjecting;
    }

    void PresentationCore::SetTargetMonitor(int index) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.targetMonitorIndex = index;
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
        if (m_Impl)
            m_Impl->background.SetLiveVolume(volume);
    }

    void PresentationCore::SetLiveMute(bool mute) {
        if (m_Impl)
            m_Impl->background.SetLiveMute(mute);
    }
void PresentationCore::SetTransitionConfig(int type, float durationSeconds) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_State.transitionType     = type;
    m_State.transitionDuration = std::max(0.05f, durationSeconds);
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

    // -------------------------------------------------------------------------
    //  ThemesDirPath — multiplataforma.
    //  En Windows usa la carpeta AppData del usuario (via SHGetFolderPathW).
    //  En Linux sigue la convencion XDG: usa $XDG_CONFIG_HOME si esta definida,
    //  o $HOME/.config en caso contrario.
    // -------------------------------------------------------------------------
    static std::string ThemesDirPath()
    {
        std::filesystem::path dir;

#ifdef _WIN32
        wchar_t buf[MAX_PATH] = {};
        SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf);
        dir = std::filesystem::path(buf) / "ProyecThor" / "themes";
#else
        const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
        std::filesystem::path base;
        if (xdgConfig && *xdgConfig)
        {
            base = std::filesystem::path(xdgConfig);
        }
        else
        {
            const char* home = std::getenv("HOME");
            base = std::filesystem::path(home ? home : ".") / ".config";
        }
        dir = base / "ProyecThor" / "themes";
#endif

        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return dir.string();
    }

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
        f << "refTextSize="    << style.size * 0.46f << "\n";
        f << "verseTextSize="  << style.size         << "\n";
        f << "songTextAlign="  << style.hAlign       << "\n";
        f << "songVAlign="     << style.vAlign       << "\n";
        f << "bibleTextAlign=" << style.hAlign       << "\n";
        f << "bibleVAlign="    << style.vAlign       << "\n";

        std::lock_guard<std::mutex> lock(m_Mutex);
        m_SavedStyles[style.name] = style;
    }

    void PresentationCore::DeleteStyle(const std::string& name)
    {
        std::error_code ec;
        std::filesystem::remove(
            std::filesystem::path(ThemesDirPath()) / (name + ".theme"), ec);

        std::lock_guard<std::mutex> lock(m_Mutex);
        m_SavedStyles.erase(name);
    }

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

    bool PresentationCore::GetSavedStyle(const std::string& name, SavedStyle& outStyle) const
    {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            auto it = m_SavedStyles.find(name);
            if (it != m_SavedStyles.end()) {
                outStyle = it->second;
                return true;
            }
        }
        return LoadThemeFromDisk(ThemesDirPath(), name, outStyle);
    }

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

    void PresentationCore::SetCategoryDefaultStyle(ItemType category, const std::string& styleName)
    {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_CategoryDefaultStyles[static_cast<int>(category)] = styleName;
        }
        SaveCategoryStyles();
    }

    std::string PresentationCore::GetCategoryDefaultStyle(ItemType category) const
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_CategoryDefaultStyles.find(static_cast<int>(category));
        if (it != m_CategoryDefaultStyles.end())
            return it->second;
        return {};
    }

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

    std::string PresentationCore::ResolveFontFilePath(const std::string& fontName) const
    {
        if (fontName.empty() || fontName == "Predeterminada") return "";

        std::string fontsDir = ProyecThor::GetAssetsPath() + "/fonts";
        for (const char* ext : { ".ttf", ".otf", ".ttc" }) {
            std::filesystem::path candidate =
                std::filesystem::path(fontsDir) / (fontName + ext);
            std::error_code ec;
            if (std::filesystem::exists(candidate, ec))
                return candidate.string();
        }
        return "";
    }

    std::string PresentationCore::GetActiveFontFilePath() const
    {
        std::string fontName;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            fontName = m_ActiveFontName;
        }
        return ResolveFontFilePath(fontName);
    }

    void PresentationCore::SetSelection(const LibrarySelection& selection)
    {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_CurrentSelection = selection;
        }

        if (selection.type != ItemType::Song && selection.type != ItemType::Bible)
            return;

        std::string styleName;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            auto it = m_CategoryDefaultStyles.find(static_cast<int>(selection.type));
            if (it == m_CategoryDefaultStyles.end() || it->second.empty())
                return;
            styleName = it->second;
        }

        SavedStyle s;
        if (!GetSavedStyle(styleName, s))
            return;

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

    void PresentationCore::ApplyStyleByName(const std::string& styleName)
    {
        if (styleName.empty()) return;

        SavedStyle style;
        if (!GetSavedStyle(styleName, style)) return;

        std::lock_guard<std::mutex> lock(m_Mutex);
        ApplySavedStyleToState(style, m_State, m_ActiveFontName);
        ++m_StreamVersion;
    }

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
                // El cliente web usa "isProjecting" solo para decidir si oculta el overlay
// de idle y muestra el texto. No debe confundirse con el "isProjecting"
// real que controla el proyector principal y el audio publico — por eso
// aqui se OR-ea con showLanQuickNote: si hay una nota SOLO-LAN activa,
// el cliente de red debe mostrarla aunque la pantalla principal este idle.
snap.isProjecting  = st.isProjecting || st.showLanQuickNote;

                if (st.showLanQuickNote) {
                    snap.currentText = st.lanQuickNoteText;
                    snap.showText    = true;
                } else {
                    snap.currentText = st.currentText;
                    snap.showText    = st.showText;
                }

                snap.textSize      = st.textSize;
                snap.textAlignment = st.textAlignment;
                snap.transitionTrigger  = st.transitionTrigger;
  snap.transitionType     = st.transitionType;
  snap.transitionDuration = st.transitionDuration;
                snap.vAlignment    = st.vAlignment;
                snap.autoScale     = st.autoScale;
                snap.isBgVideo     = (st.bgType == PresentationState::BackgroundType::Video);
                snap.version       = m_StreamVersion.load();
                snap.hasFrame      = m_FrameProviderActive.load();

                snap.refW = m_ProjectorWidth;
                snap.refH = m_ProjectorHeight;

                for (int i = 0; i < 4; i++) snap.margins[i] = st.margins[i];

                {
                    std::lock_guard<std::mutex> lock(m_Mutex);
                    snap.fontFamily = m_ActiveFontName;
                }
                snap.fontVersion = std::hash<std::string>{}(snap.fontFamily);

                for (int i = 0; i < 4; i++) snap.textColor[i] = st.textColor[i];
                for (int i = 0; i < 3; i++) snap.bgColor[i]   = st.bgColor[i];

                return snap;
            });

            m_NetworkServer->SetFrameProvider([this]() -> std::vector<uint8_t>
            {
                std::lock_guard<std::mutex> lk(m_FrameMutex);
                return m_LatestFrame;
            });

            m_NetworkServer->SetFontPathProvider([this]() -> std::string
            {
                return GetActiveFontFilePath();
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

    bool PresentationCore::IsStreamingNet() const
    {
        std::lock_guard<std::mutex> lk(m_Mutex);
        return m_State.isStreamingNet;
    }

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

        glGenBuffers(2, m_PBO);
        for (int i = 0; i < 2; i++)
        {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, m_PBO[i]);
            glBufferData(GL_PIXEL_PACK_BUFFER,
                         static_cast<GLsizeiptr>(w) * h * 3,
                         nullptr, GL_STREAM_READ);
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        m_PBOIndex = 0;

        m_FBOWidth  = w;
        m_FBOHeight = h;
    }

    void PresentationCore::DestroyFBO()
    {
        if (m_FBO)          { glDeleteFramebuffers(1,  &m_FBO);          m_FBO          = 0; }
        if (m_FBOTex)       { glDeleteTextures(1,       &m_FBOTex);      m_FBOTex       = 0; }
        if (m_FBORenderBuf) { glDeleteRenderbuffers(1,  &m_FBORenderBuf); m_FBORenderBuf = 0; }
        if (m_PBO[0] || m_PBO[1])
        {
            glDeleteBuffers(2, m_PBO);
            m_PBO[0] = m_PBO[1] = 0;
        }
        m_FBOWidth  = 0;
        m_FBOHeight = 0;
    }

    bool PresentationCore::RenderProjectorToFBO(int w, int h, std::vector<uint8_t>& outRGB)
    {
        if (w <= 0 || h <= 0) return false;
        if (!m_Impl)          return false;
        if (!IsStreamingNet()) return false;

        static constexpr double kMinCaptureIntervalSec = 1.0 / 15.0;
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

        int nextIndex = (m_PBOIndex + 1) % 2;

        glBindBuffer(GL_PIXEL_PACK_BUFFER, m_PBO[m_PBOIndex]);
        glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, 0);

        glBindBuffer(GL_PIXEL_PACK_BUFFER, m_PBO[nextIndex]);
        GLubyte* ptr = static_cast<GLubyte*>(
            glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY));
        if (ptr)
        {
            std::memcpy(outRGB.data(), ptr, static_cast<size_t>(w) * h * 3);
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

        m_PBOIndex = nextIndex;

        glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
        glViewport(prevViewport[0], prevViewport[1],
                   prevViewport[2], prevViewport[3]);

        return true;
    }

} // namespace ProyecThor::Core