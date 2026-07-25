#include "PresentationCore.h"
#include "BackgroundLayer.h"
#include "OverlayLayer.h"
#include "backend/shaders/CompositePostChain.h"
#include "frontend/panels/stb_image.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include "backend/settings/SettingsManager.h"
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <vector>
#include "AppPaths.h"
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif
#include "NetworkStreamServer.h"
#include "PreviewLoadWorker.h"
#include "frontend/views/Audio.h"

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

        // Ver PreviewLoadWorker.h: saca el Play()/Stop() del Preview del
        // hilo principal, para que una carga lenta ahi nunca le robe
        // tiempo al hilo que actualiza/dibuja el video en vivo al publico.
        PreviewLoadWorker previewLoader;

        // Post-proceso del composite completo de "ProjectorLive" (CRT/
        // Grano/FXAA) — ver CompositePostChain.h. Vive aca (no dentro de
        // background/overlay) porque corre en un punto distinto del pipeline
        // (sobre el ImDrawData ya compuesto, no sobre una textura de fondo).
        Shaders::CompositePostChain compositeFX;
    };

    PresentationCore::PresentationCore()
        : m_Impl(std::make_unique<PresentationCoreImpl>()) {}

    PresentationCore::~PresentationCore() {
        if (m_NetworkServer && m_NetworkServer->IsRunning())
            m_NetworkServer->Stop();
        DestroyFBO();
        DestroyAllSecondaryWindows();
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
    // FIX: esto muta currentText (el mismo campo que las letras/Layer2), asi
    // que dispara textTransitionTrigger, no transitionTrigger (ese es solo
    // para fondo/video — ver PresentationState). Antes compartian un unico
    // contador y un cambio de fondo animaba el texto sin que este hubiera
    // cambiado, y viceversa.
    ++m_State.textTransitionTrigger;
    ++m_StreamVersion;
}

void PresentationCore::ClearQuickNote() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_State.currentText   = "";
    m_State.showText      = false;
    m_State.showQuickNote = false;
    ++m_State.textTransitionTrigger;
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
void PresentationCore::SetGlobalMute(bool mute) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_GlobalMuted = mute;
    
    // Si manejas el volumen global en VLC o en tu OverlayLayer, aplícalo aquí.
    // Ejemplo: m_Impl->overlay.SetMute(mute);
}

