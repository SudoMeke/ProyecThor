#pragma once
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <imgui.h>

#include "NetworkStreamServer.h"
#include "ChatMessageStore.h"
#include "frontend/windowing/SecondaryOutputWindow.h"
#include "MacroTypes.h"

struct GLFWwindow;

namespace ProyecThor::UI {
    class AudioPanel;
    class Announcements;
    class OClock;
    class CapturePanel;
}

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
        // Audio: "now playing" (disco + caratula + ondas) — ver
        // PresentationCore::SetBackgroundAudio() y AudioPanel::RenderLiveBackground().
        enum class BackgroundType { SolidColor, Video, Audio };
        BackgroundType bgType = BackgroundType::SolidColor;
        std::string bgPath;
        float bgColor[3] = { 0.0f, 0.0f, 0.0f };

        std::string overlayPath;
        bool isProjecting       = false;
        int  targetMonitorIndex = 0;

        // Monitor de Control (Stage Display). Independiente de isProjecting:
        // el Stage puede estar activo con o sin proyeccion publica.
        bool isStaging          = false;
        int  stageMonitorIndex  = 0;

        std::string currentText;
        bool  showText          = false;

        // Texto que vendra despues del actual (siguiente estrofa/versiculo),
        // solo para el Stage Display — nunca se muestra al publico.
        std::string nextText;

        // Fondo/video (Layer0) UNICAMENTE — el crossfade de BackgroundLayer
        // es propio y automatico (ver BackgroundLayer::Update/Render), no
        // depende de esto para nada visual. Se mantiene solo por si algo
        // externo (ej. LAN) necesita saber que el fondo cambio.
        uint64_t transitionTrigger  = 0;

        // Letras (Layer2) UNICAMENTE — es el unico disparador real de
        // TransitionPanel (ver UIManager::RenderAll). Separado de
        // transitionTrigger para que un cambio de fondo/video NUNCA anime
        // el texto, ni al reves (antes compartian un solo contador).
        uint64_t textTransitionTrigger = 0;

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
        bool  liveLoop          = false;

        std::string quickNoteText;
        bool showQuickNote = false;

        std::string lanQuickNoteText;
        bool        showLanQuickNote = false;

        bool        isStreamingNet = false;
        std::string networkURL;

        bool        isChatRunning = false;
        std::string chatURL;
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

        // Igual que GetProcessedBackgroundTexture, pero además corre los
        // mismos efectos de CompositePostChain (CRT/Grano/FXAA/Saturación/
        // Viñetado) a la resolución del preview -- para que el recuadro del
        // operador (ViewPanel/Monitor de Control) sea un reflejo fiel de lo
        // que ve el público, en vez de mostrar siempre el fondo sin
        // procesar (esos efectos antes SOLO corrian sobre la viewport real
        // "ProjectorLive"). Ver CompositePostChain::ProcessBackgroundForPreview.
        void*          GetPreviewBackgroundTexture(int targetW, int targetH);

        // "Rellenado" de letterbox/pillarbox: version muy desenfocada del
        // fondo, a pantalla completa, para dibujar DETRAS del contenido
        // nitido en vez de barras negras. nullptr si esta desactivado.
        void*          GetBackgroundFillTexture(int workW, int workH);
        void           SetFillBlurEnabled(bool enabled);
        bool           GetFillBlurEnabled() const;
        void           SetFillBlurBrightness(float v);
        float          GetFillBlurBrightness() const;

        void*          GetOverlayTexture();
        bool           IsOverlayActive() const;
        VLCBasePlayer* GetBackgroundPlayer();
        VLCBasePlayer* GetOverlayPlayer();

        void  SetFSREnabled(bool enabled);
        bool  GetFSREnabled() const;
        void  SetFSRSharpness(float sharpness);
        float GetFSRSharpness() const;

        // Motor de renderizado del fondo de video (Ajustes > Proyeccion):
        // 0 = OpenGL compuesto (default, con overlays/texto encima), 1 =
        // VLC en ventana nativa (sin overlays/texto, ver BackgroundLayer::
        // SetUseNativeEngine para el detalle completo de las limitaciones
        // de este modo).
        void SetVideoRenderEngine(int engine);
        int  GetVideoRenderEngine() const;

        // ── Post-proceso del composite completo de "ProjectorLive" (fondo +
        //    overlays + texto + anuncios + captura) — ver CompositePostChain.h
        //    para la arquitectura. A diferencia de FSR (arriba), estos no
        //    hacen upscale: son filtros a resolucion de salida.
        void  SetCRTEnabled(bool enabled);
        bool  GetCRTEnabled() const;
        void  SetCRTScanlineIntensity(float intensity);
        float GetCRTScanlineIntensity() const;

        void  SetGrainEnabled(bool enabled);
        bool  GetGrainEnabled() const;
        void  SetGrainIntensity(float intensity);
        float GetGrainIntensity() const;

        void  SetFXAAEnabled(bool enabled);
        bool  GetFXAAEnabled() const;

        void  SetSaturationEnabled(bool enabled);
        bool  GetSaturationEnabled() const;
        void  SetSaturationAmount(float amount);
        float GetSaturationAmount() const;

        void  SetVignetteEnabled(bool enabled);
        bool  GetVignetteEnabled() const;
        void  SetVignetteIntensity(float intensity);
        float GetVignetteIntensity() const;

        // Usado por UIManager (justo tras ImGui::Begin("ProjectorLive",...))
        // para informar, cada frame, cual ImGuiID es esa viewport, y por el
        // override de Renderer_RenderWindow en main.cpp para preguntar si el
        // viewport que esta por dibujarse es esa (y no "StageLive" ni un
        // panel flotante cualquiera) antes de desviar su render hacia
        // RenderProjectorViewportPostFX.
        void   SetProjectorPostFXViewportID(ImGuiID id);
        bool   IsProjectorPostFXViewport(ImGuiID id) const;
        void   RenderProjectorViewportPostFX(ImGuiViewport* viewport,
                                              void (*defaultRenderFn)(ImGuiViewport*, void*));

        // fromQueue=true: la seleccion viene de MonitorQueueEngine::PlayIndex
        // (solo para mostrar el titulo del item actual de la cola), NO de un
        // click manual del operador en la Biblioteca. MonitorView/MediaView
        // usan IsSelectionFromQueue() para NO disparar su propio "cargar en
        // preview" en ese caso — sin esto, cada avance de la cola hacia que
        // el reproductor de Preview intentara abrir el MISMO archivo que ya
        // esta en vivo (o precargandose en standby), dos instancias de VLC
        // abriendo el mismo archivo a la vez, lo que crasheaba en Windows.
        void             SetSelection(const LibrarySelection& selection, bool fromQueue = false);
        LibrarySelection GetSelection();
        LibrarySelection PeekSelection();
        bool             IsSelectionFromQueue() const { return m_SelectionFromQueue; }

        void StopBackgroundMedia();
        void BlockBackgroundPath(const std::string& path);
        void UnblockBackgroundPath();

        void SetLayer0_Color(float r, float g, float b);

        void SetOverlayMedia(const std::string& path);
        void StopOverlayMedia();
        void SetLayer2_Text(const std::string& text);
        void ClearLayer2();
        void SetNextText(const std::string& text); // vista previa para el Stage Display, nunca al publico

        // Cue de cambio de estilo para OClock, disparada por un MacroPlayer.
        // ConsumeClockStyleCue() devuelve "" si no hay ninguna pendiente.
        void        SetClockStyleCue(const std::string& styleName);
        std::string ConsumeClockStyleCue();

        // Transicion pendiente para la proxima vez que se dispare una (ver
        // TransitionPanel::Trigger(), que la consume) — disparada por un
        // MacroCue con transitionName no vacio. Se guarda como string, no
        // como UI::TransitionType, para que backend/core no dependa de
        // frontend/panels; el mapeo nombre<->enum vive en TransitionPanel.cpp.
        void SetPendingTransitionOverride(const std::string& name, float duration);
        bool ConsumePendingTransitionOverride(std::string& outName, float& outDuration);

        // Cue "consumir una vez" para que el rework del editor de canciones
        // pueda abrir el editor unificado directamente tras crear una
        // cancion nueva, sin popup modal — ver CreateNewSong (LibrarySongs.cpp)
        // y SongView::Render (que hace ConsumeSongEditorOpenRequest cada
        // frame y compara contra la seleccion actual). Mismo patron que
        // SetClockStyleCue/ConsumeClockStyleCue arriba.
        void        RequestSongEditorOpen(const std::string& filename);
        bool        ConsumeSongEditorOpenRequest(std::string& outFilename);

        // ── Macros (ver MacroTypes.h) ─────────────────────────────────────
        // El MacroPlayer vive aca (no en un panel) para que tanto el editor
        // (LayersOverlayTab) como el transporte "Control Overlays"
        // (ViewPanel) controlen la MISMA reproduccion.
        void        PlayMacro(const std::string& name, bool autoAdvance);
        void        StopMacro();
        void        NextMacroCue();
        void        PrevMacroCue();
        void        SetMacroCueIndex(int index); // salta directo a una cue (ej. recall de un Pad)
        void        SetMacroAutoAdvance(bool autoAdvance);
        bool        GetMacroAutoAdvance() const;
        bool        IsMacroPlaying() const;
        std::string GetActiveMacroName() const;
        int         GetMacroCueIndex() const; // -1 = ninguna cue disparada aun
        int         GetMacroCueCount() const;
        float       GetMacroElapsed() const;

        void*          GetPreviewTexture();
        VLCBasePlayer* GetPreviewPlayer();
        void           SetPreviewMedia(const std::string& path);
        void           StopPreviewMedia();

        // Pide cargar/detener el reproductor de Preview en un hilo aparte
        // (ver PreviewLoadWorker) — a diferencia de llamar Play()/Stop()
        // directo sobre GetPreviewPlayer(), esto NUNCA bloquea el hilo
        // principal (el que actualiza/dibuja el video en vivo al publico).
        // Preferir esto sobre GetPreviewPlayer()->Play()/Stop() en
        // cualquier codigo de UI que reaccione a una seleccion cambiante.
        void RequestPreviewLoad(const std::string& path, bool loop, bool startMuted);
        void RequestPreviewStop();

        void UpdateTextStyle(float size, const float color[4], int align,
                             int vAlign, const float margins[4], bool autoScale,
                             const std::string& font);

        void UpdateBibleStyle(float refSize, float verseSize, int hAlign, int vAlign);
        void UpdateSongStyle(int hAlign, int vAlign);

        void        SetProjecting(bool projecting);
        bool        IsProjecting() const;
        void        SetTargetMonitor(int index);
        void        SetProjectorSize(int w, int h);

        // Monitor de Control (Stage Display). El contenido real se dibuja en
        // el viewport ImGui "StageLive" de UIManager.cpp, leyendo estos campos.
        void        SetStaging(bool active, int monitorIndex = -1);
        bool        IsStaging() const;

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

        // Llamar UNA VEZ POR FRAME desde main(), DESPUES de core.Update(),
        // para refrescar todas las ventanas secundarias activas.
        void RenderAllSecondaryWindows();

        // ── Atajos con nombre fijo para los casos conocidos hoy ─────────
        bool        CreateProjectorWindow(int monitorIndex);
        void        DestroyProjectorWindow();
        bool        IsProjectorWindowActive() const;
        GLFWwindow* GetProjectorWindow() const;

        float GetLivePosition();
        void  SetLivePosition(float pos);
        int   GetLiveVolume();
        bool  GetLiveMute();
        void  SetLiveVolume(int volume);
        void  SetLiveMute(bool mute);
        // Loop del player "general" (bg/PROGRAM). Antes era un bool local de
        // MonitorView; se subio al estado compartido porque el toggle (en
        // Monitor, ver MonitorCenterColumn) y el enforcement del auto-restart
        // al llegar al final (en ViewPanel, ver RenderLiveTransport) ahora
        // viven en dos clases distintas.
        bool  GetLiveLoop();
        void  SetLiveLoop(bool loop);

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

        // Chat y Streaming comparten el MISMO NetworkStreamServer/puerto (ver
        // ChatMessageStore.h para el porque) — cualquiera de los dos puede
        // arrancarlo si todavia no esta corriendo; apagar uno no lo tira
        // abajo si el otro todavia lo esta usando.
        void ToggleChatServer(bool enable, int port = 8080);
        bool IsChatRunning() const;
        ChatMessageStore* GetChatMessageStore() { return &m_ChatMessageStore; }

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

        // Default false: los fondos decorativos (Fondos/BackgroundsPanel) nunca
        // deben sonar. Solo los flujos de "enviar al monitor" pasan
        // allowAudio=true explicitamente (ver MonitorCenterColumn,
        // MonitorQueueEngine, LibraryVideos "Enviar al monitor").
        void SetBackgroundMedia(const std::string& path, bool isVideo, bool allowAudio = false);

        // ── Fondo "now playing" (disco + caratula + ondas) ──────────────────
        // Manda el bgType a Audio y para cualquier video/color previo (mismo
        // criterio que StopBackgroundMedia) — quien realmente dibuja el
        // visual es AudioPanel::RenderLiveBackground (ver GetAudioPanelRef),
        // esto solo prende el estado. Lo llama el boton "En vivo" del panel
        // de audio de la biblioteca.
        void SetBackgroundAudio();

        // Puntero no-propietario al panel de audio de la biblioteca (ver
        // LibraryPanel::GetAudioPanel), wireado una vez en main.cpp — mismo
        // patron que HomePanel::SetAudioPanel. Se usa para: (a) que
        // UIManager pueda pedirle que dibuje el fondo "now playing" en el
        // proyector real, y (b) que este mismo PresentationCore le avise si
        // hay que apagar su boton "En vivo" porque se mando otra cosa en
        // vivo desde otro lado.
        void              SetAudioPanelRef(ProyecThor::UI::AudioPanel* panel) { m_AudioPanelRef = panel; }
        ProyecThor::UI::AudioPanel* GetAudioPanelRef() const { return m_AudioPanelRef; }

        // Mismo patron que AudioPanelRef, pero estos tres se wirean solos
        // (cada panel dueño registra la direccion de su propio miembro en
        // su constructor — StylesHubPanel para Announcements/CapturePanel,
        // ViewToolsPanel para OClock — no hace falta tocar main.cpp). Se
        // usan desde ViewPanel para los botones "Limpiar X" especificos por
        // tipo de contenido, sin que ViewPanel necesite conocer StylesHubPanel
        // ni ViewToolsPanel directamente.
        void                        SetAnnouncementsRef(ProyecThor::UI::Announcements* a) { m_AnnouncementsRef = a; }
        ProyecThor::UI::Announcements* GetAnnouncementsRef() const { return m_AnnouncementsRef; }

        void                 SetOClockRef(ProyecThor::UI::OClock* c) { m_OClockRef = c; }
        ProyecThor::UI::OClock* GetOClockRef() const { return m_OClockRef; }

        void                       SetCapturePanelRef(ProyecThor::UI::CapturePanel* c) { m_CapturePanelRef = c; }
        ProyecThor::UI::CapturePanel* GetCapturePanelRef() const { return m_CapturePanelRef; }

        // ── Preload adelantado (ver BackgroundLayer::Prefetch/CommitPrefetch) ──
        // Usado por la cola del Monitor para cargar el SIGUIENTE clip en
        // segundo plano mientras el actual sigue reproduciendose, sin
        // disparar la transicion visible (no toca transitionTrigger:
        // invisible para el operador hasta que se confirma con
        // CommitNextBackgroundMedia). El resultado es un corte instantaneo
        // en la transicion, en vez de recien abrir el archivo en ese momento.
        void PreloadNextBackgroundMedia(const std::string& path, bool allowAudio = false);
        void CommitNextBackgroundMedia(const std::string& path, bool isVideo, bool allowAudio = false);

        // Para el indicador de carga en el preview del operador (ver
        // ViewPanel) — nunca se muestra en la salida real al publico.
        bool  IsBackgroundSwapPending() const;
        float GetBackgroundSwapEta() const;

        // Crossfade del fondo (ver BackgroundLayer::Update/Render) expuesto
        // para que el rendering ImGui del proyector real ("ProjectorLive" en
        // UIManager.cpp) pueda blendear Active/Standby igual que ya hace
        // BackgroundLayer::Render() via BlitTexture — evita duplicar la
        // logica de CUANDO blendear, solo el COMO (ImGui::AddImage con tint
        // alpha en vez de BlitTexture con glBlendFunc).
        void*  GetStandbyBackgroundTexture();
        float  GetBackgroundBlendProgress() const;
        bool   IsBackgroundStandbyReady();

        // ── Logo / pantalla de carga PUBLICA (ver Ajustes > Proyeccion) ────
        // A diferencia de lo anterior, esto SI se muestra en la salida real
        // (ver BackgroundLayer::Render / RenderProjectorWindow / el bloque
        // "ProjectorLive" de UIManager.cpp) mientras haya algo cargando.
        void  SetLoadingLogoPath(const std::string& path);
        void* GetLoadingLogoTexture() const;
        int   GetLoadingLogoWidth()  const { return m_LoadingLogoW; }
        int   GetLoadingLogoHeight() const { return m_LoadingLogoH; }
        bool  ShouldShowLoadingScreen() const;

    private:
        void RenderDefaultStyleCombo();
        void EnsureFBO(int w, int h);
        void DestroyFBO();

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
        ImGuiID m_ProjectorPostFXViewportID = 0;
        std::unique_ptr<PresentationCoreImpl> m_Impl;
        mutable std::mutex m_Mutex;

        PresentationState m_State;
        LibrarySelection  m_CurrentSelection;
        bool              m_SelectionFromQueue = false;

        // Cue de "cambiar estilo del reloj" pendiente de un MacroPlayer (ver
        // MacroTypes.h/LayersOverlayTab). Patron "consumir una vez", igual
        // que ConsumeEndReached() en VLCBasePlayer: OClock::Update() la lee
        // y limpia cada frame, asi no compite con que el operador cambie el
        // estilo a mano desde el combo de OClock.
        std::string m_PendingClockStyleCue;
        bool        m_HasClockStyleCue = false;

        // Mismo patron "consumir una vez" que m_PendingClockStyleCue, para
        // la transicion pendiente de un MacroCue (ver SetPendingTransitionOverride).
        std::string m_PendingTransitionName;
        float       m_PendingTransitionDuration = -1.0f;
        bool        m_HasTransitionOverride = false;

        // Ver RequestSongEditorOpen/ConsumeSongEditorOpenRequest arriba.
        std::string m_PendingSongEditorOpenFile;
        bool        m_HasSongEditorOpenRequest = false;

        MacroPlayer m_MacroPlayer;

        // ── Logo (pantalla de carga, ver Ajustes > Proyeccion) ────────────
        // Textura GL cargada una sola vez (recargada si el path cambia),
        // mostrada a la salida real mientras ShouldShowLoadingScreen() es
        // true (ver SetLoadingLogoPath/GetLoadingLogoTexture).
        std::string  m_LoadingLogoPath;
        unsigned int m_LoadingLogoTex = 0;
        int          m_LoadingLogoW = 0;
        int          m_LoadingLogoH = 0;

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

        // ── Streaming en red local ───────────────────────────────────────
        std::unique_ptr<NetworkStreamServer> m_NetworkServer;
        ChatMessageStore                     m_ChatMessageStore;

        // Arma los 3 providers (snapshot/frame/fuente) de un NetworkStreamServer
        // recien creado — lo llaman tanto ToggleNetworkStream como
        // ToggleChatServer cuando les toca crear el server compartido.
        void WireNetworkServerProviders(NetworkStreamServer& srv);

        // Ver SetAudioPanelRef/GetAudioPanelRef.
        ProyecThor::UI::AudioPanel*    m_AudioPanelRef    = nullptr;
        ProyecThor::UI::Announcements* m_AnnouncementsRef = nullptr;
        ProyecThor::UI::OClock*        m_OClockRef        = nullptr;
        ProyecThor::UI::CapturePanel*  m_CapturePanelRef  = nullptr;

        // Unico lugar que escribe m_State.bgType: si se esta dejando Audio
        // por otra cosa, apaga el boton "En vivo" del panel de audio. Debe
        // llamarse con m_Mutex ya tomado por quien invoca.
        void SetBgTypeLocked(PresentationState::BackgroundType newType);
        std::atomic<uint64_t>                m_StreamVersion { 0 };

        mutable std::mutex    m_FrameMutex;
        std::vector<uint8_t>  m_LatestFrame;
        std::atomic<bool>     m_FrameProviderActive { false };
    };

} // namespace ProyecThor::Core