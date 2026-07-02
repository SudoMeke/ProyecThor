// Audio.cpp — AudioPanel refactorizado, estilo Spotify oscuro
// Disco giratorio con albumart procedural, waveform animado,
// controles de transporte modernos y playlist estilizada.

#include "Audio.h"
#include "audio/AudioHelpers.h"
#include "frontend/ui/bin/StyleGeneralApp.h"

#include <vlc/vlc.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cstdlib>

namespace fs = std::filesystem;

// ─── Callback de fin de pista ─────────────────────────────────────────────────
// No llamar a libVLC desde aqui, solo escribir el flag atomico.

static void OnMediaEndReached(const libvlc_event_t* /*event*/, void* userData) {
    auto* panel = static_cast<ProyecThor::UI::AudioPanel*>(userData);
    panel->m_TrackEndedFlag = true;
}

// ─── Helpers de color internos ────────────────────────────────────────────────

namespace {

inline ImU32 Col(float r, float g, float b, float a = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
}

// HSV a RGB (valores en [0,1])
inline void HsvToRgb(float h, float s, float v,
                     float& r, float& g, float& b) {
    ImGui::ColorConvertHSVtoRGB(h, s, v, r, g, b);
}

// Mezcla lineal de dos ImU32
inline ImU32 LerpColor(ImU32 a, ImU32 b, float t) {
    float ar = ((a >>  0) & 0xFF) / 255.0f;
    float ag = ((a >>  8) & 0xFF) / 255.0f;
    float ab_ = ((a >> 16) & 0xFF) / 255.0f;
    float aa = ((a >> 24) & 0xFF) / 255.0f;
    float br = ((b >>  0) & 0xFF) / 255.0f;
    float bg = ((b >>  8) & 0xFF) / 255.0f;
    float bb_ = ((b >> 16) & 0xFF) / 255.0f;
    float ba = ((b >> 24) & 0xFF) / 255.0f;
    return IM_COL32(
        static_cast<int>((ar + (br - ar) * t) * 255),
        static_cast<int>((ag + (bg - ag) * t) * 255),
        static_cast<int>((ab_ + (bb_ - ab_) * t) * 255),
        static_cast<int>((aa + (ba - aa) * t) * 255));
}

// Botón cuadrado/circular con icono de StyleGeneralApp o texto de fallback
static bool IconButton(const char* id,
                       const char* iconKey,
                       const char* fallbackText,
                       ImVec2      size,
                       ImVec4      tint,
                       bool        active    = false,
                       float       rounding  = 8.0f) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rounding);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_Button,
        active ? ImVec4(0.18f, 0.30f, 0.50f, 1.0f)
               : ImVec4(0.10f, 0.12f, 0.16f, 0.0f)); // fondo transparente por defecto
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.08f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.0f, 1.0f, 1.0f, 0.14f));
    ImGui::PushStyleColor(ImGuiCol_Text, tint);

    auto it = StyleGeneralApp::Icons.find(iconKey);
    bool hasIcon = (it != StyleGeneralApp::Icons.end() && it->second.textureID != nullptr);
    std::string label = (hasIcon ? "" : std::string(fallbackText)) + "##" + id;

    bool clicked = ImGui::Button(label.c_str(), size);

    if (hasIcon) {
        ImVec2 bMin = ImGui::GetItemRectMin();
        ImVec2 bMax = ImGui::GetItemRectMax();
        float  pad  = size.x * 0.20f;
        ImGui::GetWindowDrawList()->AddImage(
            it->second.textureID,
            ImVec2(bMin.x + pad, bMin.y + pad),
            ImVec2(bMax.x - pad, bMax.y - pad),
            ImVec2(0, 0), ImVec2(1, 1),
            ImGui::ColorConvertFloat4ToU32(tint));
    }

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
    return clicked;
}