bool PresentationCore::GetGlobalMute() const {
    return m_GlobalMuted;
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

    void PresentationCore::RequestPreviewLoad(const std::string& path, bool loop, bool startMuted) {
        if (!m_Impl) return;
        m_Impl->previewLoader.RequestLoad(m_Impl->preview.GetPlayer(), path, loop, startMuted);
    }

    void PresentationCore::RequestPreviewStop() {
        if (!m_Impl) return;
        m_Impl->previewLoader.RequestStop(m_Impl->preview.GetPlayer());
    }

    void* PresentationCore::GetProcessedBackgroundTexture(int targetW, int targetH) {
        return m_Impl ? m_Impl->background.GetProcessedTexture(targetW, targetH) : nullptr;
    }

    void* PresentationCore::GetPreviewBackgroundTexture(int targetW, int targetH) {
        if (!m_Impl) return nullptr;
        void* rawTex = m_Impl->background.GetProcessedTexture(targetW, targetH);
        if (!rawTex) return nullptr;

        GLuint raw = static_cast<GLuint>(reinterpret_cast<uintptr_t>(rawTex));
        GLuint processed = m_Impl->compositeFX.ProcessBackgroundForPreview(raw, targetW, targetH);
        return (void*)(uintptr_t)processed;
    }

    void* PresentationCore::GetBackgroundFillTexture(int workW, int workH) {
        return m_Impl ? m_Impl->background.GetBlurredFillTexture(workW, workH) : nullptr;
    }
    void PresentationCore::SetFillBlurEnabled(bool enabled) {
        if (m_Impl) m_Impl->background.SetFillBlurEnabled(enabled);
    }
    bool PresentationCore::GetFillBlurEnabled() const {
        return m_Impl ? m_Impl->background.GetFillBlurEnabled() : false;
    }
    void PresentationCore::SetFillBlurBrightness(float v) {
        if (m_Impl) m_Impl->background.SetFillBlurBrightness(v);
    }
    float PresentationCore::GetFillBlurBrightness() const {
        return m_Impl ? m_Impl->background.GetFillBlurBrightness() : 0.6f;
    }

    void* PresentationCore::GetOverlayTexture() {
        return m_Impl ? m_Impl->overlay.GetTextureID() : nullptr;
    }

    bool PresentationCore::IsOverlayActive() const {
        return m_Impl && m_Impl->overlay.IsActive();
    }

    void PresentationCore::SetFSREnabled(bool enabled) {
        if (!m_Impl) return;
        m_Impl->background.SetFSREnabled(enabled);
        // Mutua exclusion: los dos son upscalers de la misma etapa, no
        // tiene sentido correr ambos (ver PostProcessorNIS.h).
        if (enabled) m_Impl->background.SetNISEnabled(false);
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

    void PresentationCore::SetNISEnabled(bool enabled) {
        if (!m_Impl) return;
        m_Impl->background.SetNISEnabled(enabled);
        if (enabled) m_Impl->background.SetFSREnabled(false);
    }

    bool PresentationCore::GetNISEnabled() const {
        return m_Impl ? m_Impl->background.GetNISEnabled() : false;
    }

    void PresentationCore::SetNISSharpness(float sharpness) {
        if (m_Impl) m_Impl->background.SetNISSharpness(sharpness);
    }

    float PresentationCore::GetNISSharpness() const {
        return m_Impl ? m_Impl->background.GetNISSharpness() : 0.5f;
    }

    void PresentationCore::SetVideoRenderEngine(int engine) {
        if (m_Impl) m_Impl->background.SetUseNativeEngine(engine != 0);
    }
    int PresentationCore::GetVideoRenderEngine() const {
        return (m_Impl && m_Impl->background.GetUseNativeEngine()) ? 1 : 0;
    }

    void PresentationCore::SetCRTEnabled(bool enabled) {
        if (m_Impl) m_Impl->compositeFX.SetCRTEnabled(enabled);
    }
    bool PresentationCore::GetCRTEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetCRTEnabled() : false;
    }
    void PresentationCore::SetCRTScanlineIntensity(float intensity) {
        if (m_Impl) m_Impl->compositeFX.SetCRTScanlineIntensity(intensity);
    }
    float PresentationCore::GetCRTScanlineIntensity() const {
        return m_Impl ? m_Impl->compositeFX.GetCRTScanlineIntensity() : 0.5f;
    }

    void PresentationCore::SetGrainEnabled(bool enabled) {
        if (m_Impl) m_Impl->compositeFX.SetGrainEnabled(enabled);
    }
    bool PresentationCore::GetGrainEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetGrainEnabled() : false;
    }
    void PresentationCore::SetGrainIntensity(float intensity) {
        if (m_Impl) m_Impl->compositeFX.SetGrainIntensity(intensity);
    }
    float PresentationCore::GetGrainIntensity() const {
        return m_Impl ? m_Impl->compositeFX.GetGrainIntensity() : 0.15f;
    }

    void PresentationCore::SetFXAAEnabled(bool enabled) {
        if (m_Impl) m_Impl->compositeFX.SetFXAAEnabled(enabled);
    }
    bool PresentationCore::GetFXAAEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetFXAAEnabled() : false;
    }

    void PresentationCore::SetSaturationEnabled(bool enabled) {
        if (m_Impl) m_Impl->compositeFX.SetSaturationEnabled(enabled);
    }
    bool PresentationCore::GetSaturationEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetSaturationEnabled() : false;
    }
    void PresentationCore::SetSaturationAmount(float amount) {
        if (m_Impl) m_Impl->compositeFX.SetSaturationAmount(amount);
    }
    float PresentationCore::GetSaturationAmount() const {
        return m_Impl ? m_Impl->compositeFX.GetSaturationAmount() : 1.3f;
    }

    void PresentationCore::SetVignetteEnabled(bool enabled) {
        if (m_Impl) m_Impl->compositeFX.SetVignetteEnabled(enabled);
    }
    bool PresentationCore::GetVignetteEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetVignetteEnabled() : false;
    }
    void PresentationCore::SetVignetteIntensity(float intensity) {
        if (m_Impl) m_Impl->compositeFX.SetVignetteIntensity(intensity);
    }
    float PresentationCore::GetVignetteIntensity() const {
        return m_Impl ? m_Impl->compositeFX.GetVignetteIntensity() : 0.45f;
    }

    void PresentationCore::SetBlurEnabled(bool enabled) {
        if (m_Impl) m_Impl->compositeFX.SetBlurEnabled(enabled);
    }
    bool PresentationCore::GetBlurEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetBlurEnabled() : false;
    }
    void PresentationCore::SetBlurIntensity(float intensity) {
        if (m_Impl) m_Impl->compositeFX.SetBlurIntensity(intensity);
    }
    float PresentationCore::GetBlurIntensity() const {
        return m_Impl ? m_Impl->compositeFX.GetBlurIntensity() : 0.35f;
    }

    void PresentationCore::SetSharpenEnabled(bool enabled) {
        if (m_Impl) m_Impl->compositeFX.SetSharpenEnabled(enabled);
    }
    bool PresentationCore::GetSharpenEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetSharpenEnabled() : false;
    }
    void PresentationCore::SetSharpenIntensity(float intensity) {
        if (m_Impl) m_Impl->compositeFX.SetSharpenIntensity(intensity);
    }
    float PresentationCore::GetSharpenIntensity() const {
        return m_Impl ? m_Impl->compositeFX.GetSharpenIntensity() : 0.35f;
    }

    void PresentationCore::SetBloomEnabled(bool enabled) {
        if (m_Impl) m_Impl->compositeFX.SetBloomEnabled(enabled);
    }
    bool PresentationCore::GetBloomEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetBloomEnabled() : false;
    }
    void PresentationCore::SetBloomIntensity(float intensity) {
        if (m_Impl) m_Impl->compositeFX.SetBloomIntensity(intensity);
    }
    float PresentationCore::GetBloomIntensity() const {
        return m_Impl ? m_Impl->compositeFX.GetBloomIntensity() : 0.35f;
    }

    void PresentationCore::SetChromaticAberrationEnabled(bool enabled) {
        if (m_Impl) m_Impl->compositeFX.SetChromaticAberrationEnabled(enabled);
    }
    bool PresentationCore::GetChromaticAberrationEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetChromaticAberrationEnabled() : false;
    }
    void PresentationCore::SetChromaticAberrationIntensity(float intensity) {
        if (m_Impl) m_Impl->compositeFX.SetChromaticAberrationIntensity(intensity);
    }
    float PresentationCore::GetChromaticAberrationIntensity() const {
        return m_Impl ? m_Impl->compositeFX.GetChromaticAberrationIntensity() : 0.35f;
    }

    void PresentationCore::SetVHSEnabled(bool enabled) {
        if (m_Impl) m_Impl->compositeFX.SetVHSEnabled(enabled);
    }
    bool PresentationCore::GetVHSEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetVHSEnabled() : false;
    }
    void PresentationCore::SetVHSIntensity(float intensity) {
        if (m_Impl) m_Impl->compositeFX.SetVHSIntensity(intensity);
    }
    float PresentationCore::GetVHSIntensity() const {
        return m_Impl ? m_Impl->compositeFX.GetVHSIntensity() : 0.5f;
    }

    void PresentationCore::SetCineEnabled(bool enabled) {
        if (m_Impl) m_Impl->compositeFX.SetCineEnabled(enabled);
    }
    bool PresentationCore::GetCineEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetCineEnabled() : false;
    }
    void PresentationCore::SetCineIntensity(float intensity) {
        if (m_Impl) m_Impl->compositeFX.SetCineIntensity(intensity);
    }
    float PresentationCore::GetCineIntensity() const {
        return m_Impl ? m_Impl->compositeFX.GetCineIntensity() : 0.5f;
    }
    void PresentationCore::SetCineTint(int tint) {
        if (m_Impl) m_Impl->compositeFX.SetCineTint(tint);
    }
    int PresentationCore::GetCineTint() const {
        return m_Impl ? m_Impl->compositeFX.GetCineTint() : 0;
    }

    void PresentationCore::SetContrastEnabled(bool enabled) {
        if (m_Impl) m_Impl->compositeFX.SetContrastEnabled(enabled);
    }
    bool PresentationCore::GetContrastEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetContrastEnabled() : false;
    }
    void PresentationCore::SetContrastAmount(float amount) {
        if (m_Impl) m_Impl->compositeFX.SetContrastAmount(amount);
    }
    float PresentationCore::GetContrastAmount() const {
        return m_Impl ? m_Impl->compositeFX.GetContrastAmount() : 1.3f;
    }

    void PresentationCore::SetLuminosityEnabled(bool enabled) {
        if (m_Impl) m_Impl->compositeFX.SetLuminosityEnabled(enabled);
    }
    bool PresentationCore::GetLuminosityEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetLuminosityEnabled() : false;
    }
    void PresentationCore::SetLuminosityAmount(float amount) {
        if (m_Impl) m_Impl->compositeFX.SetLuminosityAmount(amount);
    }
    float PresentationCore::GetLuminosityAmount() const {
        return m_Impl ? m_Impl->compositeFX.GetLuminosityAmount() : 1.2f;
    }

    void PresentationCore::SetTAAEnabled(bool enabled) {
        if (m_Impl) m_Impl->compositeFX.SetTAAEnabled(enabled);
    }
    bool PresentationCore::GetTAAEnabled() const {
        return m_Impl ? m_Impl->compositeFX.GetTAAEnabled() : false;
    }
    void PresentationCore::SetTAAIntensity(float intensity) {
        if (m_Impl) m_Impl->compositeFX.SetTAAIntensity(intensity);
    }
    float PresentationCore::GetTAAIntensity() const {
        return m_Impl ? m_Impl->compositeFX.GetTAAIntensity() : 0.5f;
    }

    void PresentationCore::SetProjectorPostFXViewportID(ImGuiID id) {
        m_ProjectorPostFXViewportID = id;
    }
    bool PresentationCore::IsProjectorPostFXViewport(ImGuiID id) const {
        return id != 0 && id == m_ProjectorPostFXViewportID;
    }
    void PresentationCore::RenderProjectorViewportPostFX(ImGuiViewport* viewport,
                                                         void (*defaultRenderFn)(ImGuiViewport*, void*))
    {
        if (m_Impl) m_Impl->compositeFX.RenderViewport(viewport, defaultRenderFn);
        else if (defaultRenderFn) defaultRenderFn(viewport, nullptr);
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
        m_MacroPlayer.Update();
    }

    void PresentationCore::RenderBackground(int outputW, int outputH) {
        if (m_Impl)
            m_Impl->background.Render(outputW, outputH);
    }

    void PresentationCore::RenderProjectorWindow() {
    if (m_Impl) {
        if (ShouldShowLoadingScreen()) {
            // Pantalla de carga: se muestra el logo en vez del fondo/overlay
            // mientras algo esta cargando, para que el publico nunca vea un
            // frame entrecortado o desactualizado (ver Ajustes > Proyeccion
            // > Logo).
            m_Impl->background.RenderLogo(
                static_cast<unsigned int>(reinterpret_cast<uintptr_t>(GetLoadingLogoTexture())),
                m_LoadingLogoW, m_LoadingLogoH, m_ProjectorWidth, m_ProjectorHeight);
        } else {
            m_Impl->background.Render(m_ProjectorWidth, m_ProjectorHeight);
            m_Impl->overlay.Render();
        }
    }
}
    // ── Ventanas secundarias, API generica ──────────────────────────────
    bool PresentationCore::CreateSecondaryWindow(const std::string& id, int monitorIndex,
                                                  const std::string& title,
                                                  SecondaryOutputWindow::RenderFn renderFn)
    {
        if (!m_MainWindow) {
            std::cerr << "[PresentationCore] CreateSecondaryWindow('" << id
                      << "'): falta SetMainWindow() previo.\n";
            return false;
        }

        std::lock_guard<std::mutex> lock(m_SecondaryWindowsMutex);

        SecondaryOutput& out = m_SecondaryWindows[id]; // crea si no existe
        if (!out.window.Create(m_MainWindow, monitorIndex, title))
        {
            m_SecondaryWindows.erase(id);
            return false;
        }
        out.renderFn = std::move(renderFn);
        return true;
    }

    void PresentationCore::DestroySecondaryWindow(const std::string& id)
    {
        std::lock_guard<std::mutex> lock(m_SecondaryWindowsMutex);
        m_SecondaryWindows.erase(id); // el destructor de SecondaryOutputWindow limpia la ventana
    }

    void PresentationCore::DestroyAllSecondaryWindows()
    {
        std::lock_guard<std::mutex> lock(m_SecondaryWindowsMutex);
        m_SecondaryWindows.clear();
    }

    bool PresentationCore::IsSecondaryWindowActive(const std::string& id) const
    {
        std::lock_guard<std::mutex> lock(m_SecondaryWindowsMutex);
        auto it = m_SecondaryWindows.find(id);
        return it != m_SecondaryWindows.end() && it->second.window.IsActive();
    }

    void PresentationCore::RenderAllSecondaryWindows()
    {
        // Copia de punteros bajo lock, render fuera del lock: RenderFrame
        // hace MakeContextCurrent + swap, no queremos tener el mutex
        // tomado durante llamadas GL potencialmente bloqueantes (vsync).
        std::vector<SecondaryOutput*> active;
        {
            std::lock_guard<std::mutex> lock(m_SecondaryWindowsMutex);
            active.reserve(m_SecondaryWindows.size());
            for (auto& [id, out] : m_SecondaryWindows)
                if (out.window.IsActive())
                    active.push_back(&out);
        }

        for (auto* out : active)
            out->window.RenderFrame(out->renderFn);
    }

    // ── Atajos con nombre fijo: Proyector ────────────────────────────────
    bool PresentationCore::CreateProjectorWindow(int monitorIndex)
{
    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    if (monitorIndex >= 0 && monitorIndex < monitorCount) {
        if (const GLFWvidmode* vm = glfwGetVideoMode(monitors[monitorIndex])) {
            SetProjectorSize(vm->width, vm->height); // <-- clave
        }
    }

    bool ok = CreateSecondaryWindow(kProjectorId, monitorIndex, "ProyecThor - Proyector",
        [this](int w, int h) {
            SetProjectorSize(w, h);   // también usar el tamaño real que llega al renderFn
            RenderProjectorWindow();
        });

        if (ok) {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_State.targetMonitorIndex = monitorIndex;
        }
        return ok;
    }

    void PresentationCore::DestroyProjectorWindow()
    {
        DestroySecondaryWindow(kProjectorId);
    }

    bool PresentationCore::IsProjectorWindowActive() const
    {
        return IsSecondaryWindowActive(kProjectorId);
    }

    GLFWwindow* PresentationCore::GetProjectorWindow() const
    {
        std::lock_guard<std::mutex> lock(m_SecondaryWindowsMutex);
        auto it = m_SecondaryWindows.find(kProjectorId);
        return (it != m_SecondaryWindows.end()) ? it->second.window.GetWindow() : nullptr;
    }

