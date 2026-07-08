#pragma once
#include <GL/glew.h>
#include "backend/media/VLCBasePlayer.h"
#include "backend/shaders/PostProcessorFSR.h"
#include <string>

namespace ProyecThor::Core {

    class BackgroundLayer {
    private:
        VLCBasePlayer m_PlayerA;
        VLCBasePlayer m_PlayerB;
        bool          m_ActiveIsA = true;

        bool   m_SwapPending      = false;
        double m_PendingSwapStart = 0.0;

        int  m_TargetVolume = 100;
        bool m_TargetMuted  = true;

        // Gate real de audio al publico. Solo cuando esta en true el
        // player activo puede sonar de verdad (ver SetPubliclyLive). Sin
        // esto, cargar un video de fondo (SetVideo) o mover el doble
        // buffer (PerformSwap) podia dejar audio sonando sin que el
        // operador hubiese puesto nada al aire todavia.
        bool m_IsLiveToPublic = false;

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
        // forceSilentAudio=true construye ambos players internos como
        // permanentemente mudos (ver VLCBasePlayer::m_ForceSilent). Se usa
        // para la instancia de preview de biblioteca, que por requisito
        // de producto NUNCA debe emitir audio, sin importar que boton la
        // toque.
        explicit BackgroundLayer(bool forceSilentAudio = false);
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

        VLCBasePlayer* GetPlayer();

        void SetVideo(const std::string& path);
        void SetSolidColor(float r, float g, float b);

        // Activa/desactiva el gate de audio al publico. Llamado por
        // PresentationCore::SetProjecting(). Al pasar a false, el audio
        // se corta de inmediato en ambos players (activo y standby), sin
        // importar el volumen/mute configurado.
        void SetPubliclyLive(bool live);
        bool IsPubliclyLive() const { return m_IsLiveToPublic; }

        void SetLiveVolume(int volume0to200);
        void SetLiveMute(bool mute);

        void BlockPath(const std::string& path);
        void UnblockPath();
    };

} // namespace ProyecThor::Core