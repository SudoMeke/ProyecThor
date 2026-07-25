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

private:
    void PushToCore();
    void ClearFromCore();
    void SyncTransmission();      // <-- nuevo: reemplaza el push directo
    void RenderTransmitCards();   // <-- nuevo: UI tipo tarjetas (igual a OClock)
    void RenderStyleSelector();   // <-- nuevo

    std::array<char, 4096> m_TextBuffer{};
    bool m_IsLive = false;

    // ── Transmision ───────────────────────────────────────────────────
    QuickNoteTransmitMode m_TransmitMode     = QuickNoteTransmitMode::Off;
    QuickNoteTransmitMode m_PrevTransmitMode = QuickNoteTransmitMode::Off;

    // ── Estilo predeterminado (igual criterio que OClock) ──────────────
    std::string m_StyleName; // vacio = usar el estilo activo actual
};

} // namespace ProyecThor::UI