// Unico lugar que escribe m_State.bgType — ver comentario en el header.
void PresentationCore::SetBgTypeLocked(PresentationState::BackgroundType newType)
{
    if (m_State.bgType == PresentationState::BackgroundType::Audio &&
        newType != PresentationState::BackgroundType::Audio &&
        m_AudioPanelRef)
    {
        m_AudioPanelRef->SetLiveBackground(false);
    }
    m_State.bgType = newType;
}

void PresentationCore::SetBackgroundMedia(const std::string& path, bool /*isVideo*/, bool allowAudio) {
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.bgPath = path;
        SetBgTypeLocked(PresentationState::BackgroundType::Video);
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
        SetBgTypeLocked(PresentationState::BackgroundType::SolidColor);
        m_State.bgColor[0] = 0.0f; m_State.bgColor[1] = 0.0f; m_State.bgColor[2] = 0.0f;
        ++m_State.transitionTrigger;   // NUEVO
        ++m_StreamVersion;
    }
    if (m_Impl) m_Impl->background.SetSolidColor(0.0f, 0.0f, 0.0f);
}

// Ver comentario en el header (junto a la declaracion) para el porque.
void PresentationCore::SetBackgroundAudio() {
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.bgPath = "";
        SetBgTypeLocked(PresentationState::BackgroundType::Audio);
        ++m_State.transitionTrigger;
        ++m_StreamVersion;
    }
    // Mismo criterio que StopBackgroundMedia: un video de fondo previo no
    // debe seguir sonando por debajo del audio que se acaba de mandar en vivo.
    if (m_Impl) m_Impl->background.SetSolidColor(0.0f, 0.0f, 0.0f);
}

    void PresentationCore::PreloadNextBackgroundMedia(const std::string& path, bool allowAudio) {
        // A proposito NO toca m_State/transitionTrigger: este preload debe
        // ser invisible para el operador y para TransitionPanel — solo
        // adelanta la carga en standby (ver BackgroundLayer::Prefetch).
        if (m_Impl) m_Impl->background.Prefetch(path, allowAudio);
    }

    void PresentationCore::CommitNextBackgroundMedia(const std::string& path, bool /*isVideo*/, bool allowAudio) {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_State.bgPath = path;
            SetBgTypeLocked(PresentationState::BackgroundType::Video);
            ++m_State.transitionTrigger;
            ++m_StreamVersion;
        }
        if (m_Impl)
            m_Impl->background.CommitPrefetch(path, allowAudio);
    }

    bool PresentationCore::IsBackgroundSwapPending() const {
        return m_Impl && m_Impl->background.IsSwapPending();
    }

    float PresentationCore::GetBackgroundSwapEta() const {
        return m_Impl ? m_Impl->background.GetEstimatedLoadSeconds() : 0.0f;
    }

    void* PresentationCore::GetStandbyBackgroundTexture() {
        return m_Impl ? m_Impl->background.GetStandbyTextureID() : nullptr;
    }

    float PresentationCore::GetBackgroundBlendProgress() const {
        return m_Impl ? m_Impl->background.GetTransitionProgress() : 1.0f;
    }

    bool PresentationCore::IsBackgroundStandbyReady() {
        return m_Impl && m_Impl->background.StandbyHasFrame();
    }

    void PresentationCore::SetLoadingLogoPath(const std::string& path) {
        if (path == m_LoadingLogoPath) return; // sin cambios, no recargar cada frame

        if (m_LoadingLogoTex != 0) {
            GLuint old = m_LoadingLogoTex;
            glDeleteTextures(1, &old);
            m_LoadingLogoTex = 0;
        }
        m_LoadingLogoPath = path;
        m_LoadingLogoW = 0;
        m_LoadingLogoH = 0;

        if (path.empty()) return;

        int w = 0, h = 0, ch = 0;
        unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
        if (!data) {
            std::cerr << "[PresentationCore] No se pudo cargar el logo: " << path << "\n";
            return;
        }

        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);

        m_LoadingLogoTex = tex;
        m_LoadingLogoW   = w;
        m_LoadingLogoH   = h;
    }

    void* PresentationCore::GetLoadingLogoTexture() const {
        return m_LoadingLogoTex != 0 ? reinterpret_cast<void*>(static_cast<uintptr_t>(m_LoadingLogoTex)) : nullptr;
    }

    bool PresentationCore::ShouldShowLoadingScreen() const {
        // Se elimino el logo/pantalla de carga: sumado al preflight de la
        // cola, era una fuente constante de cortes y arranques lentos —
        // BackgroundLayer::Render() ya sigue mostrando el frame actual de
        // Active() mientras un swap esta en curso (asi funciona el
        // crossfade), asi que nunca hace falta tapar la salida con un logo.
        return false;
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
        SetBgTypeLocked(PresentationState::BackgroundType::SolidColor);
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

    void PresentationCore::SetTextEffects(const TextEffectsData& effects) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.effects = effects;
        ++m_StreamVersion;
    }

    TextEffectsData PresentationCore::GetTextEffects() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_State.effects;
    }

