#pragma once

#include "IPanel.h"
#include "audio/AudioAlbumArt.h"
#include <string>
#include <vector>
#include <cstdint>
#include <atomic>

struct libvlc_instance_t;
struct libvlc_media_player_t;
struct libvlc_media_t;
struct libvlc_event_t;

static void OnMediaEndReached(const libvlc_event_t* event, void* userData);

namespace ProyecThor::UI {

// ─── Datos de una pista ───────────────────────────────────────────────────────

struct AudioTrack {
    std::string filename;
    std::string displayName;
    std::string fullPath;

    // Color de acento procedural (hash del nombre, estable por pista)
    float accentH = 0.0f;

    // Portada embebida. Se extrae de forma lazy al reproducir la pista.
    // coverLoaded = false hasta que se intente la extraccion.
    ProyecThor::Audio::AlbumArt coverArt;
    bool coverLoaded = false;   // true = ya intentamos extraer (puede estar vacia)
};

enum class AudioRepeatMode { None, One, All };

// ─── Disco giratorio ──────────────────────────────────────────────────────────

struct SpinningDiscParams {
    float rotationAngle = 0.0f;
    float targetSpeed   = 0.0f;
    float currentSpeed  = 0.0f;
    float needleAngle   = -0.45f;
    bool  needleLifted  = true;
};

// ─── Panel de audio ───────────────────────────────────────────────────────────

class AudioPanel : public IPanel {
    friend void ::OnMediaEndReached(const libvlc_event_t* event, void* userData);

public:
    AudioPanel();
    ~AudioPanel() override;

    void Render() override;
    std::string GetName() const override { return "Audio"; }

    // Actualiza progreso, animaciones y waveform. Llamar cada frame.
    void Update();

    // Vista de biblioteca (lista de pistas + header)
    void RenderLibraryList();

    // Vista del reproductor (disco, controles, EQ) — se usa en HomePanel
    void RenderPlayerView();

    // ── "En vivo" en el proyector real ───────────────────────────────────
    // true mientras este panel es la fuente del fondo del proyector (ver
    // PresentationCore::SetBackgroundAudio/SetBgTypeLocked, que llama
    // SetLiveBackground(false) automaticamente si el operador manda otra
    // cosa en vivo desde otro lado — video, cancion, biblia).
    bool IsLiveBackground()      const { return m_IsLiveBackground; }
    void SetLiveBackground(bool v)     { m_IsLiveBackground = v;    }

    // Dibuja el fondo "now playing" (disco + caratula + ondas) en el
    // drawlist de la ventana ACTUAL — pensado para llamarse desde dentro
    // del Begin("ProjectorLive") de UIManager (ver ese archivo), asi el
    // ImGui::GetWindowDrawList() que usa RenderSpinningDisc() cae en el
    // proyector real. (x,y,w,h) es el rectangulo completo del proyector.
    void RenderLiveBackground(float x, float y, float w, float h);

    // Acceso a los datos del waveform para que el proyector los dibuje
    const std::vector<float>& GetWaveBars()  const { return m_WaveVec; }
    float                     GetAccentHue() const;
    float                     GetTime()      const { return m_LastTime; }
    bool                      GetIsPlaying() const { return m_IsPlaying && !m_IsPaused; }

    // Acceso publico para el callback de fin de pista
    volatile bool m_TrackEndedFlag = false;

private:
    // ── VLC ───────────────────────────────────────────────────────────────
    void InitVLC();
    void ShutdownVLC();

    // ── Reproduccion ─────────────────────────────────────────────────────
    void Play(int trackIndex);
    void PlayCurrent();
    void Stop();
    void Pause();
    void TogglePlayPause();
    void Next();
    void Previous();
    void SeekTo(float normalizedPosition);
    void SetVolume(int volume);
    void ApplyGain(float gainDb);

    // Extrae y sube a GPU la portada de la pista actual (lazy, solo una vez)
    void EnsureCoverLoaded(int trackIndex);

    // ── Biblioteca ────────────────────────────────────────────────────────
    void RefreshLibrary();
    void ImportAudioFile();

    // ── Render por secciones ──────────────────────────────────────────────
    void RenderHeader();
    void RenderSpinningDisc(float cx, float cy, float radius);
    void RenderNowPlayingCard();
    void RenderProgressBar();
    void RenderTransportControls();
    void RenderVolumeRow();
    void RenderEqualizerSection();
    void RenderPlaylist();

    // ── Helpers ───────────────────────────────────────────────────────────
    std::string FormatTime(int64_t ms) const;
    static float DbToLinear(float dB);
    int  ComputeEffectiveVolume() const;
    static void ComputeTrackAccent(AudioTrack& track);

    // ── VLC ───────────────────────────────────────────────────────────────
    libvlc_instance_t*     m_VLC    = nullptr;
    libvlc_media_player_t* m_Player = nullptr;
    libvlc_media_t*        m_Media  = nullptr;

    // ── Pistas ────────────────────────────────────────────────────────────
    std::vector<AudioTrack> m_Tracks;
    int  m_CurrentTrack = -1;
    bool m_IsPlaying    = false;
    bool m_IsPaused     = false;

    float   m_Progress      = 0.0f;
    int64_t m_CurrentTimeMs = 0;
    int64_t m_TotalTimeMs   = 0;
    bool    m_IsSeeking     = false;

    // ── Audio ─────────────────────────────────────────────────────────────
    int   m_Volume           = 80;
    float m_GainDb           = 0.0f;
    bool  m_Muted            = false;
    int   m_VolumeBeforeMute = 80;

    // ── Ecualizador ───────────────────────────────────────────────────────
    static constexpr int kEqBands = 10;
    float m_EqBands[kEqBands] = { 0.0f };
    float m_EqPreamp          = 0.0f;
    bool  m_EqEnabled         = false;

    static constexpr const char* kBandLabels[kEqBands] = {
        "31", "62", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"
    };

    // ── Modos ─────────────────────────────────────────────────────────────
    AudioRepeatMode m_RepeatMode = AudioRepeatMode::None;
    bool            m_Shuffle    = false;

    // ── Disco giratorio ───────────────────────────────────────────────────
    SpinningDiscParams m_Disc;

    // ── Waveform ──────────────────────────────────────────────────────────
    static constexpr int kWaveBars = 32;
    float m_WaveBars[kWaveBars]    = { 0.0f };
    float m_WaveTargets[kWaveBars] = { 0.0f };
    float m_WaveTimer              = 0.0f;

    // Vector para exponer el waveform al exterior (proyector)
    std::vector<float> m_WaveVec;

    // ── "En vivo" en el proyector real (ver IsLiveBackground/SetLiveBackground) ──
    bool m_IsLiveBackground = false;

    // Tiempo de la ultima animacion
    float m_LastTime = 0.0f;
};

} // namespace ProyecThor::UI