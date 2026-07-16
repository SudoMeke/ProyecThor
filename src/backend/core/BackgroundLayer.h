#pragma once
#include <GL/glew.h>
#include "backend/media/VLCBasePlayer.h"
#include "backend/shaders/PostProcessorFSR.h"
#include <string>
#include <vector>
#include <algorithm>

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
        float m_TransitionProgress = 1.0f;

        // Gate real de audio al publico. Solo cuando esta en true el
        // player activo puede sonar de verdad (ver SetPubliclyLive). Sin
        // esto, cargar un video de fondo (SetVideo) o mover el doble
        // buffer (PerformSwap) podia dejar audio sonando sin que el
        // operador hubiese puesto nada al aire todavia.
        bool m_IsLiveToPublic = false;

        // Indica si el contenido actualmente cargado (o pendiente de swap)
        // tiene PERMITIDO sonar cuando m_IsLiveToPublic sea true. Se fija
        // en cada llamada a SetVideo() segun quien la invoque:
        // true  -> viene de "Videos"/cola (audio permitido)
        // false -> viene de "Fondos" (BackgroundsPanel/LayersBgTab), NUNCA
        //          suena sin importar el estado de m_IsLiveToPublic.
        bool m_ContentAllowsAudio = true;

        // Dispositivo de salida de audio seleccionado por el operador
        // (vacio o "default" = predeterminado del sistema). Se aplica a
        // AMBOS players (m_PlayerA y m_PlayerB) apenas se selecciona, para
        // que no importe cual este activo hoy ni cual pase a estarlo tras
        // un swap: el audio siempre sale por este dispositivo.
        std::string m_AudioDeviceId;

#ifdef _WIN32
    bool m_FlipVideoY = false;
#else
    bool m_FlipVideoY = true;  // default: VAAPI en Linux suele invertir
#endif
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

        void SetFlipVideoY(bool flip) { m_FlipVideoY = flip; }
        bool GetFlipVideoY() const { return m_FlipVideoY; }

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

        void SetTransitionProgress(float p) { m_TransitionProgress = std::clamp(p, 0.0f, 1.0f); }

        // allowAudio=false para fondos decorativos (BackgroundsPanel):
        // estructuralmente no podran sonar aunque se este "al aire".
        void SetVideo(const std::string& path, bool allowAudio = true);

        void SetSolidColor(float r, float g, float b);

        // Activa/desactiva el gate de audio al publico. Llamado por
        // PresentationCore::SetProjecting(). Al pasar a false, el audio
        // se corta de inmediato en ambos players (activo y standby), sin
        // importar el volumen/mute configurado.
        void SetPubliclyLive(bool live);
        bool IsPubliclyLive() const { return m_IsLiveToPublic; }

        void SetLiveVolume(int volume0to200);
        void SetLiveMute(bool mute);

        // ── Dispositivo de salida de audio ───────────────────────────────
        // Enumera los dispositivos de audio disponibles en el sistema
        // (altavoces, HDMI, interfaces USB, etc.) para mostrarlos en un
        // combo/selector de UI.
        std::vector<VLCBasePlayer::AudioDevice> GetAvailableAudioDevices();

        // Selecciona el dispositivo por el que debe salir el audio del
        // fondo. deviceId vacio o "default" usa el dispositivo
        // predeterminado del sistema. Se aplica de inmediato a ambos
        // players internos (activo y standby), asi que el cambio tiene
        // efecto sin importar que este sonando en este momento.
        void SetAudioOutputDevice(const std::string& deviceId);
        std::string GetAudioOutputDevice() const { return m_AudioDeviceId; }

        void BlockPath(const std::string& path);
        void UnblockPath();
    };

} // namespace ProyecThor::Core