void PresentationCore::SetLayer2_Text(const std::string& text) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_State.currentText = text;
    m_State.showText    = !text.empty();
    ++m_State.textTransitionTrigger;
    ++m_StreamVersion;
}

void PresentationCore::ClearLayer2() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_State.currentText = "";
    m_State.showText    = false;
    m_State.nextText    = "";
    ++m_State.textTransitionTrigger;
    ++m_StreamVersion;
}

void PresentationCore::SetClockStyleCue(const std::string& styleName) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_PendingClockStyleCue = styleName;
    m_HasClockStyleCue     = true;
}

std::string PresentationCore::ConsumeClockStyleCue() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (!m_HasClockStyleCue) return {};
    m_HasClockStyleCue = false;
    return m_PendingClockStyleCue;
}

void PresentationCore::SetPendingTransitionOverride(const std::string& name, float duration) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_PendingTransitionName     = name;
    m_PendingTransitionDuration = duration;
    m_HasTransitionOverride     = true;
}

bool PresentationCore::ConsumePendingTransitionOverride(std::string& outName, float& outDuration) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (!m_HasTransitionOverride) return false;
    m_HasTransitionOverride = false;
    outName     = m_PendingTransitionName;
    outDuration = m_PendingTransitionDuration;
    return true;
}

void PresentationCore::RequestSongEditorOpen(const std::string& filename) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_PendingSongEditorOpenFile = filename;
    m_HasSongEditorOpenRequest  = true;
}