// Slider vertical para el ecualizador
static bool EqBandSlider(const char* id, float* value,
                          float minV, float maxV,
                          float width, float height) {
    ImGui::PushStyleColor(ImGuiCol_FrameBg,          ImVec4(0.10f, 0.12f, 0.16f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,       ImVec4(0.35f, 0.65f, 1.00f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.50f, 0.80f, 1.00f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding,  4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

    ImGui::SetNextItemWidth(width);
    bool changed = ImGui::VSliderFloat(id, ImVec2(width, height), value, minV, maxV, "");

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);
    return changed;
}

// Circulo con recorte (para la portada circular del disco)
// Dibuja N segmentos de la imagen como cuña — simplificado: dibuja un circulo
// relleno de color y luego texto de las iniciales como albumart procedural.
static void DrawDiscArtwork(ImDrawList* dl,
                             ImVec2      center,
                             float       radius,
                             float       hue,
                             const char* initials,
                             float       rotAngle) {
    const int   segments = 64;
    const float pi2      = 6.28318530718f;

    // ── Sombra exterior ──────────────────────────────────────────────────
    for (int s = 6; s >= 1; s--) {
        float sr = radius + s * 3.0f;
        dl->AddCircleFilled(center, sr,
            IM_COL32(0, 0, 0, static_cast<int>(30.0f - s * 3.5f)), segments);
    }

    // ── Anillos de vinilo (fondo oscuro del disco) ────────────────────────
    dl->AddCircleFilled(center, radius, IM_COL32(18, 18, 22, 255), segments);

    // Anillos concéntricos como un vinilo real
    float accentR, accentG, accentB;
    HsvToRgb(hue, 0.70f, 0.85f, accentR, accentG, accentB);

    for (int ring = 1; ring <= 12; ring++) {
        float rr = radius * (0.35f + ring * 0.052f);
        if (rr >= radius) break;
        float alpha = (ring % 3 == 0) ? 0.20f : 0.07f;
        dl->AddCircle(center, rr,
            IM_COL32(static_cast<int>(accentR * 255),
                     static_cast<int>(accentG * 255),
                     static_cast<int>(accentB * 255),
                     static_cast<int>(alpha * 255)),
            segments, 1.0f);
    }

    // ── Zona de la portada (cuadrante central rotado) ─────────────────────
    float artRadius = radius * 0.52f;

    // Degradado de color procedural para el albumart: sectors de color
    const int colorSectors = 6;
    for (int s = 0; s < colorSectors; s++) {
        float angleStart = rotAngle + (pi2 / colorSectors) * s;
        float angleEnd   = angleStart + (pi2 / colorSectors);

        float sH = std::fmod(hue + s * (1.0f / colorSectors), 1.0f);
        float sS = 0.55f + (s % 2) * 0.15f;
        float sV = 0.40f + (s % 3) * 0.12f;
        float sR, sG, sB;
        HsvToRgb(sH, sS, sV, sR, sG, sB);
        ImU32 sColor = IM_COL32(static_cast<int>(sR * 255),
                                static_cast<int>(sG * 255),
                                static_cast<int>(sB * 255), 220);

        // Triangulo de sector (fan)
        const int subSegs = 8;
        for (int ss = 0; ss < subSegs; ss++) {
            float a0 = angleStart + (angleEnd - angleStart) * (ss     / static_cast<float>(subSegs));
            float a1 = angleStart + (angleEnd - angleStart) * ((ss+1) / static_cast<float>(subSegs));
            dl->AddTriangleFilled(
                center,
                ImVec2(center.x + std::cos(a0) * artRadius,
                       center.y + std::sin(a0) * artRadius),
                ImVec2(center.x + std::cos(a1) * artRadius,
                       center.y + std::sin(a1) * artRadius),
                sColor);
        }
    }

    // Degradado radial oscuro encima del albumart para suavizar
    const int fadeSegs = 32;
    for (int f = fadeSegs; f >= 1; f--) {
        float fr    = artRadius * (f / static_cast<float>(fadeSegs));
        float alpha = 0.0f + (1.0f - f / static_cast<float>(fadeSegs)) * 0.45f;
        dl->AddCircleFilled(center, fr,
            IM_COL32(10, 10, 14, static_cast<int>(alpha * 255)), 32);
    }

    // ── Hueco central del disco (spindle hole) ───────────────────────────
    float spindleR = radius * 0.08f;
    dl->AddCircleFilled(center, spindleR, IM_COL32(8, 8, 10, 255), 24);
    dl->AddCircle(center, spindleR,
        IM_COL32(static_cast<int>(accentR * 180),
                 static_cast<int>(accentG * 180),
                 static_cast<int>(accentB * 180), 200), 24, 1.5f);

    // ── Iniciales / título centrado en albumart ───────────────────────────
    // (solo si el area de albumart es suficientemente grande)
    if (artRadius > 24.0f) {
        ImGui::SetWindowFontScale(1.0f);
        ImVec2 textSz = ImGui::CalcTextSize(initials);
        float  scale  = std::min((artRadius * 0.9f) / std::max(textSz.x, 1.0f),
                                 (artRadius * 0.6f) / std::max(textSz.y, 1.0f));
        scale = std::min(scale, 1.6f);

        ImVec2 tPos = ImVec2(center.x - textSz.x * scale * 0.5f,
                             center.y - textSz.y * scale * 0.5f);
        dl->AddText(nullptr, ImGui::GetFontSize() * scale, tPos,
            IM_COL32(255, 255, 255, 160), initials);
    }

    // ── Borde del disco ───────────────────────────────────────────────────
    dl->AddCircle(center, radius,
        IM_COL32(static_cast<int>(accentR * 255),
                 static_cast<int>(accentG * 255),
                 static_cast<int>(accentB * 255), 80),
        segments, 1.5f);
}

// Dibuja el brazo del tocadiscos
static void DrawTonearm(ImDrawList* dl,
                        ImVec2      discCenter,
                        float       discRadius,
                        float       armAngle,  // angulo del brazo en radianes
                        ImU32       color) {
    // Pivote del brazo: arriba a la derecha del disco
    float pivotX = discCenter.x + discRadius * 1.10f;
    float pivotY = discCenter.y - discRadius * 0.60f;

    float armLen = discRadius * 1.25f;

    // Punto de contacto (punta del brazo sobre el disco)
    float tipX = pivotX + std::cos(armAngle + 3.14159f) * armLen;
    float tipY = pivotY + std::sin(armAngle + 3.14159f) * armLen;

    // Linea del brazo
    dl->AddLine(ImVec2(pivotX, pivotY), ImVec2(tipX, tipY), color, 2.0f);

    // Circulo en el pivote
    dl->AddCircleFilled(ImVec2(pivotX, pivotY), 5.0f, color, 12);
    dl->AddCircleFilled(ImVec2(pivotX, pivotY), 2.5f, IM_COL32(20, 20, 26, 255), 12);

    // Cabezal (rectangulo pequeño en la punta)
    float headSize = 5.0f;
    float perpAngle = armAngle + 3.14159f + 1.5708f;
    ImVec2 h0 = ImVec2(tipX + std::cos(perpAngle) * headSize,
                        tipY + std::sin(perpAngle) * headSize);
    ImVec2 h1 = ImVec2(tipX - std::cos(perpAngle) * headSize,
                        tipY - std::sin(perpAngle) * headSize);
    dl->AddLine(h0, h1, color, 3.0f);
}

// Dibuja texto centrado en (cx, cy) con el DrawList — funcion libre interna
static void DrawTextCenteredFree(ImDrawList* dl, ImFont* font, float fontSize,
                                  ImVec2 center, ImU32 color, const char* text) {
    ImVec2 sz = font ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text)
                     : ImGui::CalcTextSize(text);
    dl->AddText(font, fontSize,
                ImVec2(center.x - sz.x * 0.5f, center.y - sz.y * 0.5f),
                color, text);
}

} // namespace anonimo

