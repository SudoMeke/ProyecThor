#pragma once
#include <GL/glew.h>
#include "backend/media/VLCBasePlayer.h"
#include "backend/shaders/PostProcessorFSR.h"
#include <string>

namespace ProyecThor::Core {

    class BackgroundLayer {
    private:
        // Dos reproductores persistentes: uno visible/audible (activo) y uno
        // en espera (standby) que precarga el siguiente clip en silencio.
        // El swap solo ocurre cuando el standby ya decodifico un frame real,
        // eliminando el congelamiento/corte de audio de reabrir el mismo
        // reproductor para cada clip nuevo.
        VLCBasePlayer m_PlayerA;
        VLCBasePlayer m_PlayerB;
        bool          m_ActiveIsA = true;

        bool   m_SwapPending      = false;
        double m_PendingSwapStart = 0.0;

        // Volumen/mute deseados del layer en vivo, cacheados aqui porque el
        // reproductor que se vuelve activo tras un swap es un objeto
        // distinto y no hereda el volumen del anterior por si solo.
        int  m_TargetVolume = 100;
        bool m_TargetMuted  = false;

        bool  m_IsVideo = false;
        float m_BgColor[3] = { 0.0f, 0.0f, 0.0f };

        ProyecThor::Shaders::PostProcessorFSR m_FSR;
        bool  m_FSREnabled   = true;
        float m_FSRSharpness = 0.2f;

        bool  m_StretchToFill = true;

        VLCBasePlayer& Active();
        VLCBasePlayer& Standby();
        void PerformSwap();

    public:
        BackgroundLayer() = default;
        ~BackgroundLayer() = default;

        void Update();
        void Render(int outputW, int outputH);

        void  SetFSREnabled(bool enabled);
        bool  GetFSREnabled() const;
        void  SetFSRSharpness(float sharpness);
        float GetFSRSharpness() const;

        void  SetStretchToFill(bool stretch);
        bool  GetStretchToFill() const;
        void* GetProcessedTexture(int targetW, int targetH);
        void* GetTextureID();

        // Devuelve el reproductor actualmente visible/audible. Su identidad
        // puede cambiar entre frames si hubo un swap: nunca cachear el
        // puntero, siempre pedirlo de nuevo antes de usarlo.
        VLCBasePlayer* GetPlayer();

        void SetVideo(const std::string& path);
        void SetSolidColor(float r, float g, float b);

        void SetLiveVolume(int volume0to200);
        void SetLiveMute(bool mute);

        void BlockPath(const std::string& path);
        void UnblockPath();
    };

} // namespace ProyecThor::Core