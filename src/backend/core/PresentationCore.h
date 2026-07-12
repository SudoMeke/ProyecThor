#pragma once
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <imgui.h>

#include "NetworkStreamServer.h"
#include "frontend/windowing/SecondaryOutputWindow.h"

struct GLFWwindow;

namespace ProyecThor::Core {

    class VLCBasePlayer;

    enum class ItemType { None = -1, Video = 0, Image = 1, Song = 2, Bible = 3, Documents = 4, Audio = 5 };

    struct LibrarySelection {
        std::string title;
        ItemType type = ItemType::None;
        std::vector<std::string> contentData;
    };

    struct SavedStyle {
        std::string name;
        float       size          = 60.0f;
        float       color[4]      = { 1.0f, 1.0f, 1.0f, 1.0f };
        int         hAlign        = 1;
        int         vAlign        = 1;
        float       margins[4]    = { 50.0f, 50.0f, 50.0f, 50.0f };
        bool        autoScale     = true;
        std::string fontName      = "Predeterminada";
    };

    struct PresentationState {
        enum class BackgroundType { SolidColor, Video };
        BackgroundType bgType = BackgroundType::SolidColor;
        std::string bgPath;
        float bgColor[3] = { 0.0f, 0.0f, 0.0f };

        std::string overlayPath;
        bool isProjecting       = false;
        int  targetMonitorIndex = 0;

        std::string currentText;
        bool  showText          = false;

        uint64_t transitionTrigger  = 0;
        int      transitionType     = 0;
        float    transitionDuration = 1.0f;

        float textSize          = 60.0f;
        float textColor[4]      = { 1.0f, 1.0f, 1.0f, 1.0f };
        int   textAlignment     = 1;
        int   vAlignment        = 1;
        float margins[4]        = { 50.0f, 50.0f, 50.0f, 50.0f };
        bool  autoScale         = true;
        std::string selectedFont = "Predeterminada";

        float refTextSize   = 28.0f;
        float verseTextSize = 60.0f;

        int songTextAlignment  = 1;
        int songVAlignment     = 1;
        int bibleTextAlignment = 1;
        int bibleVAlignment    = 1;

        float livePosition      = 0.0f;
        int   liveVolume        = 100;
        bool  liveMuted         = false;

        std::string quickNoteText;
        bool showQuickNote = false;

        std::string lanQuickNoteText;
        bool        showLanQuickNote = false;

        bool        isStreamingNet = false;
        std::string networkURL;
    };

    class PresentationCoreImpl;

    class PresentationCore {
    public:
        static PresentationCore& Get() {
            static PresentationCore instance;
            return instance;
        }
void SetGlobalMute(bool mute);
    bool GetGlobalMute() const;
        PresentationCore();
        ~PresentationCore();

        PresentationCore(const PresentationCore&)            = delete;
        PresentationCore& operator=(const PresentationCore&) = delete;

        void Update();

        void RenderBackground(int outputW, int outputH);
        void RenderProjectorWindow(); // dibuja background+overlay (contenido, no la ventana en si)
        PresentationState GetState();
        void ApplyStyleByName(const std::string& styleName);
        void  SetStretchToFill(bool stretch);
        bool  GetStretchToFill() const;

        void ClearQuickNote();

        void SetTransitionConfig(int type, float durationSeconds);
        void SetBackgroundTransitionProgress(float progress);

        void SetLiveQuickNote(const std::string& text, const float* colorOverride = nullptr);
        void SetLiveQuickNoteLAN(const std::string& text, const float* colorOverride = nullptr);
        void ClearQuickNoteLAN();

        void*          GetBackgroundTexture();
        void*          GetProcessedBackgroundTexture(int targetW, int targetH);
        void*          GetOverlayTexture();
        VLCBasePlayer* GetBackgroundPlayer();
        VLCBasePlayer* GetOverlayPlayer();

        void  SetFSREnabled(bool enabled);
        bool  GetFSREnabled() const;
        void  SetFSRSharpness(float sharpness);
        float GetFSRSharpness() const;

        void             SetSelection(const LibrarySelection& selection);
        LibrarySelection GetSelection();
        LibrarySelection PeekSelection();

        void StopBackgroundMedia();
        void BlockBackgroundPath(const std::string& path);
        void UnblockBackgroundPath();

        void SetLayer0_Color(float r, float g, float b);

        void SetOverlayMedia(const std::string& path);
        void StopOverlayMedia();
        void SetLayer2_Text(const std::string& text);
        void ClearLayer2();

        void*          GetPreviewTexture();
        VLCBasePlayer* GetPreviewPlayer();
        void           SetPreviewMedia(const std::string& path);
        void           StopPreviewMedia();

        void UpdateTextStyle(float size, const float color[4], int align,
                             int vAlign, const float margins[4], bool autoScale,
                             const std::string& font);

        void UpdateBibleStyle(float refSize, float verseSize, int hAlign, int vAlign);
        void UpdateSongStyle(int hAlign, int vAlign);

        void        SetProjecting(bool projecting);
        bool        IsProjecting() const;
        void        SetTargetMonitor(int index);
        void        SetProjectorSize(int w, int h);

        // ── Ventana principal ────────────────────────────────────────────
        // Necesaria para poder crear ventanas secundarias con contexto GL
        // compartido (texturas/shaders/buffers; VAO/FBO no se comparten,
        // ver BackgroundLayer.cpp). Se setea una vez desde main() apenas
        // se crea la ventana principal.
        void SetMainWindow(GLFWwindow* mainWindow) { m_MainWindow = mainWindow; }

