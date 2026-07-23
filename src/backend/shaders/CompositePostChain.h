#pragma once
#include "PostProcessorCRT.h"
#include "PostProcessorGrain.h"
#include "PostProcessorFXAA.h"
#include <imgui.h>

namespace ProyecThor::Shaders {

// Cadena de post-proceso sobre el COMPOSITE completo de la ventana/viewport
// "ProjectorLive" (fondo + overlays + texto + anuncios + captura, ya
// dibujados por ImGui en su ImDrawData) — a diferencia de FSR, que sigue
// viviendo en BackgroundLayer y solo escala el fondo antes de componer (ver
// PostProcessorFSR.h para la justificacion de por que no se unifican).
//
// Se usa desde el override de ImGuiPlatformIO::Renderer_RenderWindow en
// main.cpp: cuando el viewport que esta por dibujarse es "ProjectorLive",
// PresentationCore::RenderProjectorViewportPostFX() delega aca en vez de
// dejar que corra el renderer default de ImGui.
class CompositePostChain {
public:
    CompositePostChain()  = default;
    ~CompositePostChain() { Destroy(); }

    CompositePostChain(const CompositePostChain&)            = delete;
    CompositePostChain& operator=(const CompositePostChain&) = delete;

    void SetCRTEnabled(bool e)              { m_CRT.SetEnabled(e); }
    bool GetCRTEnabled() const              { return m_CRT.IsEnabled(); }
    void SetCRTScanlineIntensity(float v)   { m_CRT.SetScanlineIntensity(v); }
    float GetCRTScanlineIntensity() const   { return m_CRT.GetScanlineIntensity(); }

    void SetGrainEnabled(bool e)            { m_Grain.SetEnabled(e); }
    bool GetGrainEnabled() const            { return m_Grain.IsEnabled(); }
    void SetGrainIntensity(float v)         { m_Grain.SetIntensity(v); }
    float GetGrainIntensity() const         { return m_Grain.GetIntensity(); }

    void SetFXAAEnabled(bool e)             { m_FXAA.SetEnabled(e); }
    bool GetFXAAEnabled() const             { return m_FXAA.IsEnabled(); }

    bool AnyEnabled() const {
        return m_CRT.IsEnabled() || m_Grain.IsEnabled() || m_FXAA.IsEnabled();
    }

    // Llamado desde el override de Renderer_RenderWindow, con el contexto GL
    // de esa viewport ya activo (ver PresentationCore::
    // RenderProjectorViewportPostFX). defaultRenderFn es el renderer
    // original de ImGui (ImGui_ImplOpenGL3_RenderWindow), usado tal cual
    // cuando ningun efecto esta activo (costo cero) y como "capturador" del
    // composite cuando si.
    void RenderViewport(ImGuiViewport* viewport,
                        void (*defaultRenderFn)(ImGuiViewport*, void*));

    void Destroy();

private:
    void EnsureSized(int w, int h, void* platformHandle);
    void CreateCaptureFBO(int w, int h);
    void DestroyCaptureFBO();
    void EnsureBlitProgram();
    void BlitToCurrentFramebuffer(unsigned int tex, int w, int h);

    PostProcessorCRT   m_CRT;
    PostProcessorGrain m_Grain;
    PostProcessorFXAA  m_FXAA;

    unsigned int m_CaptureFBO = 0, m_CaptureTex = 0;
    unsigned int m_BlitProgram = 0, m_BlitVAO = 0, m_BlitVBO = 0;

    int   m_W = 0, m_H = 0;
    void* m_LastPlatformHandle    = nullptr;
    bool  m_SubEffectsInitialized = false;
};

} // namespace ProyecThor::Shaders
