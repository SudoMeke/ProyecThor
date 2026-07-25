#pragma once
#include <string>
#include <vector>
#include <chrono>

namespace ProyecThor::Core {

// ─────────────────────────────────────────────────────────────────────────────
//  Macro — secuencia de cues con tiempo ("comandos macro", no un compositor
//  de video con propiedades interpoladas): a cada cue le corresponde un
//  instante (segundos desde el inicio del macro) y una accion que reutiliza
//  la MISMA funcionalidad que ya existe en el resto de la app (cambiar
//  fondo, poner/quitar overlay, mostrar/quitar texto, cambiar el estilo del
//  reloj). Las transiciones "bonitas" no son un sistema nuevo: al llamar a
//  las mismas funciones que ya usa el resto de la UI (SetBackgroundMedia,
//  SetLayer2_Text...) se dispara el mismo TransitionPanel/crossfade de
//  fondo que ya existe.
//
//  Reproduccion en dos modos (ver MacroPlayer):
//   - Automatico: un playhead avanza con el reloj y dispara cada cue cuando
//     le toca segun timeSeconds (como un video).
//   - Manual: el operador avanza/retrocede cue por cue con Next()/
//     Previous(), como pasar diapositivas — timeSeconds solo ordena.
// ─────────────────────────────────────────────────────────────────────────────
enum class MacroCueType {
    ChangeBackground,  // param = ruta a imagen/video (Fondos u Overlays)
    SetOverlay,        // param = ruta a imagen/video de overlay (compuesto sobre el fondo)
    ClearOverlay,      // sin param: quita el overlay activo
    ShowText,          // param = texto a proyectar (Layer2)
    ClearText,         // sin param: limpia el texto proyectado
    ChangeClockStyle,  // param = nombre de un estilo guardado, para el widget de Reloj
};

const char* MacroCueTypeLabel(MacroCueType t);

struct MacroCue {
    float        timeSeconds  = 0.0f;
    MacroCueType type         = MacroCueType::ChangeBackground;
    std::string  param;                // ruta o texto, segun el tipo
    bool         paramIsVideo = false; // solo aplica a ChangeBackground

    // Solo ShowText: si es true, al aplicar la cue tambien se pone un fondo
    // solido negro (PresentationCore::SetLayer0_Color) para que el texto
    // quede solo en pantalla en vez de compuesto sobre lo que hubiera de
    // fondo antes.
    bool         textStandalone = false;

    // Nombre de un UI::TransitionType (ver TransitionPanel.h), vacio = usar
    // la transicion global actual sin tocarla. Guardado como string (no el
    // enum de frontend/panels) para que backend/core no dependa de
    // frontend/panels — el mapeo nombre<->enum vive en TransitionPanel.cpp.
    std::string  transitionName;
    float        transitionDuration = -1.0f; // -1 = usar la duracion actual
};

struct Macro {
    std::string           name;
    std::vector<MacroCue> cues; // ver SortCues: se mantiene ordenada por timeSeconds
};

void SortCues(Macro& m);

// ── Persistencia: un archivo JSON por macro ─────────────────────────────────
std::vector<std::string> ListMacroNames();
bool                     SaveMacro(const Macro& m);
bool                     LoadMacro(const std::string& name, Macro& out);
bool                     DeleteMacroFile(const std::string& name);
bool                     RenameMacroFile(const std::string& oldName, const std::string& newName);

// ─────────────────────────────────────────────────────────────────────────────
//  MacroPlayer — vive dentro de PresentationCore (ver PlayMacro/StopMacro/
//  NextMacroCue/PrevMacroCue/UpdateMacroPlayer alli) para que tanto el
//  editor (LayersOverlayTab) como el transporte "Control Overlays"
//  (ViewPanel) puedan controlar el mismo macro en reproduccion.
// ─────────────────────────────────────────────────────────────────────────────
class MacroPlayer {
public:
    void Play(const Macro& m, bool autoAdvance);
    void Stop();
    void Update(); // llamar una vez por frame; en modo manual no hace nada

    void Next();     // manual: dispara la cue siguiente (si hay)
    void Previous();  // manual: re-dispara la cue anterior (si hay)
    void GoToCue(int index); // salta directo a una cue (ej. recall de un Pad), sin pasar por las intermedias

    bool  IsPlaying() const     { return m_Playing; }
    bool  IsAutoAdvance() const { return m_AutoAdvance; }
    void  SetAutoAdvance(bool v);

    int          GetCurrentCueIndex() const { return m_CurrentIndex; } // -1 = ninguna disparada aun
    int          GetCueCount() const        { return (int)m_Macro.cues.size(); }
    float        GetElapsed() const;
    const Macro& GetMacro() const           { return m_Macro; }

private:
    Macro  m_Macro;
    bool   m_Playing      = false;
    bool   m_AutoAdvance  = true;
    int    m_CurrentIndex = -1; // ultima cue aplicada (indice en m_Macro.cues)
    std::chrono::steady_clock::time_point m_StartTime;

    void ApplyCue(int index);
};

} // namespace ProyecThor::Core
