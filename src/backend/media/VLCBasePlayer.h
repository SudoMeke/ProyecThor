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

        VLCBasePlayer(int decodeThreads = 0);
        ~VLCBasePlayer();

        VLCBasePlayer(const VLCBasePlayer&)            = delete;
        VLCBasePlayer& operator=(const VLCBasePlayer&) = delete;

        void Play(const std::string& path, bool loop = false, bool startMuted = false);
        void Stop();

        void BlockPath(const std::string& path);
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

        // true si ya se decodifico al menos un frame de video real (no solo
        // que VLC negocio el formato). Usado para saber cuando un
        // reproductor en modo "standby" ya esta listo para volverse visible
        // sin mostrar un frame negro.
        bool HasVideoFrame() const;

        std::vector<AudioDevice> GetAvailableAudioDevices();
        void SetAudioDevice(const std::string& deviceId);

        bool IsLoading() const { return m_Loading.load(std::memory_order_relaxed); }

        bool ConsumeEndReached();

    private:

        int m_DecodeThreads = 0;

        libvlc_instance_t*       m_Instance    = nullptr;
        libvlc_media_player_t*   m_MediaPlayer = nullptr;
        void* m_VideoCtx = nullptr;
        void* m_AudioCtx = nullptr;

        mutable std::mutex m_MediaSwapMutex;

        std::atomic<float> m_VolumeMultiplier{1.0f};
        std::atomic<bool>  m_Muted{false};
        std::atomic<bool>  m_EndReached{false};
        std::atomic<bool>  m_Paused{false};
        unsigned int m_TextureID = 0;
        int          m_VideoW    = 0;
        int          m_VideoH    = 0;

        std::thread             m_WorkerThread;
        mutable std::mutex      m_WorkMutex;
        std::condition_variable m_WorkCV;
        std::string             m_PendingPath;
        bool                    m_PendingLoop       = false;
        bool                    m_PendingStartMuted = false;
        uint64_t                m_PendingGeneration = 0;
        bool                    m_HasPendingRequest = false;
        bool                    m_ShuttingDown      = false;

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