#pragma once
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <imgui.h>

#include "NetworkStreamServer.h"

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

        std::string quickNoteText;
        bool showQuickNote = false;

        // ── Nota rápida SOLO para clientes de red (LAN) ─────────────────────
        // A diferencia de currentText/showText (que dibuja la pantalla
        // principal/proyector Y se replica hacia los clientes de red), este
        // texto NUNCA se dibuja localmente. Solo lo usa el SnapshotProvider
        // de NetworkStreamServer (ver ToggleNetworkStream en el .cpp) para
        // sobreescribir lo que reciben los dispositivos conectados por LAN,
        // sin afectar en absoluto lo que se proyecta en la pantalla principal.
        std::string lanQuickNoteText;
        bool        showLanQuickNote = false;

        // ── Streaming en red local ─────────────────────────────────────────
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

        PresentationCore();
        ~PresentationCore();

        PresentationCore(const PresentationCore&)            = delete;
        PresentationCore& operator=(const PresentationCore&) = delete;

        void Update();

        void RenderBackground(int outputW, int outputH);
        void RenderProjectorWindow();

        PresentationState GetState();
void ApplyStyleByName(const std::string& styleName);
        void  SetStretchToFill(bool stretch);
        bool  GetStretchToFill() const;

        void SetLiveQuickNote(const std::string& text);
        void ClearQuickNote();

        // ── Nota rápida SOLO LAN ─────────────────────────────────────────
        // Igual que SetLiveQuickNote/ClearQuickNote, pero el texto solo
        // llega a los clientes conectados por red (ver PresentationState::
        // lanQuickNoteText). No modifica currentText/showText/isProjecting,
        // por lo que la pantalla principal/proyector no se ve afectada.
        void SetLiveQuickNoteLAN(const std::string& text);
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

        void SetBackgroundMedia(const std::string& path, bool isVideo);
        void StopBackgroundMedia();

        // Bloquea/desbloquea la ruta de fondo actual contra reproduccion.
        // Ver comentarios de VLCBasePlayer::BlockPath / UnblockPath. Se usa
        // para poder eliminar del disco el archivo de video de fondo sin
        // que quede en riesgo de que algo (cola automatica, boton manual,
        // etc.) lo vuelva a abrir mientras se procesa el borrado.
        void BlockBackgroundPath(const std::string& path);
        void UnblockBackgroundPath();

        void SetLayer0_Color(float r, float g, float b);

        void SetOverlayMedia(const std::string& path);
        void StopOverlayMedia();
        void SetLayer2_Text(const std::string& text);
        void ClearLayer2();
        

        // ── Preview independiente del video en vivo ─────────────────────────
        // Reproductor completamente aislado del que maneja el fondo en vivo
        // (m_Impl->background). Pensado para que los paneles de biblioteca
        // puedan mostrar una vista previa de cualquier video, o recorrer la
        // lista rapidamente, SIN tocar en absoluto el video que esta
        // proyectandose en ese momento. Al ser un VLCBasePlayer distinto,
        // tiene su propio libvlc_media_player_t, su propio dispositivo de
        // audio nativo y su propia textura de OpenGL: cargar o descargar
        // clips aqui no puede interrumpir ni recargar el video en vivo.
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
        void        CreateProjectorWindow();
        void        DestroyProjectorWindow();
        GLFWwindow* GetProjectorWindow() const;

        float GetLivePosition();
        void  SetLivePosition(float pos);
        int   GetLiveVolume();
        void  SetLiveVolume(int volume);

        void        LoadFontsIntoImGui();
        void        LoadSingleFontIntoImGui(const std::string& fontPath);
        void        SyncFontListFromDisk(std::vector<std::string>& outList);
        std::string GetActiveFontName() const;
        ImFont*     GetImGuiFont(const std::string& fontName, float size = 0.0f);

        // Resuelve la ruta absoluta en disco (assets/fonts/<nombre>.ttf|otf|ttc)
        // para un nombre de fuente. Devuelve "" para "Predeterminada" o si no
        // se encuentra ningun archivo con ese nombre. Usado por el provider de
        // fuente del streaming LAN (ver ToggleNetworkStream) para poder servir
        // el archivo real vía HTTP y que el cliente web use la MISMA fuente
        // que el usuario eligio, en vez de una fuente generica del sistema.
        std::string GetActiveFontFilePath() const;

        void                     SaveStyle(const SavedStyle& style);
        void                     DeleteStyle(const std::string& name);
        std::vector<std::string> GetSavedStyleNames() const;
        bool                     GetSavedStyle(const std::string& name, SavedStyle& outStyle) const;

        // ── Estilos predeterminados por categoria ──────────────────────────
        void        SetCategoryDefaultStyle(ItemType category, const std::string& styleName);
        std::string GetCategoryDefaultStyle(ItemType category) const;
        void        LoadCategoryStyles();
        void        SaveCategoryStyles() const;

        // ── Streaming en red local ─────────────────────────────────────────
        void ToggleNetworkStream(bool enable, int port = 8080);
        bool IsStreamingNet() const;
bool RenderProjectorToFBO(int w, int h, std::vector<uint8_t>& outRGB);
        // Acceso al servidor para que StreamingPanel pueda cambiar la config
        NetworkStreamServer* GetNetworkServer() { return m_NetworkServer.get(); }

        // Llamado por StreamingPanel cada frame con el JPEG capturado
        void PushFrame(std::vector<uint8_t> jpegData)
        {
            {
                std::lock_guard<std::mutex> lk(m_FrameMutex);
                m_LatestFrame = std::move(jpegData);
            }
            m_FrameProviderActive.store(true);
            ++m_StreamVersion;
        }

    private:
               void RenderDefaultStyleCombo();
        void EnsureFBO(int w, int h);
        void DestroyFBO();

        // Helper interno de disco: busca <assets>/fonts/<fontName>.{ttf,otf,ttc}
        // y devuelve la ruta completa si existe, o "" en caso contrario.
        std::string ResolveFontFilePath(const std::string& fontName) const;

        unsigned int m_FBO          = 0;
        unsigned int m_FBOTex       = 0;
        unsigned int m_FBORenderBuf = 0;
        int          m_FBOWidth     = 0;
        int          m_FBOHeight    = 0;
         double       m_LastFBOCaptureTime = 0.0;

        bool m_stretchToFill = false;
        std::unique_ptr<PresentationCoreImpl> m_Impl;
        mutable std::mutex m_Mutex;

        PresentationState m_State;
        LibrarySelection  m_CurrentSelection;

        int         m_ProjectorWidth  = 1920;
        int         m_ProjectorHeight = 1080;
        GLFWwindow* m_ProjectorWindow = nullptr;
        std::string m_ActiveFontName  = "Predeterminada";

        std::unordered_map<std::string, ImFont*>      m_ImGuiFonts;
        std::unordered_map<std::string, SavedStyle>   m_SavedStyles;
        std::unordered_map<int, std::string>          m_CategoryDefaultStyles;

        // ── Streaming en red local ─────────────────────────────────────────
        std::unique_ptr<NetworkStreamServer> m_NetworkServer;
        std::atomic<uint64_t>                m_StreamVersion { 0 };

        // Frame compartido: StreamingPanel escribe, FrameProvider lambda lee
        mutable std::mutex    m_FrameMutex;
        std::vector<uint8_t>  m_LatestFrame;
        std::atomic<bool>     m_FrameProviderActive { false };
    };

} // namespace ProyecThor::Core