        // ── Ventanas secundarias, API generica ──────────────────────────
        // Cualquier salida adicional (proyector, stage, un segundo stage
        // en otro monitor a futuro, etc.) se identifica por un id de
        // string unico. Agregar una N-esima ventana de salida en el
        // futuro (multi-monitor) es simplemente otro llamado a esto con
        // un id nuevo, no hay que tocar la clase.
        bool CreateSecondaryWindow(const std::string& id, int monitorIndex,
                                    const std::string& title,
                                    SecondaryOutputWindow::RenderFn renderFn);
        void DestroySecondaryWindow(const std::string& id);
        void DestroyAllSecondaryWindows();
        bool IsSecondaryWindowActive(const std::string& id) const;
        int  GetSecondaryWindowMonitor(const std::string& id) const; // -1 si no existe/inactiva

        // Llamar UNA VEZ POR FRAME desde main(), DESPUES de core.Update(),
        // para refrescar todas las ventanas secundarias activas.
        void RenderAllSecondaryWindows();

        // ── Atajos con nombre fijo para los casos conocidos hoy ─────────
        bool        CreateProjectorWindow(int monitorIndex);
        void        DestroyProjectorWindow();
        bool        IsProjectorWindowActive() const;
        GLFWwindow* GetProjectorWindow() const;

        bool CreateStageWindow(int monitorIndex);
        void DestroyStageWindow();
        bool IsStageWindowActive() const;

        float GetLivePosition();
        void  SetLivePosition(float pos);
        int   GetLiveVolume();
        bool  GetLiveMute();
        void  SetLiveVolume(int volume);
        void  SetLiveMute(bool mute);

        void        LoadFontsIntoImGui();
        void        LoadSingleFontIntoImGui(const std::string& fontPath);
        void        SyncFontListFromDisk(std::vector<std::string>& outList);
        std::string GetActiveFontName() const;
        ImFont*     GetImGuiFont(const std::string& fontName, float size = 0.0f);
        std::string GetActiveFontFilePath() const;

        void                     SaveStyle(const SavedStyle& style);
        void                     DeleteStyle(const std::string& name);
        std::vector<std::string> GetSavedStyleNames() const;
        bool                     GetSavedStyle(const std::string& name, SavedStyle& outStyle) const;

        void        SetCategoryDefaultStyle(ItemType category, const std::string& styleName);
        std::string GetCategoryDefaultStyle(ItemType category) const;
        void        LoadCategoryStyles();
        void        SaveCategoryStyles() const;

        void ToggleNetworkStream(bool enable, int port = 8080);
        bool IsStreamingNet() const;
        bool RenderProjectorToFBO(int w, int h, std::vector<uint8_t>& outRGB);
        NetworkStreamServer* GetNetworkServer() { return m_NetworkServer.get(); }

       void PushFrame(std::vector<uint8_t> jpegData)
{
    // Un jpegData vacio significa que no hay frame real disponible
    // (fallo de captura/compresion). En ese caso hasFrame debe quedar
    // en false para que el cliente muestre el color solido de fondo
    // en vez de un JPEG corrupto. Si trae datos, hasFrame pasa a true.
    bool hasRealFrame = !jpegData.empty();

    {
        std::lock_guard<std::mutex> lk(m_FrameMutex);
        m_LatestFrame = std::move(jpegData);
    }
    m_FrameProviderActive.store(hasRealFrame);
    ++m_StreamVersion;
}

        void SetBackgroundMedia(const std::string& path, bool isVideo, bool allowAudio = true);

    private:
        void RenderDefaultStyleCombo();
        void EnsureFBO(int w, int h);
        void DestroyFBO();
        void RenderStageContent(int w, int h); // contenido visual del Stage (siguiente entrega)

        std::string ResolveFontFilePath(const std::string& fontName) const;
bool m_GlobalMuted = false;
        unsigned int m_FBO          = 0;
        unsigned int m_FBOTex       = 0;
        unsigned int m_FBORenderBuf = 0;
        int          m_FBOWidth     = 0;
        int          m_FBOHeight    = 0;
        double       m_LastFBOCaptureTime = 0.0;

        unsigned int m_PBO[2] = { 0, 0 };
        int          m_PBOIndex = 0;

        bool m_stretchToFill = false;
        std::unique_ptr<PresentationCoreImpl> m_Impl;
        mutable std::mutex m_Mutex;

        PresentationState m_State;
        LibrarySelection  m_CurrentSelection;

        int         m_ProjectorWidth  = 1920;
        int         m_ProjectorHeight = 1080;
        std::string m_ActiveFontName  = "Predeterminada";
        std::unordered_map<std::string, ImFont*>      m_ImGuiFonts;
        std::unordered_map<std::string, SavedStyle>   m_SavedStyles;
        std::unordered_map<int, std::string>          m_CategoryDefaultStyles;

        // ── Ventanas secundarias ─────────────────────────────────────────
        GLFWwindow* m_MainWindow = nullptr;

        struct SecondaryOutput {
            SecondaryOutputWindow           window;
            SecondaryOutputWindow::RenderFn renderFn;
        };
        std::unordered_map<std::string, SecondaryOutput> m_SecondaryWindows;
        mutable std::mutex m_SecondaryWindowsMutex;

        static constexpr const char* kProjectorId = "projector";
        static constexpr const char* kStageId     = "stage";

        // ── Streaming en red local ───────────────────────────────────────
        std::unique_ptr<NetworkStreamServer> m_NetworkServer;
        std::atomic<uint64_t>                m_StreamVersion { 0 };

        mutable std::mutex    m_FrameMutex;
        std::vector<uint8_t>  m_LatestFrame;
        std::atomic<bool>     m_FrameProviderActive { false };
    };

} // namespace ProyecThor::Core