bool PresentationCore::ConsumeSongEditorOpenRequest(std::string& outFilename) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (!m_HasSongEditorOpenRequest) return false;
    m_HasSongEditorOpenRequest = false;
    outFilename = m_PendingSongEditorOpenFile;
    return true;
}

// ── Macros ───────────────────────────────────────────────────────────────
void PresentationCore::PlayMacro(const std::string& name, bool autoAdvance) {
    Macro m;
    if (!LoadMacro(name, m)) {
        std::cerr << "[PresentationCore] No se pudo cargar el macro \"" << name << "\".\n";
        return;
    }
    m_MacroPlayer.Play(m, autoAdvance);
}

void PresentationCore::StopMacro()               { m_MacroPlayer.Stop(); }
void PresentationCore::NextMacroCue()            { m_MacroPlayer.Next(); }
void PresentationCore::PrevMacroCue()            { m_MacroPlayer.Previous(); }
void PresentationCore::SetMacroCueIndex(int index) { m_MacroPlayer.GoToCue(index); }
void PresentationCore::SetMacroAutoAdvance(bool a) { m_MacroPlayer.SetAutoAdvance(a); }
bool PresentationCore::GetMacroAutoAdvance() const { return m_MacroPlayer.IsAutoAdvance(); }
bool PresentationCore::IsMacroPlaying() const      { return m_MacroPlayer.IsPlaying(); }
std::string PresentationCore::GetActiveMacroName() const { return m_MacroPlayer.GetMacro().name; }
int   PresentationCore::GetMacroCueIndex() const   { return m_MacroPlayer.GetCurrentCueIndex(); }
int   PresentationCore::GetMacroCueCount() const   { return m_MacroPlayer.GetCueCount(); }
float PresentationCore::GetMacroElapsed() const    { return m_MacroPlayer.GetElapsed(); }

