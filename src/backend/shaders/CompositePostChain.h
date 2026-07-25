#pragma once
#include "PostProcessorCRT.h"
#include "PostProcessorGrain.h"
#include "PostProcessorFXAA.h"
#include "PostProcessorSaturation.h"
#include "PostProcessorVignette.h"
#include "PostProcessorBlur.h"
#include "PostProcessorSharpen.h"
#include "PostProcessorBloom.h"
#include "PostProcessorChromaticAberration.h"
#include "PostProcessorVHS.h"
#include "PostProcessorCine.h"
#include "PostProcessorContrast.h"
#include "PostProcessorLuminosity.h"
#include "PostProcessorTAA.h"
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

    void SetSaturationEnabled(bool e)       { m_Saturation.SetEnabled(e); }
    bool GetSaturationEnabled() const       { return m_Saturation.IsEnabled(); }
    void SetSaturationAmount(float v)       { m_Saturation.SetAmount(v); }
    float GetSaturationAmount() const       { return m_Saturation.GetAmount(); }

    void SetVignetteEnabled(bool e)         { m_Vignette.SetEnabled(e); }
    bool GetVignetteEnabled() const         { return m_Vignette.IsEnabled(); }
    void SetVignetteIntensity(float v)      { m_Vignette.SetIntensity(v); }
    float GetVignetteIntensity() const      { return m_Vignette.GetIntensity(); }

    void SetBlurEnabled(bool e)             { m_Blur.SetEnabled(e); }
    bool GetBlurEnabled() const             { return m_Blur.IsEnabled(); }
    void SetBlurIntensity(float v)          { m_Blur.SetIntensity(v); }
    float GetBlurIntensity() const          { return m_Blur.GetIntensity(); }

    void SetSharpenEnabled(bool e)          { m_Sharpen.SetEnabled(e); }
    bool GetSharpenEnabled() const          { return m_Sharpen.IsEnabled(); }
    void SetSharpenIntensity(float v)       { m_Sharpen.SetIntensity(v); }
    float GetSharpenIntensity() const       { return m_Sharpen.GetIntensity(); }

    void SetBloomEnabled(bool e)            { m_Bloom.SetEnabled(e); }
    bool GetBloomEnabled() const            { return m_Bloom.IsEnabled(); }
    void SetBloomIntensity(float v)         { m_Bloom.SetIntensity(v); }
    float GetBloomIntensity() const         { return m_Bloom.GetIntensity(); }

    void SetChromaticAberrationEnabled(bool e)    { m_ChromaticAberration.SetEnabled(e); }
    bool GetChromaticAberrationEnabled() const    { return m_ChromaticAberration.IsEnabled(); }
    void SetChromaticAberrationIntensity(float v) { m_ChromaticAberration.SetIntensity(v); }
    float GetChromaticAberrationIntensity() const { return m_ChromaticAberration.GetIntensity(); }

    void SetVHSEnabled(bool e)              { m_VHS.SetEnabled(e); }
    bool GetVHSEnabled() const              { return m_VHS.IsEnabled(); }
    void SetVHSIntensity(float v)           { m_VHS.SetIntensity(v); }
    float GetVHSIntensity() const           { return m_VHS.GetIntensity(); }

    void SetCineEnabled(bool e)             { m_Cine.SetEnabled(e); }
    bool GetCineEnabled() const             { return m_Cine.IsEnabled(); }
    void SetCineIntensity(float v)          { m_Cine.SetIntensity(v); }
    float GetCineIntensity() const          { return m_Cine.GetIntensity(); }
    void SetCineTint(int t)                 { m_Cine.SetTintInt(t); }
    int  GetCineTint() const                { return m_Cine.GetTintInt(); }

    void SetContrastEnabled(bool e)         { m_Contrast.SetEnabled(e); }
    bool GetContrastEnabled() const         { return m_Contrast.IsEnabled(); }
    void SetContrastAmount(float v)         { m_Contrast.SetAmount(v); }
    float GetContrastAmount() const         { return m_Contrast.GetAmount(); }

    void SetLuminosityEnabled(bool e)       { m_Luminosity.SetEnabled(e); }
    bool GetLuminosityEnabled() const       { return m_Luminosity.IsEnabled(); }
    void SetLuminosityAmount(float v)       { m_Luminosity.SetAmount(v); }
    float GetLuminosityAmount() const       { return m_Luminosity.GetAmount(); }

    void SetTAAEnabled(bool e)              { m_TAA.SetEnabled(e); }
    bool GetTAAEnabled() const              { return m_TAA.IsEnabled(); }
    void SetTAAIntensity(float v)           { m_TAA.SetIntensity(v); }
    float GetTAAIntensity() const           { return m_TAA.GetIntensity(); }

    bool AnyEnabled() const {
        return m_CRT.IsEnabled() || m_Grain.IsEnabled() || m_FXAA.IsEnabled() ||
               m_Saturation.IsEnabled() || m_Vignette.IsEnabled() ||
               m_Blur.IsEnabled() || m_Sharpen.IsEnabled() || m_Bloom.IsEnabled() ||
               m_ChromaticAberration.IsEnabled() ||
               m_VHS.IsEnabled() || m_Cine.IsEnabled() || m_Contrast.IsEnabled() ||
               m_Luminosity.IsEnabled() || m_TAA.IsEnabled();
    }

    // Aplica la MISMA cadena de efectos (mismo habilitado/intensidad que
    // arriba) a una textura arbitraria, en SU PROPIA resolucion chica --
    // pensado para el preview del operador (ViewPanel/Monitor de Control),
    // que antes SIEMPRE mostraba el fondo crudo (sin CRT/Grano/FXAA/
    // Saturación/Viñetado) porque esos solo corrian sobre el composite
    // completo de la viewport "ProjectorLive" real, nunca sobre el recuadro
    // chico del preview. Usa instancias PROPIAS (ver m_Preview*, mas abajo)
    // en vez de las de arriba: esas ya estan dimensionadas a la resolucion
    // COMPLETA del proyector, y correr esa cadena una segunda vez a full-res
    // solo para alimentar un recuadro chico seria carisimo de mas. Devuelve
    // srcTex sin cambios si nada esta activo (costo cero en ese caso).
    GLuint ProcessBackgroundForPreview(GLuint srcTex, int w, int h);

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

    PostProcessorCRT        m_CRT;
    PostProcessorGrain      m_Grain;
    PostProcessorFXAA       m_FXAA;
    PostProcessorSaturation m_Saturation;
    PostProcessorVignette   m_Vignette;
    PostProcessorBlur       m_Blur;
    PostProcessorSharpen    m_Sharpen;
    PostProcessorBloom      m_Bloom;
    PostProcessorChromaticAberration m_ChromaticAberration;
    PostProcessorVHS         m_VHS;
    PostProcessorCine        m_Cine;
    PostProcessorContrast    m_Contrast;
    PostProcessorLuminosity  m_Luminosity;
    PostProcessorTAA         m_TAA;

    // Segundo juego de las mismas, a resolucion de PREVIEW -- ver
    // ProcessBackgroundForPreview().
    PostProcessorCRT        m_PreviewCRT;
    PostProcessorGrain      m_PreviewGrain;
    PostProcessorFXAA       m_PreviewFXAA;
    PostProcessorSaturation m_PreviewSaturation;
    PostProcessorVignette   m_PreviewVignette;
    PostProcessorBlur       m_PreviewBlur;
    PostProcessorSharpen    m_PreviewSharpen;
    PostProcessorBloom      m_PreviewBloom;
    PostProcessorChromaticAberration m_PreviewChromaticAberration;
    PostProcessorVHS         m_PreviewVHS;
    PostProcessorCine        m_PreviewCine;
    PostProcessorContrast    m_PreviewContrast;
    PostProcessorLuminosity  m_PreviewLuminosity;
    PostProcessorTAA         m_PreviewTAA;
    int  m_PreviewW = 0, m_PreviewH = 0;
    bool m_PreviewInitialized = false;

    unsigned int m_CaptureFBO = 0, m_CaptureTex = 0;
    unsigned int m_BlitProgram = 0, m_BlitVAO = 0, m_BlitVBO = 0;

    int   m_W = 0, m_H = 0;
    void* m_LastPlatformHandle    = nullptr;
    bool  m_SubEffectsInitialized = false;
};

} // namespace ProyecThor::Shaders
