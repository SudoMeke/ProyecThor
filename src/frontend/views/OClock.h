#pragma once
#include <string>
#include <vector>
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

// Sentido del conteo.
//   CountUp   -> cronómetro clásico: 00:00 -> objetivo (y sigue ascendiendo en overtime).
//   CountDown -> cuenta regresiva: objetivo -> 00:00 (y sigue ascendiendo el excedente, con signo "-").
// El cruce de "final" (m_IsOvertime) es el mismo evento en ambos casos: elapsed >= target.
// Lo unico que cambia es como se formatea el numero en pantalla.
enum class OClockDirection {
    CountUp,
    CountDown
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
    void        RenderStyleSelector();
    void        RenderDirectionSelector();
    void        RenderTitleSection();

    // ── Título / mensaje sobre el reloj ──────────────────────────────────
    std::string GetCurrentTitle() const;
    void        AdvanceTitle();

    // ── Lógica de tiempo (cuenta con objetivo, ascendente o descendente) ──
    bool m_IsRunning  = false;
    bool m_IsOvertime = false;
    std::chrono::steady_clock::time_point m_StartTime;
    std::chrono::duration<double> m_ElapsedTime{0};
    std::chrono::duration<double> m_PausedElapsed{0};
    std::chrono::seconds m_TargetTime{0};

    OClockDirection m_Direction = OClockDirection::CountUp;

    // ── Inputs de usuario ────────────────────────────────────────────────
    int m_InputMin = 5;
    int m_InputSec = 0;

    // ── Opciones de visualización ───────────────────────────────────────
    bool m_ShowProgressBar = true;
    bool m_ShowSignPrefix  = false; // "+45:01" (CountUp) o "-00:15" (CountDown) durante overtime

    // ── Transmisión ──────────────────────────────────────────────────────
    OClockTransmitMode m_TransmitMode     = OClockTransmitMode::Off;
    OClockTransmitMode m_PrevTransmitMode = OClockTransmitMode::Off;

    // ── Estilos definidos por el usuario ─────────────────────────────────
    std::string m_StyleName;       // estilo normal (mientras corre / antes del final)
    std::string m_FinalStyleName;  // estilo aplicado al llegar al final (overtime).
                                    // Si esta vacio, se usa el estilo normal + color
                                    // de peligro forzado (comportamiento clasico).

    // ── Título / mensaje editable, avanzable manualmente ─────────────────
    std::vector<std::string> m_Titles;
    int  m_TitleIndex = -1;             // -1 = sin título activo
    char m_TitleInputBuf[128] = "";
};

} // namespace ProyecThor::UI