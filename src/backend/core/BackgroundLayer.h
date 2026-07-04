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
        //
        // Se les pasa un limite explicito de hilos de decode (en vez de 0 =
        // automatico) porque con avcodec-threads=0 cada instancia reclama un
        // pool de hilos del tamano de los nucleos de CPU disponibles. Con dos
        // instancias full-auto compitiendo por los mismos nucleos durante la
        // ventana de precarga (standby decodificando mientras el activo sigue
        // reproduciendo), ambas se pisan y el resultado es caida de fps en el
        // video que esta en pantalla. Acotando cada una a un numero fijo y
        // razonable de hilos, la suma de ambas se mantiene dentro de un
        // presupuesto de CPU predecible.
        VLCBasePlayer m_PlayerA{ 2 };
        VLCBasePlayer m_PlayerB{ 2 };
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