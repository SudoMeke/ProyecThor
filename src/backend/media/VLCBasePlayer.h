#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

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

        VLCBasePlayer(int decodeThreads = 2);
        ~VLCBasePlayer();

        VLCBasePlayer(const VLCBasePlayer&)            = delete;
        VLCBasePlayer& operator=(const VLCBasePlayer&) = delete;

        // Carga y reproduce un medio nuevo de forma asincrona, REUTILIZANDO
        // siempre el mismo reproductor. No se crea ni destruye ningun
        // libvlc_media_player_t por clip: solo se le asigna un "media"
        // nuevo (libvlc_media_player_set_media). Al no correr dos
        // pipelines de decode en paralelo durante el cambio (como antes),
        // se elimina la contencion de CPU que causaba el microcorte de
        // audio/video al empezar y terminar clips.
        //
        // Internamente esto NO crea un hilo del sistema operativo por
        // llamada: solo actualiza un unico "pedido pendiente" que procesa
        // un hilo de trabajo persistente (ver m_WorkerThread). Si Play()
        // se llama varias veces muy seguido (usuario hojeando la libreria
        // rapido, o pasando el mouse por varios previews), solo el ultimo
        // pedido termina reproduciendose; los anteriores se descartan sin
        // costo.
        //
        // Si la ruta solicitada coincide con la ruta actualmente bloqueada
        // (ver BlockPath), la llamada se ignora por completo: no se
        // encola ningun pedido nuevo. Esto existe para permitir que un
        // archivo pueda eliminarse del disco sin que algun otro sistema
        // (por ejemplo una cola de reproduccion automatica) lo vuelva a
        // abrir justo despues de haberlo detenido.
        void Play(const std::string& path, bool loop = false, bool startMuted = false);

        // Detiene la reproduccion actual. El reproductor y el dispositivo
        // de audio NO se destruyen, quedan listos para el siguiente Play().
        void Stop();

        // Marca una ruta como bloqueada: mientras este activa, cualquier
        // llamada a Play() con esa misma ruta (comparacion insensible a
        // mayusculas y a separadores de carpeta \ vs /) se ignora. Pensado
        // para usarse justo antes de intentar eliminar el archivo del
        // disco, evitando que algun otro sistema (cola automatica, boton
        // manual, etc.) lo vuelva a abrir mientras se procesa el borrado.
        void BlockPath(const std::string& path);

        // Quita el bloqueo de ruta. Debe llamarse siempre despues de
        // BlockPath, tanto si el borrado tuvo exito como si fallo, para
        // no dejar la reproduccion de ese archivo bloqueada para siempre.
        void UnblockPath();

        void GetAudioLevels(float& left, float& right);
        void SetPause(bool paused);
        bool IsPaused() const { return m_Paused.load(std::memory_order_relaxed); }

        void SetMute(bool mute);
        void SetVolume(int volume);   // 0-200
        void SetSoftwareVolume(float percent);

        void SetPosition(float pos);

        int64_t GetTime() const;
        int64_t GetLength() const;

        void* GetTextureID();
        void  GetVideoSize(int& width, int& height);
        void  UpdateTexture();

        std::vector<AudioDevice> GetAvailableAudioDevices();
        void SetAudioDevice(const std::string& deviceId);

        // true mientras se resuelve/abre un medio nuevo en segundo plano.
        bool IsLoading() const { return m_Loading.load(std::memory_order_relaxed); }

        // Reemplazo directo de la vieja heuristica de tiempo/duracion.
        // Devuelve true UNA sola vez, la primera vez que se consulta
        // despues de que VLC reporto (via su propio evento) que el clip
        // actual termino de forma natural. No se basa en leer GetTime(),
        // por lo tanto es inmune a pausas y a ventanas de carga asincrona.
        bool ConsumeEndReached();

    private:

     int m_DecodeThreads = 2;
     
        libvlc_instance_t* m_Instance = nullptr;

        // Reproductor persistente: se crea UNA vez en el constructor y
        // vive hasta el destructor. Cambiar de clip nunca lo recrea.
        libvlc_media_player_t* m_MediaPlayer = nullptr;
        void* m_VideoCtx = nullptr;
        void* m_AudioCtx = nullptr;

        // Protege unicamente la operacion de "asignarle un medio nuevo al
        // reproductor". El puntero m_MediaPlayer en si nunca cambia, asi
        // que el resto de metodos no necesitan lock.
        mutable std::mutex m_MediaSwapMutex;

        std::atomic<float> m_VolumeMultiplier{1.0f};
        std::atomic<bool>  m_Muted{false};
        std::atomic<bool>  m_EndReached{false};
        std::atomic<bool>  m_Paused{false}; 
        unsigned int m_TextureID = 0;
        int          m_VideoW    = 0;
        int          m_VideoH    = 0;

        // ── Hilo de trabajo persistente ─────────────────────────────────
        // Un unico hilo, creado una vez en el constructor y unido (join)
        // una unica vez en el destructor. Play() NUNCA crea un hilo del
        // sistema operativo: solo escribe el pedido pendiente y notifica.
        // Esto reemplaza el esquema anterior de "un std::thread por cada
        // Play(), detach si el anterior seguia vivo", que podia dejar
        // hilos huerfanos usando 'this' despues de que el objeto ya
        // hubiera sido destruido.
        std::thread             m_WorkerThread;
        mutable std::mutex      m_WorkMutex;
        std::condition_variable m_WorkCV;
        std::string             m_PendingPath;
        bool                    m_PendingLoop       = false;
        bool                    m_PendingStartMuted = false;
        uint64_t                m_PendingGeneration = 0;
        bool                    m_HasPendingRequest = false;
        bool                    m_ShuttingDown      = false;

        // ── Bloqueo de ruta para eliminacion segura ─────────────────────
        // Protegido por m_WorkMutex, igual que el resto del estado de
        // "pedido pendiente" de este bloque.
        bool                    m_PathBlocked       = false;
        std::string             m_BlockedPath;

        std::atomic<bool>     m_Loading{false};
        std::atomic<uint64_t> m_LoadGeneration{0};

        void InitVLC();
        void DestroyVLC();
        void EnsureTexture(int w, int h);
        void CreatePersistentPlayer();

        void WorkerLoop();
        void LoadAndPlay(const std::string& path, bool loop, bool startMuted, uint64_t myGeneration);

        static void OnVlcEvent(const libvlc_event_t* evt, void* userData);
    };

} // namespace ProyecThor::Core