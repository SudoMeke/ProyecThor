#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include <atomic>
#include <mutex>

struct libvlc_instance_t;
struct libvlc_media_player_t;
struct libvlc_event_t;

namespace ProyecThor::Core {

    class VLCBasePlayer {
    public:

        struct AudioDevice {
            std::string id;
            std::string description;
        };

        // useHardwareDecode controla si esta instancia usa el decodificador
        // de hardware de la GPU (D3D11VA/DXVA2 en Windows, VAAPI/VDPAU en
        // Linux, autodetectado via "--avcodec-hw=any") o decode por
        // software. Se expone como parametro de construccion para poder
        // diagnosticar contencion de sesiones de decode de hardware.
        //
        // forceSilent: garantia estructural de silencio, fijada UNA sola
        // vez al construir el player y valida durante toda su vida. Un
        // player con forceSilent=true NUNCA puede emitir audio real, sin
        // importar que SetMute/SetVolume/SetAudioActive se llamen con
        // valores "audibles" desde cualquier parte del codigo (boton mal
        // cableado, swap de doble buffer, etc.). Se usa para el player de
        // preview (biblioteca), que por requisito de producto jamas debe
        // sonar: solo el monitor a publico puede tener audio real.
        VLCBasePlayer(int decodeThreads = 0, bool useHardwareDecode = true, bool forceSilent = false);
        ~VLCBasePlayer();

        VLCBasePlayer(const VLCBasePlayer&)            = delete;
        VLCBasePlayer& operator=(const VLCBasePlayer&) = delete;

        // NOTA: sincronico. Se ejecuta en el hilo que llama a Play(), sin
        // hilo de fondo propio. Si el archivo tarda en abrir (disco lento,
        // red, o resolucion de YouTube via yt-dlp), el hilo llamante se
        // bloquea durante ese lapso. Es un evento puntual al cambiar de
        // clip, no una carga sostenida por frame.
        void Play(const std::string& path, bool loop = false, bool startMuted = false);
        void Stop();

        void BlockPath(const std::string& path);
        void UnblockPath();

        // En Windows devuelve los picos calculados a partir de los samples
        // interceptados manualmente (WinMM). En Linux, donde el audio lo
        // maneja la salida nativa de libVLC (Pulse/ALSA autodetectado),
        // no hay acceso a los samples crudos, asi que devuelve 0.0f/0.0f.
        void GetAudioLevels(float& left, float& right);

        void SetPause(bool paused);
        bool IsPaused() const { return m_Paused.load(std::memory_order_relaxed); }

        // Si el player es forceSilent, estas tres funciones siguen
        // aceptando el valor pedido (para no romper a quien las llama,
        // ej. sliders de UI), pero el resultado audible real queda
        // siempre en silencio. Ver detalle en VLCBasePlayer.cpp.
        void SetMute(bool mute);
        void SetVolume(int volume);   // 0-200
        void SetSoftwareVolume(float percent);

        // En Windows corta la salida de audio real (HWAVEOUT) de raiz: el
        // callback de audio de VLC retorna de inmediato sin tocar el
        // dispositivo ni hacer busy-wait sobre los buffers. En Linux, sin
        // callback custom de audio, esto se traduce a mute/volumen 0 via
        // libVLC nativo (ver .cpp).
        void SetAudioActive(bool active);

        bool IsForceSilent() const { return m_ForceSilent.load(std::memory_order_relaxed); }

        void SetPosition(float pos);

        int64_t GetTime() const;
        int64_t GetLength() const;

        void* GetTextureID();
        void  GetVideoSize(int& width, int& height);
        void  UpdateTexture();

        // true si ya se decodifico al menos un frame de video real.
        bool HasVideoFrame() const;

        std::vector<AudioDevice> GetAvailableAudioDevices();
        void SetAudioDevice(const std::string& deviceId);

        // Sin hilo de fondo, la carga ya terminó cuando Play() retorna,
        // asi que esto siempre es false. Se mantiene por compatibilidad
        // con quien lo consulte (ej. BackgroundLayer).
        bool IsLoading() const { return false; }

        bool ConsumeEndReached();

    private:

        int  m_DecodeThreads    = 0;
        bool m_UseHardwareDecode = true;

        libvlc_instance_t*       m_Instance    = nullptr;
        libvlc_media_player_t*   m_MediaPlayer = nullptr;
        void* m_VideoCtx = nullptr;
        void* m_AudioCtx = nullptr;

        mutable std::mutex m_MediaSwapMutex;

        std::atomic<float> m_VolumeMultiplier{1.0f};
        std::atomic<bool>  m_Muted{false};
        std::atomic<bool>  m_EndReached{false};
        std::atomic<bool>  m_Paused{false};
        std::atomic<bool>  m_AudioActive{true};
        std::atomic<bool>  m_ForceSilent{false};
        unsigned int m_TextureID = 0;
        int          m_VideoW    = 0;
        int          m_VideoH    = 0;

        bool                    m_PathBlocked       = false;
        std::string             m_BlockedPath;

        std::atomic<uint64_t> m_LoadGeneration{0};

        void InitVLC();
        void DestroyVLC();
        void EnsureTexture(int w, int h);
        void CreatePersistentPlayer();

        void LoadAndPlay(const std::string& path, bool loop, bool startMuted, uint64_t myGeneration);

        static void OnVlcEvent(const libvlc_event_t* evt, void* userData);
    };

} // namespace ProyecThor::Core