void PresentationCore::SetNextText(const std::string& text) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_State.nextText = text;
    ++m_StreamVersion;
}

    void PresentationCore::SetProjecting(bool projecting) {
        int monitorIndex;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_State.isProjecting = projecting;
            ++m_StreamVersion;
            monitorIndex = m_State.targetMonitorIndex;
        }

        // Unico punto que habilita/corta el audio real hacia el publico
        // (y, con el motor "VLC ventana nativa", tambien la ventana de
        // video en si — ver BackgroundLayer::SetPubliclyLive). Fuera del
        // lock: BackgroundLayer solo toca atomicos de los players (mas la
        // ventana nativa, que vive en el hilo principal igual que esto),
        // no hace falta serializarlo con m_State.
        if (m_Impl)
            m_Impl->background.SetPubliclyLive(projecting, monitorIndex);
    }

    bool PresentationCore::IsProjecting() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_State.isProjecting;
    }

    void PresentationCore::SetTargetMonitor(int index) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.targetMonitorIndex = index;
    }

    void PresentationCore::SetStaging(bool active, int monitorIndex) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.isStaging = active;
        if (monitorIndex >= 0)
            m_State.stageMonitorIndex = monitorIndex;
        ++m_StreamVersion;
    }

    bool PresentationCore::IsStaging() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_State.isStaging;
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

    bool PresentationCore::GetLiveMute() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_State.liveMuted;
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
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_State.liveMuted = mute;
        }
        if (m_Impl)
            m_Impl->background.SetLiveMute(mute);
    }

    bool PresentationCore::GetLiveLoop() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_State.liveLoop;
    }

    void PresentationCore::SetLiveLoop(bool loop) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.liveLoop = loop;
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

    // Empaqueta/desempaqueta TextEffectsData como una sola linea CSV en el
    // archivo .theme -- evita 7 bloques de bool/color/intensidad repetidos
    // (uno por efecto) en el formato "key=value" de este archivo. Orden fijo:
    // bg(enabled,r,g,b,a) border(enabled,r,g,b,a,width) shadow(enabled,r,g,b,a,intensity)
    // chromaticAberration(enabled,intensity) glow(enabled,r,g,b,a,intensity)
    // neon(enabled,r,g,b,a,intensity) underline(enabled,r,g,b,a,thickness)
    std::string PackTextEffects(const TextEffectsData& e)
    {
        std::ostringstream ss;
        auto put = [&](float v) { ss << v << ","; };
        put(e.bgEnabled ? 1.0f : 0.0f);
        for (float c : e.bgColor) put(c);
        put(e.borderEnabled ? 1.0f : 0.0f);
        for (float c : e.borderColor) put(c);
        put(e.borderWidth);
        put(e.shadowEnabled ? 1.0f : 0.0f);
        for (float c : e.shadowColor) put(c);
        put(e.shadowIntensity);
        put(e.chromaticAberrationEnabled ? 1.0f : 0.0f);
        put(e.chromaticAberrationIntensity);
        put(e.glowEnabled ? 1.0f : 0.0f);
        for (float c : e.glowColor) put(c);
        put(e.glowIntensity);
        put(e.neonEnabled ? 1.0f : 0.0f);
        for (float c : e.neonColor) put(c);
        put(e.neonIntensity);
        put(e.underlineEnabled ? 1.0f : 0.0f);
        for (float c : e.underlineColor) put(c);
        ss << e.underlineThickness;
        return ss.str();
    }

    void UnpackTextEffects(const std::string& v, TextEffectsData& e)
    {
        std::vector<float> f;
        std::stringstream ss(v);
        std::string tok;
        while (std::getline(ss, tok, ',')) {
            if (!tok.empty()) f.push_back(std::stof(tok));
        }
        if (f.size() < 37) return; // linea corrupta/vieja -- deja los defaults

        size_t i = 0;
        e.bgEnabled = f[i++] != 0.0f;
        for (float& c : e.bgColor) c = f[i++];
        e.borderEnabled = f[i++] != 0.0f;
        for (float& c : e.borderColor) c = f[i++];
        e.borderWidth = f[i++];
        e.shadowEnabled = f[i++] != 0.0f;
        for (float& c : e.shadowColor) c = f[i++];
        e.shadowIntensity = f[i++];
        e.chromaticAberrationEnabled = f[i++] != 0.0f;
        e.chromaticAberrationIntensity = f[i++];
        e.glowEnabled = f[i++] != 0.0f;
        for (float& c : e.glowColor) c = f[i++];
        e.glowIntensity = f[i++];
        e.neonEnabled = f[i++] != 0.0f;
        for (float& c : e.neonColor) c = f[i++];
        e.neonIntensity = f[i++];
        e.underlineEnabled = f[i++] != 0.0f;
        for (float& c : e.underlineColor) c = f[i++];
        e.underlineThickness = f[i++];
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
            else if (k == "textEffects")
                UnpackTextEffects(v, out.effects);
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
        f << "textEffects="    << PackTextEffects(style.effects) << "\n";

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
        state.effects            = s.effects;
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

    void PresentationCore::SetSelection(const LibrarySelection& selection, bool fromQueue)
    {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_CurrentSelection    = selection;
            m_SelectionFromQueue  = fromQueue;
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

    void PresentationCore::ApplyStyleSnapshot(const SavedStyle& style)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        ApplySavedStyleToState(style, m_State, m_ActiveFontName);
        ++m_StreamVersion;
    }

    // Arma los providers de un NetworkStreamServer recien creado. Lo llaman
    // tanto ToggleNetworkStream como ToggleChatServer cuando les toca ser
    // los que crean el server compartido (el primero de los dos en pedirlo).
    void PresentationCore::WireNetworkServerProviders(NetworkStreamServer& srv)
    {
        srv.SetSnapshotProvider([this]() -> StreamSnapshot
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

        srv.SetFrameProvider([this]() -> std::vector<uint8_t>
        {
            std::lock_guard<std::mutex> lk(m_FrameMutex);
            return m_LatestFrame;
        });

        srv.SetFontPathProvider([this]() -> std::string
        {
            return GetActiveFontFilePath();
        });
    }

    void PresentationCore::ToggleNetworkStream(bool enable, int port)
    {
        if (enable)
        {
            if (m_NetworkServer && m_NetworkServer->IsRunning())
            {
                // Ya esta corriendo (lo pudo haber arrancado el Chat) —
                // Streaming solo se "suma" como usuario, no reinicia nada.
                std::lock_guard<std::mutex> lk(m_Mutex);
                m_State.isStreamingNet = true;
                m_State.networkURL     = m_NetworkServer->GetBaseURL();
                return;
            }

            m_NetworkServer = std::make_unique<NetworkStreamServer>();
            m_NetworkServer->SetChatStore(&m_ChatMessageStore);
            WireNetworkServerProviders(*m_NetworkServer);

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
            {
                std::lock_guard<std::mutex> lk(m_Mutex);
                m_State.isStreamingNet = false;
                m_State.networkURL.clear();
            }

            m_FrameProviderActive.store(false);
            {
                std::lock_guard<std::mutex> lk(m_FrameMutex);
                m_LatestFrame.clear();
            }

            // El server entero solo se apaga si Chat tampoco lo esta usando
            // — si esta activo, se queda arriba para el (sin video: dejamos
            // de pushear frames arriba, asi que /frame y /stream vuelven a
            // quedar "vacios" para quien mire el video por LAN).
            if (m_NetworkServer && !IsChatRunning())
            {
                m_NetworkServer->Stop();
                m_NetworkServer.reset();
            }
        }
    }

    bool PresentationCore::IsStreamingNet() const
    {
        std::lock_guard<std::mutex> lk(m_Mutex);
        return m_State.isStreamingNet;
    }

    void PresentationCore::ToggleChatServer(bool enable, int port)
    {
        if (enable)
        {
            if (m_NetworkServer && m_NetworkServer->IsRunning())
            {
                // Ya esta corriendo (lo pudo haber arrancado Streaming) —
                // solo conectamos el store de mensajes si todavia no estaba.
                m_NetworkServer->SetChatStore(&m_ChatMessageStore);
                std::lock_guard<std::mutex> lk(m_Mutex);
                m_State.isChatRunning = true;
                m_State.chatURL       = m_NetworkServer->GetBaseURL() + "/chat";
                return;
            }

            m_NetworkServer = std::make_unique<NetworkStreamServer>();
            m_NetworkServer->SetChatStore(&m_ChatMessageStore);
            WireNetworkServerProviders(*m_NetworkServer);

            if (!m_NetworkServer->Start(port))
            {
                m_NetworkServer.reset();
                std::cerr << "[ChatServer] No se pudo iniciar en puerto " << port << ".\n";
                return;
            }

            std::lock_guard<std::mutex> lk(m_Mutex);
            m_State.isChatRunning = true;
            m_State.chatURL       = m_NetworkServer->GetBaseURL() + "/chat";
        }
        else
        {
            {
                std::lock_guard<std::mutex> lk(m_Mutex);
                m_State.isChatRunning = false;
                m_State.chatURL.clear();
            }

            // Igual que del otro lado: el server entero solo se apaga si
            // Streaming tampoco lo esta usando.
            if (m_NetworkServer && !IsStreamingNet())
            {
                m_NetworkServer->Stop();
                m_NetworkServer.reset();
            }
        }
    }

    bool PresentationCore::IsChatRunning() const
    {
        std::lock_guard<std::mutex> lk(m_Mutex);
        return m_State.isChatRunning;
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
outRGB.resize(static_cast<size_t>(w) * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
        {
            std::lock_guard<std::mutex> lk(m_Mutex);
            glClearColor(m_State.bgColor[0], m_State.bgColor[1], m_State.bgColor[2], 1.0f);
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Mismo criterio que RenderProjectorWindow(): el stream de red
        // tampoco debe mostrar un frame entrecortado mientras algo carga.
        if (ShouldShowLoadingScreen()) {
            m_Impl->background.RenderLogo(
                static_cast<unsigned int>(reinterpret_cast<uintptr_t>(GetLoadingLogoTexture())),
                m_LoadingLogoW, m_LoadingLogoH, w, h);
        } else {
            m_Impl->background.Render(w, h);
            m_Impl->overlay.Render();
        }

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
    // glReadPixels entrega fila 0 = abajo de la pantalla. JPEG/PNG
    // esperan fila 0 = arriba. Invertimos filas aca, una sola vez,
    // antes de que el buffer salga hacia el compresor JPEG.
    const size_t rowBytes = static_cast<size_t>(w) * 3;
    for (int row = 0; row < h; ++row)
    {
        const GLubyte* srcRow = ptr + static_cast<size_t>(row) * rowBytes;
        uint8_t* dstRow = outRGB.data() + static_cast<size_t>(h - 1 - row) * rowBytes;
        std::memcpy(dstRow, srcRow, rowBytes);
    }
    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
}

        m_PBOIndex = nextIndex;

        glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
        glViewport(prevViewport[0], prevViewport[1],
                   prevViewport[2], prevViewport[3]);

        return true;
    }

} // namespace ProyecThor::Core