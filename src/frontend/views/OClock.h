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

// Sentido del conteo (solo aplica en OClockMode::Timer).
//   CountUp   -> cronómetro clásico: 00:00 -> objetivo (y sigue ascendiendo en overtime).
//   CountDown -> cuenta regresiva: objetivo -> 00:00 (y sigue ascendiendo el excedente, con signo "-").
// El cruce de "final" (m_IsOvertime) es el mismo evento en ambos casos: elapsed >= target.
// Lo unico que cambia es como se formatea el numero en pantalla.
enum class OClockDirection {
    CountUp,
    CountDown
};

// Modo de operacion general del widget.
//   Timer     -> cronometro/cuenta regresiva con objetivo (comportamiento original).
//   WallClock -> muestra la hora actual del dispositivo (reloj de pared), sin
//                objetivo ni concepto de overtime.
enum class OClockMode {
    Timer,
    WallClock
};

class OClock {
public:
    OClock();

    // Recalcula el tiempo interno (si esta en modo Timer y corriendo) y
    // sincroniza la transmision hacia el proyector/LAN via SyncTransmission().
    //
    // IMPORTANTE: esto debe llamarse UNA VEZ POR FRAME desde el tick global
    // de la aplicacion (junto a las demas actualizaciones "de fondo", ej.
    // PresentationCore::Get().Update()), SIN IMPORTAR si la pestaña/panel de
    // OClock esta actualmente visible o no. Render() tambien lo llama
    // internamente para que el numero mostrado en pantalla este siempre
    // fresco mientras el panel esta abierto, pero eso NO alcanza por si
    // solo: si esta llamada global falta, la transmision hacia el publico
    // se congela apenas el usuario cambia de pestaña, aunque el tiempo
    // interno siga corriendo bien.
    void Update();

    void Render(GlassRenderer& glass);

private:
    void Start(int minutes, int seconds);
    void Stop();
    void Reset();
    void ApplyPreset(int minutes);

    std::string GetFormattedTime() const;
    float       GetProgressRatio() const; // 0..1 hasta el objetivo (clamped). Solo Timer.
    void        SyncTransmission(const std::string& timeStr);
    void        RenderStyleSelector();
    void        RenderModeSelector();
    void        RenderDirectionSelector();
    void        RenderWallClockOptions();
    void        RenderTitleSection();

    // ── Título / mensaje sobre el reloj ──────────────────────────────────
    std::string GetCurrentTitle() const;
    void        AdvanceTitle();

    // ── Modo de operacion ─────────────────────────────────────────────────
    OClockMode m_Mode = OClockMode::Timer;

    // ── Lógica de tiempo (cuenta con objetivo, ascendente o descendente) ──
    bool m_IsRunning  = false;
    bool m_IsOvertime = false;
    std::chrono::steady_clock::time_point m_StartTime;
    std::chrono::duration<double> m_ElapsedTime{0};
    std::chrono::duration<double> m_PausedElapsed{0};
    std::chrono::seconds m_TargetTime{0};

    OClockDirection m_Direction = OClockDirection::CountUp;

    // ── Inputs de usuario (modo Timer) ───────────────────────────────────
    int m_InputMin = 5;
    int m_InputSec = 0;

    // ── Opciones de formato (modo WallClock) ─────────────────────────────
    bool m_WallClock24h        = true;
    bool m_WallClockShowSeconds = true;

    // ── Opciones de visualización (modo Timer) ───────────────────────────
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