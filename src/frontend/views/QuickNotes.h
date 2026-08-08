#pragma once
#include <string>
#include <array>

namespace ProyecThor::UI {

// Mismo esquema que OClockTransmitMode: reutilizarlo tal cual evitaria
// duplicacion, pero lo declaro aparte para no acoplar OClock <-> QuickNotes.
enum class QuickNoteTransmitMode {
    Off,
    MainOnly,
    LANOnly,
    Both
};

class QuickNotes {
public:
    QuickNotes();
    ~QuickNotes();

    std::string GetName() const;
    void Render();

    // Fuerza el guardado a disco del texto actual, saltando el debounce del
    // autoguardado -- llamado por UIManager al cerrar la ventana flotante
    // (X o Shift+Z) para que el cierre nunca pierda las ultimas pulsaciones.
    void PersistNow();

private:
    void PushToCore();
    void ClearFromCore();
    void SyncTransmission();      // <-- nuevo: reemplaza el push directo
    void RenderTransmitCards();   // <-- nuevo: UI tipo tarjetas (igual a OClock)
    void RenderStyleSelector();   // <-- nuevo
    void LoadPersisted();         // carga m_TextBuffer desde Settings::general.quickNotesText

    std::array<char, 4096> m_TextBuffer{};
    bool m_IsLive = false;

    // Autoguardado con debounce (ver Render()/PersistNow()) -- evita escribir
    // a disco en cada tecla mientras se tipea rapido.
    double m_LastPersistTime = 0.0;

    // ── Transmision ───────────────────────────────────────────────────
    QuickNoteTransmitMode m_TransmitMode     = QuickNoteTransmitMode::Off;
    QuickNoteTransmitMode m_PrevTransmitMode = QuickNoteTransmitMode::Off;

    // ── Estilo predeterminado (igual criterio que OClock) ──────────────
    std::string m_StyleName; // vacio = usar el estilo activo actual
};

} // namespace ProyecThor::UI