namespace ProyecThor::UI {

void AudioPanel::RenderLibraryList()
{
    Update();
    RenderHeader();
    ImGui::Spacing();
    RenderPlaylist();
}

void AudioPanel::RenderPlayerView()
{
    RenderNowPlayingCard();
    RenderProgressBar();
    RenderTransportControls();
    RenderVolumeRow();
    RenderEqualizerSection();
}

void AudioPanel::Render()
{
    RenderLibraryList();
    RenderPlayerView();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────

AudioPanel::AudioPanel() {
    try {
        fs::create_directories(ProyecThor::Audio::GetAudioPath());
    } catch (const std::exception& e) {
        std::cerr << "[AudioPanel] No se pudo crear carpeta: " << e.what() << '\n';
    }

    // Inicializar waveform en silencio
    for (int i = 0; i < kWaveBars; i++) {
        m_WaveBars[i]    = 0.02f;
        m_WaveTargets[i] = 0.02f;
    }

    InitVLC();
    RefreshLibrary();
}

AudioPanel::~AudioPanel()
{
    ShutdownVLC();

    // Liberar texturas GL de portadas
    for (auto& track : m_Tracks)
        ProyecThor::Audio::FreeAlbumArtTexture(track.coverArt);
}

// ─────────────────────────────────────────────────────────────────────────────
//  VLC init / shutdown
// ─────────────────────────────────────────────────────────────────────────────

void AudioPanel::InitVLC() {
    const char* args[] = {
        "--no-video",
        "--aout=directsound",
        "--verbose=2"
    };
    m_VLC = libvlc_new(3, args);
    if (!m_VLC) {
        std::cerr << "[AudioPanel] libvlc_new falló\n";
        return;
    }
    m_Player = libvlc_media_player_new(m_VLC);
    if (!m_Player) {
        std::cerr << "[AudioPanel] No se pudo crear el media player\n";
        return;
    }

    libvlc_event_manager_t* em = libvlc_media_player_event_manager(m_Player);
    libvlc_event_attach(em, libvlc_MediaPlayerEndReached, OnMediaEndReached, this);

    libvlc_audio_set_volume(m_Player, ComputeEffectiveVolume());
}

void AudioPanel::ShutdownVLC() {
    if (m_Player) {
        libvlc_media_player_stop(m_Player);
        libvlc_media_player_release(m_Player);
        m_Player = nullptr;
    }
    if (m_Media) {
        libvlc_media_release(m_Media);
        m_Media = nullptr;
    }
    if (m_VLC) {
        libvlc_release(m_VLC);
        m_VLC = nullptr;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers de calculo
// ─────────────────────────────────────────────────────────────────────────────

float AudioPanel::DbToLinear(float dB) {
    return std::pow(10.0f, dB / 20.0f);
}

int AudioPanel::ComputeEffectiveVolume() const {
    float gain    = DbToLinear(m_GainDb);
    float scaled  = static_cast<float>(m_Volume) * gain;
    int   clamped = static_cast<int>(std::round(scaled));
    return std::max(0, std::min(200, clamped));
}

std::string AudioPanel::FormatTime(int64_t ms) const {
    if (ms < 0) ms = 0;
    int totalSec = static_cast<int>(ms / 1000);
    int minutes  = totalSec / 60;
    int seconds  = totalSec % 60;
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << minutes
        << ':' << std::setfill('0') << std::setw(2) << seconds;
    return oss.str();
}

void AudioPanel::ComputeTrackAccent(AudioTrack& track) {
    // Hash simple del nombre para generar un hue estable
    uint32_t hash = 2166136261u;
    for (unsigned char c : track.displayName)
        hash = (hash ^ c) * 16777619u;
    track.accentH = static_cast<float>(hash % 1000) / 1000.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Control de reproduccion
// ─────────────────────────────────────────────────────────────────────────────

void AudioPanel::Play(int trackIndex) {
    if (!m_Player || !m_VLC) return;
    if (trackIndex < 0 || trackIndex >= static_cast<int>(m_Tracks.size())) return;

    libvlc_media_player_stop(m_Player);
    if (m_Media) { libvlc_media_release(m_Media); m_Media = nullptr; }

    m_CurrentTrack = trackIndex;
    const std::string& path = m_Tracks[trackIndex].fullPath;

#ifdef _WIN32
    std::string uri = ProyecThor::Audio::PathToVLCUri(path);
    m_Media = libvlc_media_new_location(m_VLC, uri.c_str());
#else
    m_Media = libvlc_media_new_path(m_VLC, path.c_str());
#endif

    if (!m_Media) {
        std::cerr << "[AudioPanel] No se pudo abrir: " << path << "\n";
        return;
    }

    libvlc_media_player_set_media(m_Player, m_Media);
    libvlc_audio_set_volume(m_Player, ComputeEffectiveVolume());
    libvlc_media_player_play(m_Player);

    if (m_EqEnabled) {
        libvlc_equalizer_t* eq = libvlc_audio_equalizer_new();
        if (eq) {
            libvlc_audio_equalizer_set_preamp(eq, m_EqPreamp);
            for (int b = 0; b < kEqBands; b++)
                libvlc_audio_equalizer_set_amp_at_index(eq, m_EqBands[b],
                                                        static_cast<unsigned>(b));
            libvlc_media_player_set_equalizer(m_Player, eq);
            libvlc_audio_equalizer_release(eq);
        }
    }

    m_IsPlaying      = true;
    m_IsPaused       = false;
    m_TrackEndedFlag = false;
    m_Progress       = 0.0f;
    m_CurrentTimeMs  = 0;
    m_TotalTimeMs    = 0;

    // Arrancar el disco girando
    m_Disc.targetSpeed = 2.0f; // ~1 vuelta cada pi segundos
    m_Disc.needleLifted = false;

    EnsureCoverLoaded(trackIndex);
}

void AudioPanel::PlayCurrent() {
    Play(m_CurrentTrack);
}

void AudioPanel::Stop() {
    if (!m_Player) return;
    libvlc_media_player_stop(m_Player);
    m_IsPlaying     = false;
    m_IsPaused      = false;
    m_Progress      = 0.0f;
    m_CurrentTimeMs = 0;

    m_Disc.targetSpeed  = 0.0f;
    m_Disc.needleLifted = true;
}

void AudioPanel::Pause() {
    if (!m_Player || !m_IsPlaying) return;
    libvlc_media_player_pause(m_Player);
    m_IsPaused = !m_IsPaused;

    m_Disc.targetSpeed = m_IsPaused ? 0.0f : 2.0f;
}

void AudioPanel::TogglePlayPause() {
    if (m_Tracks.empty()) return;
    if (m_CurrentTrack < 0) m_CurrentTrack = 0;

    if (m_IsPlaying) {
        if (m_Player) {
            libvlc_media_player_pause(m_Player);
            m_IsPaused = !m_IsPaused;
            m_Disc.targetSpeed  = m_IsPaused ? 0.0f : 2.0f;
            m_Disc.needleLifted = m_IsPaused;
        }
    } else {
        PlayCurrent();
    }
}

void AudioPanel::Next() {
    if (m_Tracks.empty()) return;

    int next = -1;
    if (m_Shuffle) {
        if (m_Tracks.size() == 1) { next = 0; }
        else {
            do { next = std::rand() % static_cast<int>(m_Tracks.size()); }
            while (next == m_CurrentTrack);
        }
    } else {
        next = m_CurrentTrack + 1;
        if (next >= static_cast<int>(m_Tracks.size())) {
            if (m_RepeatMode == AudioRepeatMode::All) next = 0;
            else { Stop(); return; }
        }
    }
    Play(next);
}

void AudioPanel::Previous() {
    if (m_Tracks.empty()) return;
    if (m_CurrentTimeMs > 3000 && m_CurrentTrack >= 0) {
        SeekTo(0.0f);
        return;
    }
    int prev = m_CurrentTrack - 1;
    if (prev < 0) {
        prev = (m_RepeatMode == AudioRepeatMode::All)
            ? static_cast<int>(m_Tracks.size()) - 1
            : 0;
    }
    Play(prev);
}

void AudioPanel::SeekTo(float normalizedPosition) {
    if (!m_Player) return;
    normalizedPosition = std::max(0.0f, std::min(1.0f, normalizedPosition));
    libvlc_media_player_set_position(m_Player, normalizedPosition);
    m_Progress = normalizedPosition;
}

void AudioPanel::SetVolume(int volume) {
    m_Volume = std::max(0, std::min(200, volume));
    if (!m_Player) return;
    if (!m_Muted)
        libvlc_audio_set_volume(m_Player, ComputeEffectiveVolume());
}

void AudioPanel::ApplyGain(float gainDb) {
    m_GainDb = std::max(-20.0f, std::min(20.0f, gainDb));
    if (!m_Player || m_Muted) return;
    libvlc_audio_set_volume(m_Player, ComputeEffectiveVolume());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Biblioteca
// ─────────────────────────────────────────────────────────────────────────────

void AudioPanel::RefreshLibrary()
{
    // Liberar texturas existentes antes de limpiar el vector
    for (auto& track : m_Tracks)
        ProyecThor::Audio::FreeAlbumArtTexture(track.coverArt);

    m_Tracks.clear();
    const std::string& base = ProyecThor::Audio::GetAudioPath();

    static const std::vector<std::string> kAudioExts = {
        ".mp3", ".flac", ".wav", ".ogg", ".aac", ".m4a", ".wma", ".opus", ".aiff"
    };

    try {
#ifdef _WIN32
        fs::path basePath(ProyecThor::Audio::Utf8ToWide(base));
#else
        fs::path basePath(base);
#endif
        if (!fs::exists(basePath)) { fs::create_directories(basePath); return; }

        for (const auto& entry : fs::directory_iterator(basePath)) {
            if (!entry.is_regular_file()) continue;

            std::string ext = ProyecThor::Audio::WideToUtf8(
                entry.path().extension().wstring());
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            bool supported = false;
            for (const auto& e : kAudioExts)
                if (ext == e) { supported = true; break; }
            if (!supported) continue;

            AudioTrack track;
#ifdef _WIN32
            track.fullPath    = ProyecThor::Audio::WideToUtf8(entry.path().wstring());
            track.filename    = ProyecThor::Audio::WideToUtf8(entry.path().filename().wstring());
            track.displayName = ProyecThor::Audio::WideToUtf8(entry.path().stem().wstring());
#else
            track.fullPath    = entry.path().string();
            track.filename    = entry.path().filename().string();
            track.displayName = entry.path().stem().string();
#endif
            ComputeTrackAccent(track);
            m_Tracks.push_back(std::move(track));
        }
    } catch (const std::exception& e) {
        std::cerr << "[AudioPanel] RefreshLibrary error: " << e.what() << '\n';
    }

    std::sort(m_Tracks.begin(), m_Tracks.end(),
              [](const AudioTrack& a, const AudioTrack& b) {
                  return a.displayName < b.displayName;
              });

    if (m_CurrentTrack >= static_cast<int>(m_Tracks.size()))
        m_CurrentTrack = m_Tracks.empty() ? -1 : 0;
}

void AudioPanel::ImportAudioFile() {
#ifdef _WIN32
    wchar_t filename[MAX_PATH] = {};
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = nullptr;
    ofn.lpstrFilter =
        L"Audio\0*.mp3;*.flac;*.wav;*.ogg;*.aac;*.m4a;*.wma;*.opus;*.aiff\0"
        L"Todos\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile  = MAX_PATH;
    ofn.Flags     = OFN_EXPLORER | OFN_FILEMUSTEXIST |
                    OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn)) {
        try {
            fs::path src{std::wstring(filename)};
            fs::path destDir{ProyecThor::Audio::Utf8ToWide(ProyecThor::Audio::GetAudioPath())};
            fs::path dest = destDir / src.filename();
            fs::create_directories(destDir);
            fs::copy(src, dest, fs::copy_options::overwrite_existing);
            RefreshLibrary();
        } catch (const std::exception& e) {
            std::cerr << "[AudioPanel] Import error: " << e.what() << '\n';
        }
    }
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
//  Update (llamar cada frame)
// ─────────────────────────────────────────────────────────────────────────────

void AudioPanel::Update() {
    float now = static_cast<float>(ImGui::GetTime());
    float dt  = now - m_LastTime;
    if (dt > 0.1f) dt = 0.1f; // clamp para evitar saltos al pausar
    m_LastTime = now;

    // ── Progreso y tiempo ─────────────────────────────────────────────────
    if (m_Player && m_IsPlaying && !m_IsSeeking) {
        float pos = libvlc_media_player_get_position(m_Player);
        m_Progress = (pos >= 0.0f) ? pos : 0.0f;
        m_CurrentTimeMs = libvlc_media_player_get_time(m_Player);
        m_TotalTimeMs   = libvlc_media_player_get_length(m_Player);
    }

    // ── Fin de pista ──────────────────────────────────────────────────────
    if (m_TrackEndedFlag) {
        m_TrackEndedFlag = false;
        if (m_RepeatMode == AudioRepeatMode::One) PlayCurrent();
        else Next();
    }

    // ── Disco giratorio ───────────────────────────────────────────────────
    // Suavizar la velocidad actual hacia la objetivo
    const float speedLerp = 1.8f; // responde en ~0.5s
    m_Disc.currentSpeed += (m_Disc.targetSpeed - m_Disc.currentSpeed) * speedLerp * dt;

    if (m_Disc.currentSpeed > 0.001f) {
        m_Disc.rotationAngle += m_Disc.currentSpeed * dt;
        if (m_Disc.rotationAngle > 6.28318530718f)
            m_Disc.rotationAngle -= 6.28318530718f;
    }

    // Aguja: bajar si reproduciendo, subir si pausado/parado
    float needleTarget = m_Disc.needleLifted ? -0.30f : -0.52f;
    m_Disc.needleAngle += (needleTarget - m_Disc.needleAngle) * 3.0f * dt;

    // ── Waveform simulada ─────────────────────────────────────────────────
    m_WaveTimer += dt;
    if (m_WaveTimer >= 0.10f) {  // refrescar targets cada 100ms
        m_WaveTimer = 0.0f;
        bool active = m_IsPlaying && !m_IsPaused;
        for (int i = 0; i < kWaveBars; i++) {
            if (active) {
                // Generar altura aleatoria ponderada (graves mas altos en extremos)
                float pos    = std::abs(i - kWaveBars * 0.5f) / (kWaveBars * 0.5f);
                float base   = 0.15f + (1.0f - pos) * 0.35f;
                float rnd    = static_cast<float>(std::rand()) / RAND_MAX;
                m_WaveTargets[i] = base + rnd * (0.70f - base);
            } else {
                m_WaveTargets[i] = 0.03f + static_cast<float>(std::rand()) / RAND_MAX * 0.04f;
            }
        }
    }

    // Suavizar waveform hacia targets
    for (int i = 0; i < kWaveBars; i++) {
        float lerp = m_IsPlaying ? 8.0f : 3.0f;
        m_WaveBars[i] += (m_WaveTargets[i] - m_WaveBars[i]) * lerp * dt;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderHeader
// ─────────────────────────────────────────────────────────────────────────────

void AudioPanel::RenderHeader() {
    const float barH = 44.0f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.04f, 0.04f, 0.06f, 1.0f));
    ImGui::BeginChild("##AudioHeader", ImVec2(0.0f, barH), false,
                      ImGuiWindowFlags_NoScrollbar);

    float centerY = (barH - ImGui::GetTextLineHeight()) * 0.5f;

    ImGui::SetCursorPos(ImVec2(14.0f, centerY));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.97f, 1.0f));
    ImGui::TextUnformatted("Reproductor");
    ImGui::PopStyleColor();

    ImGui::SameLine(0.0f, 8.0f);
    ImGui::SetCursorPosY((barH - ImGui::GetTextLineHeight()) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.40f, 0.50f, 1.0f));
    ImGui::Text("— %d pistas", static_cast<int>(m_Tracks.size()));
    ImGui::PopStyleColor();

    // Botones a la derecha
    float btnY  = (barH - 26.0f) * 0.5f;
    float rightX = ImGui::GetContentRegionAvail().x - 180.0f;

    ImGui::SameLine(rightX, 0.0f);
    ImGui::SetCursorPosY(btnY);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f, 0.26f, 0.16f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.36f, 0.22f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.30f, 0.90f, 0.50f, 1.0f));
    if (ImGui::Button("  + Importar  ", ImVec2(0.0f, 26.0f)))
        ImportAudioFile();
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0.0f, 6.0f);
    ImGui::SetCursorPosY(btnY);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.10f, 0.12f, 0.16f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.20f, 0.26f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.45f, 0.55f, 0.70f, 1.0f));
    if (ImGui::Button("  Actualizar  ", ImVec2(0.0f, 26.0f)))
        RefreshLibrary();
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderSpinningDisc — vinilo giratorio con aguja
// ─────────────────────────────────────────────────────────────────────────────
// Función auxiliar para extraer iniciales de manera limpia
std::string ExtractInitials(const std::string& name) {
    std::string initials;
    bool newWord = true;
    for (unsigned char c : name) {
        if (c == ' ' || c == '_' || c == '-') { 
            newWord = true; 
            continue; 
        }
        if (newWord && initials.size() < 2) {
            initials += static_cast<char>(std::toupper(c));
            newWord = false;
        }
    }
    return initials.empty() ? "?" : initials;
}
void AudioPanel::RenderSpinningDisc(float cx, float cy, float radius) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 center(cx, cy);

    // --- 1. Variables de Estado y Metadatos ---
    float hue = 0.58f;
    std::string initials = "PT";
    
    // NOTA: Asumimos que tu struct Track tiene un campo para la textura de la portada.
    ImTextureID coverTexture = (ImTextureID)0;

   // Reemplazar el bloque del if de m_CurrentTrack en RenderSpinningDisc:
if (m_CurrentTrack >= 0 && m_CurrentTrack < static_cast<int>(m_Tracks.size()))
{
    const auto& track = m_Tracks[m_CurrentTrack];
    hue      = track.accentH;
    initials = ExtractInitials(track.displayName);

    // Usar la textura si ya fue subida a GPU
    if (track.coverArt.HasTexture())
        coverTexture = reinterpret_cast<ImTextureID>(
            static_cast<uintptr_t>(track.coverArt.texID));
}

    // --- 2. Plataforma del Tocadiscos (Base) ---
    float baseRadius = radius + 10.0f;
  
    dl->AddCircleFilled(ImVec2(cx + 3.0f, cy + 5.0f), baseRadius, IM_COL32(0, 0, 0, 80), 72);
    dl->AddCircleFilled(center, baseRadius, IM_COL32(28, 28, 34, 255), 72);
    dl->AddCircle(center, baseRadius, IM_COL32(65, 68, 80, 255), 72, 1.5f);
    DrawDiscArtwork(dl, center, radius, hue, initials.c_str(), m_Disc.rotationAngle);

    // --- 4. Renderizar la Portada o las Iniciales en el Centro ---
    float labelRadius = radius * 0.33f; // Tamaño del centro del disco (un 33% del radio total)

    if (coverTexture != (ImTextureID)0) {
        // Borde oscuro para separar limpiamente el vinilo de la portada
        dl->AddCircleFilled(center, labelRadius + 1.0f, IM_COL32(15, 15, 15, 255), 64);

        // Renderizar la portada como un círculo perfecto
        ImVec2 pMin(cx - labelRadius, cy - labelRadius);
        ImVec2 pMax(cx + labelRadius, cy + labelRadius);
        dl->AddImageRounded(coverTexture, pMin, pMax, ImVec2(0,0), ImVec2(1,1), IM_COL32_WHITE, labelRadius);
    } else {
        // Fallback: Centro de color con iniciales si no hay portada
        float r, g, b;
        ImGui::ColorConvertHSVtoRGB(hue, 0.6f, 0.8f, r, g, b); // Usamos ImGui nativo
        ImU32 labelColor = IM_COL32((int)(r*255), (int)(g*255), (int)(b*255), 255);

        dl->AddCircleFilled(center, labelRadius, labelColor, 64);

        // Texto de iniciales centrado
        ImVec2 textSize = ImGui::CalcTextSize(initials.c_str());
        dl->AddText(ImVec2(cx - textSize.x / 2.0f, cy - textSize.y / 2.0f), IM_COL32(255, 255, 255, 255), initials.c_str());
    }

    // Agujero central del disco (spindle metálico)
    dl->AddCircleFilled(center, radius * 0.04f, IM_COL32(20, 20, 24, 255), 24);
    dl->AddCircle(center, radius * 0.04f, IM_COL32(120, 120, 130, 255), 24, 1.0f);

    // --- 5. Renderizar el Brazo/Aguja ---
    float accentR, accentG, accentB;
    ImGui::ColorConvertHSVtoRGB(hue, 0.30f, 0.85f, accentR, accentG, accentB);
    ImU32 armColor = IM_COL32((int)(accentR * 220), (int)(accentG * 220), (int)(accentB * 220), 230);

    DrawTonearm(dl, center, radius, m_Disc.needleAngle, armColor);
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderNowPlayingCard — card superior con disco + info + waveform
// ─────────────────────────────────────────────────────────────────────────────

