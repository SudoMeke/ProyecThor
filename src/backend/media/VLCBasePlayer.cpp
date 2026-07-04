#include "VLCBasePlayer.h"

#ifdef _WIN32
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

#include <vlc/vlc.h>
#include <GL/glew.h>
#include <iostream>
#include <cstring>
#include <mutex>
#include <array>
#include <memory>
#include <cstdio>
#include <cmath>
#include <atomic>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#ifndef WAVE_FORMAT_IEEE_FLOAT
#define WAVE_FORMAT_IEEE_FLOAT 0x0003
#endif
#pragma comment(lib, "winmm.lib")
#endif

namespace {

std::string GetDirectYoutubeURL(const std::string& youtubeURL)
{
    std::string command = "yt-dlp.exe -f \"best[ext=mp4]/best\" -g --no-playlist \""
                        + youtubeURL + "\"";
    std::array<char, 1024> buffer;
    std::string result;
#ifdef _WIN32
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(command.c_str(), "r"), _pclose);
#else
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
#endif
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
        result += buffer.data();
    if (!result.empty() && result.back() == '\n') result.pop_back();
    if (!result.empty() && result.back() == '\r') result.pop_back();
    return result;
}

std::string NormalizePathForCompare(const std::string& path)
{
    std::string result = path;
    std::replace(result.begin(), result.end(), '\\', '/');
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

static inline void AtomicUpdateMax(std::atomic<float>& target, float value)
{
    float current = target.load(std::memory_order_relaxed);
    while (value > current &&
           !target.compare_exchange_weak(current, value, std::memory_order_relaxed))
    {
    }
}

struct VLCAudioCtx {
    std::atomic<float> peakL{0.0f};
    std::atomic<float> peakR{0.0f};

    std::atomic<float>* volumeMultiplier = nullptr;
    std::atomic<bool>*  muted            = nullptr;

#ifdef _WIN32
    HWAVEOUT hWaveOut = nullptr;
    static const int NUM_BUFFERS = 8;
    WAVEHDR waveHeaders[NUM_BUFFERS] = {};
    int currentHeader = 0;
    bool deviceInitialized = false;
#endif
};

struct VLCVideoCtx {
    std::mutex mutex;
    void*    pixels = nullptr;
    unsigned width  = 0;
    unsigned height = 0;
    bool     dirty  = false;
};

static int vlc_audio_setup(void** opaque, char* format, unsigned* rate, unsigned* channels)
{
    auto* ctx = static_cast<VLCAudioCtx*>(*opaque);
    std::memcpy(format, "S16N", 4);
    *rate     = 44100;
    *channels = 2;

#ifdef _WIN32
    if (ctx->deviceInitialized)
        return 0;

    WAVEFORMATEX wfx       = {};
    wfx.wFormatTag         = WAVE_FORMAT_PCM;
    wfx.nChannels          = *channels;
    wfx.nSamplesPerSec     = *rate;
    wfx.wBitsPerSample     = 16;
    wfx.nBlockAlign        = (wfx.nChannels * wfx.wBitsPerSample) / 8;
    wfx.nAvgBytesPerSec    = wfx.nSamplesPerSec * wfx.nBlockAlign;

    if (waveOutOpen(&ctx->hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR)
    {
        for (int i = 0; i < VLCAudioCtx::NUM_BUFFERS; ++i)
        {
            ctx->waveHeaders[i].dwBufferLength = 4096 * 8;
            ctx->waveHeaders[i].lpData         = new char[ctx->waveHeaders[i].dwBufferLength];
            waveOutPrepareHeader(ctx->hWaveOut, &ctx->waveHeaders[i], sizeof(WAVEHDR));
        }
        ctx->deviceInitialized = true;
    }
    else
    {
        std::cerr << "[Audio] Error al abrir la salida nativa de Windows.\n";
    }
#endif
    return 0;
}

static void vlc_audio_cleanup(void* opaque)
{
    auto* ctx = static_cast<VLCAudioCtx*>(opaque);
#ifdef _WIN32
    if (ctx->hWaveOut)
        waveOutReset(ctx->hWaveOut);
#endif
}

static void vlc_audio_destroy_device(VLCAudioCtx* ctx)
{
#ifdef _WIN32
    if (ctx->hWaveOut)
    {
        waveOutReset(ctx->hWaveOut);
        for (int i = 0; i < VLCAudioCtx::NUM_BUFFERS; ++i)
        {
            waveOutUnprepareHeader(ctx->hWaveOut, &ctx->waveHeaders[i], sizeof(WAVEHDR));
            delete[] ctx->waveHeaders[i].lpData;
            ctx->waveHeaders[i].lpData = nullptr;
        }
        waveOutClose(ctx->hWaveOut);
        ctx->hWaveOut = nullptr;
    }
    ctx->deviceInitialized = false;
#endif
}

static void vlc_audio_play(void* opaque, const void* samples, unsigned count, int64_t /*pts*/)
{
    auto* ctx = static_cast<VLCAudioCtx*>(opaque);
    if (!ctx->volumeMultiplier || !ctx->muted) return;

    bool  isMuted = ctx->muted->load(std::memory_order_relaxed);
    float vol     = isMuted ? 0.0f : ctx->volumeMultiplier->load(std::memory_order_relaxed);

    const int16_t* pIn = static_cast<const int16_t*>(samples);
    float maxL = 0.0f;
    float maxR = 0.0f;

#ifdef _WIN32
    if (!ctx->hWaveOut) return;

    WAVEHDR& hdr = ctx->waveHeaders[ctx->currentHeader];
    while (hdr.dwFlags & WHDR_INQUEUE)
        Sleep(1);

    int16_t* pOut = reinterpret_cast<int16_t*>(hdr.lpData);

    for (unsigned i = 0; i < count; ++i)
    {
        float sL = pIn[i * 2]     * vol;
        float sR = pIn[i * 2 + 1] * vol;

        if (sL >  32767.0f) sL =  32767.0f;
        else if (sL < -32768.0f) sL = -32768.0f;
        if (sR >  32767.0f) sR =  32767.0f;
        else if (sR < -32768.0f) sR = -32768.0f;

        pOut[i * 2]     = static_cast<int16_t>(sL);
        pOut[i * 2 + 1] = static_cast<int16_t>(sR);

        float nL = std::abs(sL) / 32768.0f;
        float nR = std::abs(sR) / 32768.0f;
        if (nL > maxL) maxL = nL;
        if (nR > maxR) maxR = nR;
    }

    AtomicUpdateMax(ctx->peakL, maxL);
    AtomicUpdateMax(ctx->peakR, maxR);

    hdr.dwBufferLength = count * 2 * sizeof(int16_t);
    waveOutWrite(ctx->hWaveOut, &hdr, sizeof(WAVEHDR));
    ctx->currentHeader = (ctx->currentHeader + 1) % VLCAudioCtx::NUM_BUFFERS;
#endif
}

static unsigned vlc_format(void** opaque, char* chroma, unsigned* width, unsigned* height,
                            unsigned* pitches, unsigned* lines)
{
    auto* ctx = static_cast<VLCVideoCtx*>(*opaque);
    std::lock_guard<std::mutex> lock(ctx->mutex);
    std::memcpy(chroma, "RGBA", 4);
    ctx->width  = *width;
    ctx->height = *height;
    *pitches    = (*width) * 4;
    *lines      = *height;
    if (ctx->pixels) delete[] static_cast<uint8_t*>(ctx->pixels);
    ctx->pixels = new uint8_t[(*pitches) * (*lines)];
    std::memset(ctx->pixels, 0, (*pitches) * (*lines));
    return 1;
}

static void  vlc_cleanup(void* /*opaque*/) {}

static void* vlc_lock(void* opaque, void** planes)
{
    auto* ctx = static_cast<VLCVideoCtx*>(opaque);
    ctx->mutex.lock();
    *planes = ctx->pixels;
    return nullptr;
}

static void vlc_unlock(void* opaque, void* /*picture*/, void* const* /*planes*/)
{
    auto* ctx  = static_cast<VLCVideoCtx*>(opaque);
    ctx->dirty = true;
    ctx->mutex.unlock();
}

static void vlc_display(void* /*opaque*/, void* /*picture*/) {}

} // anonymous namespace

namespace ProyecThor::Core {

VLCBasePlayer::VLCBasePlayer(int decodeThreads)
    : m_DecodeThreads(decodeThreads)
{
    InitVLC();
    CreatePersistentPlayer();
    m_WorkerThread = std::thread(&VLCBasePlayer::WorkerLoop, this);
}

VLCBasePlayer::~VLCBasePlayer()
{
    DestroyVLC();
    if (m_TextureID)
    {
        glDeleteTextures(1, &m_TextureID);
        m_TextureID = 0;
    }
}

void VLCBasePlayer::InitVLC()
{
    std::string threadsArg = "--avcodec-threads=" + std::to_string(m_DecodeThreads);

    const char* args[] = {
        "--no-xlib",
        "--quiet",
        "--no-osd",
        "--no-video-title-show",
        "--avcodec-hw=d3d11va",
        threadsArg.c_str(),
        "--file-caching=300",
        "--clock-jitter=0",
        "--clock-synchro=0",
    };
    m_Instance = libvlc_new(sizeof(args) / sizeof(args[0]), args);
    if (!m_Instance)
        std::cerr << "[VLC] Error al crear instancia libVLC.\n";
}

void VLCBasePlayer::OnVlcEvent(const libvlc_event_t* evt, void* userData)
{
    auto* self = static_cast<VLCBasePlayer*>(userData);
    if (evt->type == libvlc_MediaPlayerEndReached ||
        evt->type == libvlc_MediaPlayerEncounteredError)
    {
        self->m_EndReached.store(true, std::memory_order_relaxed);
    }
}

void VLCBasePlayer::CreatePersistentPlayer()
{
    if (!m_Instance) return;

    m_MediaPlayer = libvlc_media_player_new(m_Instance);
    if (!m_MediaPlayer)
    {
        std::cerr << "[VLC] No se pudo crear el reproductor.\n";
        return;
    }

    auto* vCtx = new VLCVideoCtx();
    m_VideoCtx = vCtx;
    libvlc_video_set_format_callbacks(m_MediaPlayer, vlc_format, vlc_cleanup);
    libvlc_video_set_callbacks(m_MediaPlayer, vlc_lock, vlc_unlock, vlc_display, vCtx);

    auto* aCtx = new VLCAudioCtx();
    aCtx->volumeMultiplier = &m_VolumeMultiplier;
    aCtx->muted            = &m_Muted;
    m_AudioCtx = aCtx;
    libvlc_audio_set_format_callbacks(m_MediaPlayer, vlc_audio_setup, vlc_audio_cleanup);
    libvlc_audio_set_callbacks(m_MediaPlayer, vlc_audio_play,
                               nullptr, nullptr, nullptr, nullptr, aCtx);

    libvlc_event_manager_t* em = libvlc_media_player_event_manager(m_MediaPlayer);
    libvlc_event_attach(em, libvlc_MediaPlayerEndReached,       &VLCBasePlayer::OnVlcEvent, this);
    libvlc_event_attach(em, libvlc_MediaPlayerEncounteredError, &VLCBasePlayer::OnVlcEvent, this);
}

void VLCBasePlayer::DestroyVLC()
{
    Stop();

    {
        std::lock_guard<std::mutex> lock(m_WorkMutex);
        m_ShuttingDown = true;
    }
    m_WorkCV.notify_one();
    if (m_WorkerThread.joinable())
        m_WorkerThread.join();

    if (m_MediaPlayer)
    {
        libvlc_media_player_release(m_MediaPlayer);
        m_MediaPlayer = nullptr;
    }
    if (m_VideoCtx)
    {
        auto* ctx = static_cast<VLCVideoCtx*>(m_VideoCtx);
        delete[] static_cast<uint8_t*>(ctx->pixels);
        delete ctx;
        m_VideoCtx = nullptr;
    }
    if (m_AudioCtx)
    {
        auto* ctx = static_cast<VLCAudioCtx*>(m_AudioCtx);
        vlc_audio_destroy_device(ctx);
        delete ctx;
        m_AudioCtx = nullptr;
    }
    if (m_Instance)
    {
        libvlc_release(m_Instance);
        m_Instance = nullptr;
    }
}

void VLCBasePlayer::EnsureTexture(int w, int h)
{
    if (m_TextureID && m_VideoW == w && m_VideoH == h) return;
    if (m_TextureID) glDeleteTextures(1, &m_TextureID);

    glGenTextures(1, &m_TextureID);
    glBindTexture(GL_TEXTURE_2D, m_TextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_VideoW = w;
    m_VideoH = h;
}

void VLCBasePlayer::Play(const std::string& path, bool loop, bool startMuted)
{
    if (!m_Instance || !m_MediaPlayer) return;

    {
        std::lock_guard<std::mutex> lock(m_WorkMutex);
        if (m_PathBlocked && NormalizePathForCompare(m_BlockedPath) == NormalizePathForCompare(path))
        {
            std::cerr << "[VLC] Play() ignorado, ruta bloqueada: " << path << "\n";
            return;
        }
    }

    uint64_t myGen = ++m_LoadGeneration;
    m_Loading.store(true, std::memory_order_relaxed);

    if (startMuted)
        m_Muted.store(true, std::memory_order_relaxed);

    {
        std::lock_guard<std::mutex> lock(m_WorkMutex);
        m_PendingPath        = path;
        m_PendingLoop        = loop;
        m_PendingStartMuted  = startMuted;
        m_PendingGeneration  = myGen;
        m_HasPendingRequest  = true;
    }
    m_WorkCV.notify_one();
}

void VLCBasePlayer::BlockPath(const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_WorkMutex);
    m_BlockedPath  = path;
    m_PathBlocked  = true;
}

void VLCBasePlayer::UnblockPath()
{
    std::lock_guard<std::mutex> lock(m_WorkMutex);
    m_PathBlocked = false;
    m_BlockedPath.clear();
}

void VLCBasePlayer::WorkerLoop()
{
    for (;;)
    {
        std::string path;
        bool        loop       = false;
        bool        startMuted = false;
        uint64_t    generation = 0;

        {
            std::unique_lock<std::mutex> lock(m_WorkMutex);

            m_WorkCV.wait(lock, [this] { return m_HasPendingRequest || m_ShuttingDown; });

            if (m_ShuttingDown && !m_HasPendingRequest)
                return;

            for (;;)
            {
                uint64_t genAtWaitStart = m_LoadGeneration.load(std::memory_order_relaxed);
                m_WorkCV.wait_for(lock, std::chrono::milliseconds(90));

                if (m_ShuttingDown)
                {
                    m_HasPendingRequest = false;
                    return;
                }
                if (m_LoadGeneration.load(std::memory_order_relaxed) == genAtWaitStart)
                    break;
            }

            path                = m_PendingPath;
            loop                = m_PendingLoop;
            startMuted          = m_PendingStartMuted;
            generation          = m_PendingGeneration;
            m_HasPendingRequest = false;
        }

        LoadAndPlay(path, loop, startMuted, generation);
        m_Loading.store(false, std::memory_order_relaxed);
    }
}

void VLCBasePlayer::LoadAndPlay(const std::string& path, bool loop, bool /*startMuted*/, uint64_t myGeneration)
{
    std::string finalPath = path;
    if (finalPath.find("youtube.com") != std::string::npos ||
        finalPath.find("youtu.be")    != std::string::npos)
    {
        std::string direct = GetDirectYoutubeURL(finalPath);
        if (!direct.empty())
            finalPath = direct;
        else
            std::cerr << "[yt-dlp] Fallo al resolver la URL.\n";
    }

    if (m_LoadGeneration.load(std::memory_order_relaxed) != myGeneration)
        return;

    libvlc_media_t* media = nullptr;
    if (finalPath.rfind("http", 0) == 0 || finalPath.rfind("rtsp", 0) == 0)
        media = libvlc_media_new_location(m_Instance, finalPath.c_str());
    else
        media = libvlc_media_new_path(m_Instance, finalPath.c_str());

    if (!media)
    {
        std::cerr << "[VLC] No se pudo abrir: " << finalPath << "\n";
        return;
    }

    if (loop)
        libvlc_media_add_option(media, "input-repeat=65535");

    {
        std::lock_guard<std::mutex> lock(m_MediaSwapMutex);

        if (m_LoadGeneration.load(std::memory_order_relaxed) != myGeneration)
        {
            libvlc_media_release(media);
            return;
        }

        libvlc_media_player_stop(m_MediaPlayer);
        m_EndReached.store(false, std::memory_order_relaxed);

        libvlc_media_player_set_media(m_MediaPlayer, media);
        libvlc_media_player_play(m_MediaPlayer);
        m_Paused.store(false, std::memory_order_relaxed);
    }

    libvlc_media_release(media);
}

void VLCBasePlayer::Stop()
{
    ++m_LoadGeneration;

    {
        std::lock_guard<std::mutex> lock(m_WorkMutex);
        m_HasPendingRequest = false;
    }

    m_Loading.store(false, std::memory_order_relaxed);
    m_EndReached.store(false, std::memory_order_relaxed);

    std::lock_guard<std::mutex> lock(m_MediaSwapMutex);
    if (m_MediaPlayer)
        libvlc_media_player_stop(m_MediaPlayer);
}

bool VLCBasePlayer::ConsumeEndReached()
{
    return m_EndReached.exchange(false, std::memory_order_relaxed);
}

void VLCBasePlayer::SetMute(bool mute)
{
    m_Muted.store(mute, std::memory_order_relaxed);
}

void VLCBasePlayer::SetVolume(int volume)
{
    float multiplier = static_cast<float>(volume) / 100.0f;
    if (multiplier < 0.0f) multiplier = 0.0f;
    m_VolumeMultiplier.store(multiplier, std::memory_order_relaxed);
}

void VLCBasePlayer::SetSoftwareVolume(float percent)
{
    float multiplier = percent / 100.0f;
    if (multiplier < 0.0f) multiplier = 0.0f;
    m_VolumeMultiplier.store(multiplier, std::memory_order_relaxed);
}

void VLCBasePlayer::SetPause(bool paused)
{
    m_Paused.store(paused, std::memory_order_relaxed);
    if (m_MediaPlayer)
        libvlc_media_player_set_pause(m_MediaPlayer, paused ? 1 : 0);
}

void VLCBasePlayer::SetPosition(float pos)
{
    if (m_MediaPlayer)
        libvlc_media_player_set_position(m_MediaPlayer, pos);
}

int64_t VLCBasePlayer::GetTime() const
{
    return m_MediaPlayer ? libvlc_media_player_get_time(m_MediaPlayer) : 0;
}

int64_t VLCBasePlayer::GetLength() const
{
    return m_MediaPlayer ? libvlc_media_player_get_length(m_MediaPlayer) : -1;
}

void* VLCBasePlayer::GetTextureID()
{
    return m_TextureID
        ? reinterpret_cast<void*>(static_cast<uintptr_t>(m_TextureID))
        : nullptr;
}

void VLCBasePlayer::GetVideoSize(int& width, int& height)
{
    if (m_VideoCtx)
    {
        auto* ctx = static_cast<VLCVideoCtx*>(m_VideoCtx);
        std::lock_guard<std::mutex> lock(ctx->mutex);
        width  = static_cast<int>(ctx->width);
        height = static_cast<int>(ctx->height);
    }
    else
    {
        width = height = 0;
    }
}

void VLCBasePlayer::UpdateTexture()
{
    if (!m_MediaPlayer || !m_VideoCtx) return;

    auto* ctx = static_cast<VLCVideoCtx*>(m_VideoCtx);
    std::lock_guard<std::mutex> lock(ctx->mutex);

    if (!ctx->dirty || !ctx->pixels || ctx->width == 0 || ctx->height == 0) return;

    EnsureTexture(static_cast<int>(ctx->width), static_cast<int>(ctx->height));
    glBindTexture(GL_TEXTURE_2D, m_TextureID);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                    static_cast<int>(ctx->width), static_cast<int>(ctx->height),
                    GL_RGBA, GL_UNSIGNED_BYTE, ctx->pixels);
    glBindTexture(GL_TEXTURE_2D, 0);

    ctx->dirty = false;
}

bool VLCBasePlayer::HasVideoFrame() const
{
    if (!m_VideoCtx) return false;
    auto* ctx = static_cast<VLCVideoCtx*>(m_VideoCtx);
    std::lock_guard<std::mutex> lock(ctx->mutex);
    return ctx->dirty && ctx->pixels != nullptr && ctx->width > 0 && ctx->height > 0;
}

std::vector<VLCBasePlayer::AudioDevice> VLCBasePlayer::GetAvailableAudioDevices()
{
    std::vector<AudioDevice> devices;
    if (!m_MediaPlayer) return devices;

    libvlc_audio_output_device_t* devList = libvlc_audio_output_device_enum(m_MediaPlayer);
    for (auto* p = devList; p != nullptr; p = p->p_next)
    {
        if (p->psz_device && p->psz_description)
            devices.push_back({ p->psz_device, p->psz_description });
    }
    if (devList)
        libvlc_audio_output_device_list_release(devList);

    return devices;
}

void VLCBasePlayer::SetAudioDevice(const std::string& deviceId)
{
    if (m_MediaPlayer)
        libvlc_audio_output_device_set(m_MediaPlayer, nullptr, deviceId.c_str());
}

void VLCBasePlayer::GetAudioLevels(float& left, float& right)
{
    if (!m_AudioCtx)
    {
        left = right = 0.0f;
        return;
    }
    auto* ctx = static_cast<VLCAudioCtx*>(m_AudioCtx);
    left  = ctx->peakL.exchange(0.0f, std::memory_order_relaxed);
    right = ctx->peakR.exchange(0.0f, std::memory_order_relaxed);
}

} // namespace ProyecThor::Core