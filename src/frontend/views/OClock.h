#pragma once
#include <string>
#include <chrono>

namespace ProyecThor::UI {

class GlassRenderer; // fwd decl (ver GlassRenderer.h)

// Modo de transmisión del contador.
//   Off      -> no transmite a ningún lado.
//   MainOnly -> comportamiento clásico: escribe en currentText (pantalla principal / proyector).
//   LANOnly  -> SOLO transmite a los dispositivos conectados por red (no toca la pantalla principal).
//   Both     -> pantalla principal + LAN a la vez.
enum class OClockTransmitMode {
    Off,
    MainOnly,
    LANOnly,
    Both
};

class OClock {
public:
    OClock();
    void Render(GlassRenderer& glass);

private:
    void Start(int minutes, int seconds);
    void Stop();
    void Reset();
    void ApplyPreset(int minutes);

    std::string GetFormattedTime() const;
    float       GetProgressRatio() const; // 0..1 hasta el objetivo (clamped)
    void        SyncTransmission(const std::string& timeStr);

    // ── Lógica de tiempo (cuenta ASCENDENTE con objetivo) ──────────────────
    bool m_IsRunning  = false;
    bool m_IsOvertime = false;
    std::chrono::steady_clock::time_point m_StartTime;
    std::chrono::duration<double> m_ElapsedTime{0};
    std::chrono::duration<double> m_PausedElapsed{0};
    std::chrono::seconds m_TargetTime{0};

    // ── Inputs de usuario ────────────────────────────────────────────────
    int m_InputMin = 5;
    int m_InputSec = 0;

    // ── Opciones de visualización ───────────────────────────────────────
    bool m_ShowProgressBar = true;
    bool m_ShowSignPrefix  = false; // "+45:01" en vez de "45:01" durante overtime

    // ── Transmisión ──────────────────────────────────────────────────────
    OClockTransmitMode m_TransmitMode     = OClockTransmitMode::Off;
    OClockTransmitMode m_PrevTransmitMode = OClockTransmitMode::Off;
};

} // namespace ProyecThor::UI