void AudioPanel::RenderNowPlayingCard() {
    const float cardH   = 160.0f;
    const float discR   = 62.0f;
    const float padding = 14.0f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.07f, 0.10f, 1.0f));
    ImGui::BeginChild("##NowPlaying", ImVec2(0.0f, cardH), false,
                      ImGuiWindowFlags_NoScrollbar);

    ImDrawList* dl     = ImGui::GetWindowDrawList();
    ImVec2      winPos = ImGui::GetWindowPos();
    float       winW   = ImGui::GetWindowWidth();

    // ── Barra de acento superior ──────────────────────────────────────────
    float hue = (m_CurrentTrack >= 0 && m_CurrentTrack < static_cast<int>(m_Tracks.size()))
        ? m_Tracks[m_CurrentTrack].accentH : 0.58f;

    float accentR, accentG, accentB;
    HsvToRgb(hue, 0.70f, 0.85f, accentR, accentG, accentB);
    ImU32 accentColor = IM_COL32(static_cast<int>(accentR * 255),
                                  static_cast<int>(accentG * 255),
                                  static_cast<int>(accentB * 255), 255);

    dl->AddRectFilled(winPos, ImVec2(winPos.x + winW, winPos.y + 2.0f), accentColor);

    // ── Disco giratorio ───────────────────────────────────────────────────
    float discCX = winPos.x + padding + discR + 6.0f;
    float discCY = winPos.y + cardH * 0.50f;
    RenderSpinningDisc(discCX, discCY, discR);

    // ── Texto de la pista ─────────────────────────────────────────────────
    float textStartX = discCX + discR + 22.0f;
    float textAreaW  = winW - (textStartX - winPos.x) - padding;

    if (m_CurrentTrack >= 0 && m_CurrentTrack < static_cast<int>(m_Tracks.size())) {
        const auto& track = m_Tracks[m_CurrentTrack];

        // Estado
        const char* statusStr = m_IsPlaying
            ? (m_IsPaused ? "PAUSADO" : "REPRODUCIENDO")
            : "DETENIDO";
        ImU32 statusColor = m_IsPlaying
            ? (m_IsPaused ? IM_COL32(240, 180, 50, 200) : accentColor)
            : IM_COL32(80, 85, 100, 200);

        dl->AddText(nullptr, ImGui::GetFontSize() * 0.75f,
                    ImVec2(textStartX, winPos.y + 16.0f),
                    statusColor, statusStr);

        // Nombre de la pista (truncado si es muy largo)
        std::string displayStr = track.displayName;
        if (displayStr.size() > 28) displayStr = displayStr.substr(0, 26) + "..";
        dl->AddText(nullptr, ImGui::GetFontSize() * 1.05f,
                    ImVec2(textStartX, winPos.y + 34.0f),
                    IM_COL32(240, 242, 245, 255), displayStr.c_str());

        // Nombre del archivo (subtitulo)
        dl->AddText(nullptr, ImGui::GetFontSize() * 0.80f,
                    ImVec2(textStartX, winPos.y + 56.0f),
                    IM_COL32(90, 95, 110, 255), track.filename.c_str());

        // ── Waveform ──────────────────────────────────────────────────────
        const float waveAreaY = winPos.y + 80.0f;
        const float waveH     = 44.0f;
        const float barW      = std::min(6.0f, textAreaW / kWaveBars - 2.0f);
        const float barGap    = 2.0f;
        const float totalWaveW = kWaveBars * (barW + barGap) - barGap;
        float waveStartX = textStartX;

        for (int i = 0; i < kWaveBars; i++) {
            float barHeight = m_WaveBars[i] * waveH;
            if (barHeight < 2.0f) barHeight = 2.0f;

            float bx  = waveStartX + i * (barW + barGap);
            float by0 = waveAreaY + (waveH - barHeight) * 0.5f;
            float by1 = by0 + barHeight;

            // Color: mas brillante en las barras altas
            float brightness = 0.40f + m_WaveBars[i] * 0.60f;
            float barR, barG, barB;
            HsvToRgb(hue, 0.65f, brightness, barR, barG, barB);
            ImU32 barColor = IM_COL32(static_cast<int>(barR * 255),
                                      static_cast<int>(barG * 255),
                                      static_cast<int>(barB * 255),
                                      static_cast<int>(180 + m_WaveBars[i] * 75));

            dl->AddRectFilled(ImVec2(bx, by0), ImVec2(bx + barW, by1), barColor, 1.5f);
        }

        // Indicador de la posicion actual sobre el waveform
        if (m_IsPlaying && m_TotalTimeMs > 0) {
            float posX = waveStartX + m_Progress * totalWaveW;
            dl->AddLine(ImVec2(posX, waveAreaY),
                        ImVec2(posX, waveAreaY + waveH),
                        IM_COL32(255, 255, 255, 120), 1.5f);
        }

    } else {
        // Sin pista seleccionada
        dl->AddText(nullptr, ImGui::GetFontSize(),
                    ImVec2(textStartX, winPos.y + cardH * 0.40f),
                    IM_COL32(55, 60, 75, 255), "Sin pista seleccionada");
        dl->AddText(nullptr, ImGui::GetFontSize() * 0.80f,
                    ImVec2(textStartX, winPos.y + cardH * 0.40f + 22.0f),
                    IM_COL32(40, 44, 56, 255),
                    "Importa archivos de audio para comenzar");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderProgressBar
// ─────────────────────────────────────────────────────────────────────────────

void AudioPanel::RenderProgressBar() {
    ImGui::Spacing();

    float hue = (m_CurrentTrack >= 0 && m_CurrentTrack < static_cast<int>(m_Tracks.size()))
        ? m_Tracks[m_CurrentTrack].accentH : 0.58f;
    float accentR, accentG, accentB;
    HsvToRgb(hue, 0.65f, 0.90f, accentR, accentG, accentB);

    ImGui::SetCursorPosX(12.0f);

    // Tiempo actual
    std::string tCurrent = FormatTime(m_CurrentTimeMs);
    std::string tTotal   = FormatTime(m_TotalTimeMs);

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.50f, 0.62f, 1.0f));
    ImGui::TextUnformatted(tCurrent.c_str());
    ImGui::PopStyleColor();

    ImGui::SameLine(0.0f, 8.0f);
    float barWidth = ImGui::GetContentRegionAvail().x - 58.0f;

    ImGui::PushStyleColor(ImGuiCol_FrameBg,
        ImVec4(0.12f, 0.14f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,
        ImVec4(accentR, accentG, accentB, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,
        ImVec4(std::min(accentR + 0.15f, 1.0f),
               std::min(accentG + 0.15f, 1.0f),
               std::min(accentB + 0.15f, 1.0f), 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding,  7.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize,   12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

    ImGui::SetNextItemWidth(barWidth);
    bool wasSeekingLastFrame = m_IsSeeking;
    bool sliderChanged = ImGui::SliderFloat("##progress", &m_Progress, 0.0f, 1.0f, "");
    m_IsSeeking = ImGui::IsItemActive();
    if (wasSeekingLastFrame && !m_IsSeeking)
        SeekTo(m_Progress);

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0.0f, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.32f, 0.36f, 0.45f, 1.0f));
    ImGui::TextUnformatted(tTotal.c_str());
    ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderTransportControls
// ─────────────────────────────────────────────────────────────────────────────

void AudioPanel::RenderTransportControls() {
    ImGui::Spacing();

    float hue = (m_CurrentTrack >= 0 && m_CurrentTrack < static_cast<int>(m_Tracks.size()))
        ? m_Tracks[m_CurrentTrack].accentH : 0.58f;
    float accentR, accentG, accentB;
    HsvToRgb(hue, 0.65f, 0.90f, accentR, accentG, accentB);
    ImVec4 accentVec4(accentR, accentG, accentB, 1.0f);

    const float btnSize   = 38.0f;
    const float smallSize = 28.0f;
    // [shuffle] [prev] [play/pause] [next] [repeat]
    const float totalW = smallSize + btnSize * 2.0f + smallSize * 2.0f + 5.0f * 6.0f;
    float startX = (ImGui::GetContentRegionAvail().x - totalW) * 0.5f;
    if (startX < 0.0f) startX = 0.0f;
    ImGui::SetCursorPosX(startX);

    // ── Shuffle ───────────────────────────────────────────────────────────
    ImVec4 shuffleTint = m_Shuffle ? accentVec4 : ImVec4(0.38f, 0.42f, 0.52f, 1.0f);
    if (IconButton("shuffle", "shuffle", "RND", ImVec2(smallSize, smallSize),
                   shuffleTint, m_Shuffle))
        m_Shuffle = !m_Shuffle;

    ImGui::SameLine(0.0f, 6.0f);

    // ── Anterior ──────────────────────────────────────────────────────────
    ImVec4 navTint(0.62f, 0.68f, 0.82f, 1.0f);
    if (IconButton("prev", "anterior", "|<", ImVec2(btnSize, btnSize), navTint))
        Previous();
    ImGui::SameLine(0.0f, 6.0f);

    // ── Play / Pause (boton circular prominente) ──────────────────────────
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 999.0f);
        ImGui::PushStyleColor(ImGuiCol_Button,
            ImVec4(accentR * 0.75f, accentG * 0.75f, accentB * 0.75f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            ImVec4(accentR, accentG, accentB, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
            ImVec4(std::min(accentR + 0.15f, 1.0f),
                   std::min(accentG + 0.15f, 1.0f),
                   std::min(accentB + 0.15f, 1.0f), 1.0f));

        const char* ppIconKey = (m_IsPlaying && !m_IsPaused) ? "pausa" : "play";
        const char* ppText    = (m_IsPlaying && !m_IsPaused) ? "||"    : " > ";
        if (IconButton("pp", ppIconKey, ppText,
                        ImVec2(btnSize, btnSize),
                        ImVec4(0.98f, 0.98f, 1.0f, 1.0f),
                        false, 999.0f))
            TogglePlayPause();

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
    }

    ImGui::SameLine(0.0f, 6.0f);

    // ── Siguiente ─────────────────────────────────────────────────────────
    if (IconButton("next", "siguiente", ">|", ImVec2(btnSize, btnSize), navTint))
        Next();
    ImGui::SameLine(0.0f, 6.0f);

    // ── Repeat ────────────────────────────────────────────────────────────
    ImVec4 repeatTint;
    switch (m_RepeatMode) {
        case AudioRepeatMode::None: repeatTint = ImVec4(0.38f, 0.42f, 0.52f, 1.0f); break;
        case AudioRepeatMode::One:  repeatTint = ImVec4(0.95f, 0.72f, 0.20f, 1.0f); break;
        case AudioRepeatMode::All:  repeatTint = accentVec4; break;
    }
    if (IconButton("repeat", "repetir", "REP", ImVec2(smallSize, smallSize),
                   repeatTint, m_RepeatMode != AudioRepeatMode::None)) {
        switch (m_RepeatMode) {
            case AudioRepeatMode::None: m_RepeatMode = AudioRepeatMode::One;  break;
            case AudioRepeatMode::One:  m_RepeatMode = AudioRepeatMode::All;  break;
            case AudioRepeatMode::All:  m_RepeatMode = AudioRepeatMode::None; break;
        }
    }

    // Badge de modo de repeticion
    if (m_RepeatMode != AudioRepeatMode::None) {
        ImGui::SameLine(0.0f, 3.0f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() +
                             (smallSize - ImGui::GetTextLineHeight()) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, repeatTint);
        ImGui::TextUnformatted(m_RepeatMode == AudioRepeatMode::One ? "1" : "A");
        ImGui::PopStyleColor();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderVolumeRow
// ─────────────────────────────────────────────────────────────────────────────

void AudioPanel::RenderVolumeRow() {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const float labelW  = 72.0f;
    const float muteW   = 24.0f;
    const float gapW    = 6.0f;
    const float valueW  = 52.0f;
    const float sliderW = ImGui::GetContentRegionAvail().x
                          - labelW - muteW - gapW - valueW - 24.0f;

    // ── Volumen ───────────────────────────────────────────────────────────
    ImGui::SetCursorPosX(12.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.48f, 0.60f, 1.0f));
    ImGui::TextUnformatted("Volumen");
    ImGui::PopStyleColor();
    ImGui::SameLine(labelW);

    ImVec4 muteTint = m_Muted
        ? ImVec4(0.95f, 0.35f, 0.35f, 1.0f)
        : ImVec4(0.45f, 0.55f, 0.72f, 1.0f);
    if (IconButton("mute", m_Muted ? "silencio" : "volumen",
                   m_Muted ? "M" : "V",
                   ImVec2(muteW, muteW), muteTint)) {
        if (!m_Muted) {
            m_VolumeBeforeMute = m_Volume;
            m_Muted = true;
            if (m_Player) libvlc_audio_set_volume(m_Player, 0);
        } else {
            m_Muted  = false;
            m_Volume = m_VolumeBeforeMute;
            if (m_Player) libvlc_audio_set_volume(m_Player, ComputeEffectiveVolume());
        }
    }
    ImGui::SameLine(0.0f, gapW);

    ImGui::PushStyleColor(ImGuiCol_FrameBg,          ImVec4(0.10f, 0.12f, 0.16f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,
        m_Muted ? ImVec4(0.42f, 0.22f, 0.22f, 1.0f) : ImVec4(0.35f, 0.65f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.55f, 0.82f, 1.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding,  5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::SetNextItemWidth(sliderW);
    if (ImGui::SliderInt("##vol", &m_Volume, 0, 200, ""))
        SetVolume(m_Volume);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0.0f, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.30f, 0.35f, 0.44f, 1.0f));
    ImGui::Text("%3d%%", m_Volume);
    ImGui::PopStyleColor();

    // ── Ganancia ──────────────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::SetCursorPosX(12.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.48f, 0.60f, 1.0f));
    ImGui::TextUnformatted("Ganancia");
    ImGui::PopStyleColor();
    ImGui::SameLine(labelW + muteW + gapW);

    ImVec4 gainGrab;
    if      (m_GainDb < -0.5f) gainGrab = ImVec4(0.35f, 0.55f, 0.90f, 1.0f);
    else if (m_GainDb >  6.0f) gainGrab = ImVec4(0.95f, 0.40f, 0.25f, 1.0f);
    else if (m_GainDb >  0.5f) gainGrab = ImVec4(0.90f, 0.72f, 0.20f, 1.0f);
    else                       gainGrab = ImVec4(0.35f, 0.82f, 0.55f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_FrameBg,          ImVec4(0.10f, 0.12f, 0.16f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,       gainGrab);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,
        ImVec4(gainGrab.x + 0.10f, gainGrab.y + 0.10f, gainGrab.z + 0.10f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding,  5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::SetNextItemWidth(sliderW);
    if (ImGui::SliderFloat("##gain", &m_GainDb, -20.0f, 20.0f, ""))
        ApplyGain(m_GainDb);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0.0f, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, gainGrab);
    ImGui::Text("%+.1fdB", m_GainDb);
    ImGui::PopStyleColor();

    ImGui::SameLine(0.0f, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.10f, 0.12f, 0.16f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.22f, 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.38f, 0.44f, 0.55f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    if (ImGui::SmallButton("0dB")) ApplyGain(0.0f);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderEqualizerSection
// ─────────────────────────────────────────────────────────────────────────────

void AudioPanel::RenderEqualizerSection() {
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0.08f, 0.12f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.12f, 0.18f, 0.26f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,
        m_EqEnabled ? ImVec4(0.38f, 0.78f, 1.0f, 1.0f)
                    : ImVec4(0.38f, 0.42f, 0.52f, 1.0f));
    bool open = ImGui::CollapsingHeader("  Ecualizador  (10 bandas)");
    ImGui::PopStyleColor(3);
    if (!open) return;

    ImGui::Spacing();
    ImGui::SetCursorPosX(12.0f);
    ImGui::PushStyleColor(ImGuiCol_Text,
        m_EqEnabled ? ImVec4(0.30f, 0.90f, 0.50f, 1.0f)
                    : ImVec4(0.38f, 0.42f, 0.52f, 1.0f));
    bool eqToggled = ImGui::Checkbox("Activar EQ", &m_EqEnabled);
    ImGui::PopStyleColor();

    if (eqToggled && m_Player) {
        if (m_EqEnabled) {
            libvlc_equalizer_t* eq = libvlc_audio_equalizer_new();
            if (eq) {
                libvlc_audio_equalizer_set_preamp(eq, m_EqPreamp);
                for (int b = 0; b < kEqBands; b++)
                    libvlc_audio_equalizer_set_amp_at_index(eq, m_EqBands[b],
                                                            static_cast<unsigned>(b));
                libvlc_media_player_set_equalizer(m_Player, eq);
                libvlc_audio_equalizer_release(eq);
            }
        } else {
            libvlc_media_player_set_equalizer(m_Player, nullptr);
        }
    }

    ImGui::SameLine(0.0f, 20.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.10f, 0.12f, 0.16f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.22f, 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.50f, 0.60f, 0.80f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    if (ImGui::Button("Reset")) {
        for (int b = 0; b < kEqBands; b++) m_EqBands[b] = 0.0f;
        m_EqPreamp = 0.0f;
        if (m_EqEnabled && m_Player) {
            libvlc_equalizer_t* eq = libvlc_audio_equalizer_new();
            if (eq) {
                libvlc_audio_equalizer_set_preamp(eq, 0.0f);
                libvlc_media_player_set_equalizer(m_Player, eq);
                libvlc_audio_equalizer_release(eq);
            }
        }
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    // Preamp
    ImGui::Spacing();
    ImGui::SetCursorPosX(12.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.38f, 0.44f, 0.55f, 1.0f));
    ImGui::TextUnformatted("Preamp");
    ImGui::PopStyleColor();
    ImGui::SameLine(70.0f);

    ImGui::PushStyleColor(ImGuiCol_FrameBg,          ImVec4(0.10f, 0.12f, 0.16f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,       ImVec4(0.70f, 0.55f, 0.95f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.82f, 0.68f, 1.00f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding,  5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    float preampW = ImGui::GetContentRegionAvail().x - 76.0f;
    ImGui::SetNextItemWidth(preampW);
    bool preampChanged = ImGui::SliderFloat("##preamp", &m_EqPreamp, -20.0f, 20.0f, "");
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0.0f, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.55f, 0.95f, 1.0f));
    ImGui::Text("%+.1f", m_EqPreamp);
    ImGui::PopStyleColor();

    if (preampChanged && m_EqEnabled && m_Player) {
        libvlc_equalizer_t* eq = libvlc_audio_equalizer_new();
        if (eq) {
            libvlc_audio_equalizer_set_preamp(eq, m_EqPreamp);
            for (int b = 0; b < kEqBands; b++)
                libvlc_audio_equalizer_set_amp_at_index(eq, m_EqBands[b],
                                                        static_cast<unsigned>(b));
            libvlc_media_player_set_equalizer(m_Player, eq);
            libvlc_audio_equalizer_release(eq);
        }
    }

    ImGui::Spacing();

    // Sliders de bandas verticales
    const float sliderH     = 88.0f;
    const float sliderW_b   = 18.0f;
    const float bandSpacing = 4.0f;
    const float totalBands  = kEqBands * sliderW_b + (kEqBands - 1) * bandSpacing;
    float       eqStartX    = (ImGui::GetContentRegionAvail().x - totalBands) * 0.5f;
    if (eqStartX < 4.0f) eqStartX = 4.0f;

    bool anyBandChanged = false;
    for (int b = 0; b < kEqBands; b++) {
        float cursorX = eqStartX + b * (sliderW_b + bandSpacing);
        ImGui::SetCursorPosX(cursorX);
        ImGui::PushID(b);
        if (EqBandSlider("##band", &m_EqBands[b], -20.0f, 20.0f, sliderW_b, sliderH))
            anyBandChanged = true;
        ImGui::PopID();

        float labelX = cursorX + sliderW_b * 0.5f
                     - ImGui::CalcTextSize(kBandLabels[b]).x * 0.5f;
        ImGui::SetCursorPosX(labelX);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.28f, 0.32f, 0.40f, 1.0f));
        ImGui::TextUnformatted(kBandLabels[b]);
        ImGui::PopStyleColor();

        if (b < kEqBands - 1)
            ImGui::SameLine(eqStartX + (b + 1) * (sliderW_b + bandSpacing));
    }

    if (anyBandChanged && m_EqEnabled && m_Player) {
        libvlc_equalizer_t* eq = libvlc_audio_equalizer_new();
        if (eq) {
            libvlc_audio_equalizer_set_preamp(eq, m_EqPreamp);
            for (int b = 0; b < kEqBands; b++)
                libvlc_audio_equalizer_set_amp_at_index(eq, m_EqBands[b],
                                                        static_cast<unsigned>(b));
            libvlc_media_player_set_equalizer(m_Player, eq);
            libvlc_audio_equalizer_release(eq);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderPlaylist
// ─────────────────────────────────────────────────────────────────────────────

void AudioPanel::RenderPlaylist() {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.32f, 0.36f, 0.44f, 1.0f));
    ImGui::SetCursorPosX(12.0f);
    ImGui::TextUnformatted("LISTA DE REPRODUCCION");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.04f, 0.05f, 0.07f, 1.0f));
    ImGui::BeginChild("##AudioPlaylist", ImVec2(0.0f, 0.0f), false,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);

    if (m_Tracks.empty()) {
        ImGui::SetCursorPos(ImVec2(12.0f, 12.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.28f, 0.32f, 0.40f, 1.0f));
        ImGui::TextWrapped("No hay archivos de audio.\n"
                           "Usa '+ Importar' para agregar MP3, FLAC, WAV, etc.");
        ImGui::PopStyleColor();
    }

    ImDrawList* dl     = ImGui::GetWindowDrawList();
    const float rowH   = ImGui::GetTextLineHeight() + 16.0f;
    const float availW = ImGui::GetContentRegionAvail().x;

    for (int i = 0; i < static_cast<int>(m_Tracks.size()); i++) {
        const auto& track     = m_Tracks[i];
        bool        isCurrent = (m_CurrentTrack == i);
        ImGui::PushID(i);

        ImVec2 rowMin = ImGui::GetCursorScreenPos();
        ImVec2 rowMax = ImVec2(rowMin.x + availW, rowMin.y + rowH);

        ImGui::InvisibleButton("##row", ImVec2(availW, rowH));
        bool clicked  = ImGui::IsItemClicked();
        bool dblClick = ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemHovered();
        bool hovered  = ImGui::IsItemHovered();

        // ── Fondo de la fila ──────────────────────────────────────────────
        if (isCurrent) {
            float hue = track.accentH;
            float r, g, b;
            HsvToRgb(hue, 0.60f, 0.30f, r, g, b);
            dl->AddRectFilled(rowMin, rowMax,
                IM_COL32(static_cast<int>(r * 255),
                         static_cast<int>(g * 255),
                         static_cast<int>(b * 255),
                         static_cast<int>(m_IsPlaying && !m_IsPaused ? 180 : 120)),
                3.0f);

            // Barra de acento izquierda
            float aR, aG, aB;
            HsvToRgb(hue, 0.70f, 0.90f, aR, aG, aB);
            dl->AddRectFilled(rowMin,
                ImVec2(rowMin.x + 3.0f, rowMax.y),
                IM_COL32(static_cast<int>(aR * 255),
                         static_cast<int>(aG * 255),
                         static_cast<int>(aB * 255), 230));
        } else if (hovered) {
            dl->AddRectFilled(rowMin, rowMax, IM_COL32(255, 255, 255, 10), 3.0f);
        }

        // ── Numero de pista ───────────────────────────────────────────────
        std::string numStr = std::to_string(i + 1);
        dl->AddText(ImVec2(rowMin.x + 10.0f, rowMin.y + 8.0f),
            isCurrent
                ? [&]() -> ImU32 {
                      float r, g, b;
                      HsvToRgb(track.accentH, 0.55f, 1.0f, r, g, b);
                      return IM_COL32(static_cast<int>(r*255),
                                     static_cast<int>(g*255),
                                     static_cast<int>(b*255), 255);
                  }()
                : IM_COL32(60, 65, 80, 255),
            numStr.c_str());

        // ── Indicador de reproduccion animado ─────────────────────────────
        if (isCurrent && m_IsPlaying && !m_IsPaused) {
            float t       = static_cast<float>(ImGui::GetTime());
            float baseX   = rowMin.x + 28.0f;
            float baseY   = rowMin.y + rowH * 0.5f;
            float aR, aG, aB;
            HsvToRgb(track.accentH, 0.65f, 1.0f, aR, aG, aB);
            ImU32 barColor = IM_COL32(static_cast<int>(aR*255),
                                      static_cast<int>(aG*255),
                                      static_cast<int>(aB*255), 230);
            for (int bar = 0; bar < 3; bar++) {
                float phase = t * 3.5f + bar * 1.3f;
                float bh    = 3.0f + std::abs(std::sin(phase)) * 8.0f;
                float bx    = baseX + bar * 5.0f;
                dl->AddRectFilled(
                    ImVec2(bx, baseY - bh * 0.5f),
                    ImVec2(bx + 3.5f, baseY + bh * 0.5f),
                    barColor, 1.5f);
            }
        }

        // ── Nombre ────────────────────────────────────────────────────────
        float textX = rowMin.x + (isCurrent ? 48.0f : 36.0f);
        dl->AddText(ImVec2(textX, rowMin.y + 8.0f),
            isCurrent ? IM_COL32(235, 238, 245, 255)
                      : IM_COL32(145, 150, 168, 255),
            track.displayName.c_str());

        // ── Separador ─────────────────────────────────────────────────────
        dl->AddLine(ImVec2(rowMin.x + 10.0f, rowMax.y - 0.5f),
                    ImVec2(rowMax.x  -  8.0f, rowMax.y - 0.5f),
                    IM_COL32(255, 255, 255, 8));

        if (clicked)  m_CurrentTrack = i;
        if (dblClick) Play(i);

        ImGui::SetCursorScreenPos(ImVec2(rowMin.x, rowMax.y));
        ImGui::PopID();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}
void AudioPanel::EnsureCoverLoaded(int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= static_cast<int>(m_Tracks.size()))
        return;

    AudioTrack& track = m_Tracks[trackIndex];
    if (track.coverLoaded)
        return;  // ya intentamos, sea exitoso o no

    track.coverLoaded = true;  // marcar antes de intentar (evita reintentos)

    track.coverArt = ProyecThor::Audio::ExtractAlbumArt(track.fullPath);

    if (track.coverArt.HasData())
        ProyecThor::Audio::UploadAlbumArtToGL(track.coverArt);
        // UploadAlbumArtToGL libera pixels de CPU internamente
}


} // namespace ProyecThor::UI