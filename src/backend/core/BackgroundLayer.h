#pragma once
#include <GL/glew.h>
#include "backend/media/VLCBasePlayer.h"
#include "backend/shaders/PostProcessorFSR.h"
#include <string>
#include <vector>
#include <deque>
#include <algorithm>

namespace ProyecThor::Core {

    class BackgroundLayer {
    private:
        VLCBasePlayer m_PlayerA;
        VLCBasePlayer m_PlayerB;
        bool          m_ActiveIsA = true;

        bool   m_SwapPending      = false;
        double m_PendingSwapStart = 0.0;

        // Momento (NowSeconds()) en que Standby() quedo Ready durante un
        // swap pendiente — 0.0 mientras no lo esta. Desde ahi se cuenta un
        // crossfade corto y fijo (kSwapBlendSeconds) antes de completar el
        // swap de verdad, para que el cambio de fondo se vea como una
        // transicion fluida (frame final -> frame inicial) en vez de un
        // corte seco. Ver Update().
        double m_SwapReadyAt = 0.0;
        // Momento en que se cumplio el asentamiento (kSwapSettleSeconds
        // despues de Ready) — 0.0 hasta entonces. Desde ahi se cuenta el
        // crossfade real (kSwapBlendSeconds).
        double m_SwapSettledAt = 0.0;

        // Margen de "asentamiento": los primeros frames decodificados de un
        // codec recien abierto a veces son artefactos del decoder
        // "calentando" (frame parcial/negro/con colores mal, comun en
        // hardware decode o con B-frames) — HasVideoFrame() ya da true con
        // el primer frame, que puede ser justo uno de esos. Sin esperar un
        // poco antes de empezar a MOSTRAR standby en el blend, el publico
        // podia ver un flash breve de ese frame roto. Este margen NO
        // demora el swap final (sigue siendo kSwapBlendSeconds despues de
        // asentar) — solo demora el INICIO del blend visible. El mismo
        // margen tambien se usa antes de PAUSAR el prefetch (ver Update()),
        // asi que cuando llega a esta parte el standby ya viene de un
        // frame confirmado estable, no solo del primero que aparecio.
        static constexpr double kSwapSettleSeconds = 0.3;
        static constexpr double kSwapBlendSeconds  = 0.2;
        // Mismo rol que m_SwapReadyAt pero para el prefetch (ver Update()):
        // momento en que el prefetch quedo Ready por primera vez, para
        // saber cuando ya paso kSwapSettleSeconds y es seguro pausarlo.
        double m_PrefetchReadyAt = 0.0;
        // Limite de emergencia: si standby no llega a Ready ni a Error en
        // este tiempo (carga realmente colgada), se abandona el swap y se
        // sigue mostrando el fondo anterior — nunca se fuerza un corte a
        // contenido que no esta listo (ver Update()).
        static constexpr double kSwapGiveUpSeconds = 15.0;

        // Precarga adelantada sin swap automatico (ver Prefetch()/
        // CommitPrefetch()) — usada por la cola del Monitor para dejar el
        // SIGUIENTE clip abierto y pausado en su primer frame mientras el
        // actual todavia se esta reproduciendo, para que la transicion en
        // CommitPrefetch() sea un corte instantaneo (nada que esperar) en
        // vez de recien empezar a abrir el archivo en ese momento.
        // Separado de m_SwapPending: Prefetch() llena standby pero NO arma
        // el gate de Update(), asi que no se dispara solo.
        bool        m_PrefetchArmed = false;
        std::string m_PrefetchedPath;

        // Historial de cuanto tardo en quedar listo (Ready) el ultimo
        // puñado de swaps, para poder mostrarle al operador un tiempo
        // estimado de carga (ver GetEstimatedLoadSeconds()).
        std::deque<float> m_RecentLoadDurations;
        static constexpr size_t kMaxLoadSamples = 8;
        static constexpr float  kDefaultEtaSeconds = 1.5f;

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

        // Dibuja una imagen (el logo de pantalla de carga, ver
        // PresentationCore::ShouldShowLoadingScreen) letterboxeada dentro
        // del viewport de salida, reusando el mismo blit raw-GL que el
        // fondo normal (BlitTexture) — para la salida REAL al publico,
        // llamado en vez de Render() mientras el logo esta activo.
        void RenderLogo(unsigned int logoTex, int logoW, int logoH, int outputW, int outputH);

        void  SetFSREnabled(bool enabled);
        bool  GetFSREnabled() const;
        void  SetFSRSharpness(float sharpness);
        float GetFSRSharpness() const;

        void  SetStretchToFill(bool stretch);
        bool  GetStretchToFill() const;
        void* GetProcessedTexture(int targetW, int targetH);
        void* GetTextureID();

        // Textura cruda de Standby() (sin FSR) + progreso de blend, para que
        // un rendering ImGui (ver UIManager "ProjectorLive") pueda blendear
        // el mismo crossfade que ya hace Render() via BlitTexture, sin
        // duplicar la logica de decidir CUANDO blendear (ver IsSwapPending/
        // HasVideoFrame ya expuestos).
        void*  GetStandbyTextureID();
        float  GetTransitionProgress() const { return m_TransitionProgress; }
        bool   StandbyHasFrame();

        VLCBasePlayer* GetPlayer();

        void SetTransitionProgress(float p) { m_TransitionProgress = std::clamp(p, 0.0f, 1.0f); }

        // allowAudio=false para fondos decorativos (BackgroundsPanel):
        // estructuralmente no podran sonar aunque se este "al aire". Default
        // false: el caller tiene que pedir audio explicitamente (monitor).
        void SetVideo(const std::string& path, bool allowAudio = false);

        // Precarga path en standby, pausado apenas decodifica su primer
        // frame (ver el guard de pausa en Update()) — SIN armar el swap
        // automatico (a diferencia de SetVideo()). Llamar a
        // CommitPrefetch() cuando corresponda mostrarlo: como ya esta
        // listo y quieto en el frame 0, el corte es instantaneo.
        void Prefetch(const std::string& path, bool allowAudio = false);

        // Arma el swap para lo que ya este precargado via Prefetch() (lo
        // despausa en el instante del swap). Si no hay nada precargado (o
        // no coincide, ej. la cola se reordeno), cae a SetVideo(path,...)
        // como carga en frio normal.
        void CommitPrefetch(const std::string& path, bool allowAudio = false);

        bool  IsSwapPending() const { return m_SwapPending; }
        float GetEstimatedLoadSeconds() const;

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