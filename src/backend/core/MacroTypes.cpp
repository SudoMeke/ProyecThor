#include "MacroTypes.h"
#include "PresentationCore.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <cstdlib>
#include <pwd.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace ProyecThor::Core {

// ─────────────────────────────────────────────────────────────────────────────
//  Rutas — mismo patron que el resto de las carpetas de datos de la app.
// ─────────────────────────────────────────────────────────────────────────────
static fs::path GetAppDataDir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH] = {};
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf);
    fs::path dir = fs::path(buf) / "ProyecThor";
#else
    fs::path base;
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg) {
        base = xdg;
    } else if (const char* home = std::getenv("HOME"); home && *home) {
        base = fs::path(home) / ".local" / "share";
    } else if (struct passwd* pw = getpwuid(getuid())) {
        base = fs::path(pw->pw_dir) / ".local" / "share";
    } else {
        base = fs::current_path();
    }
    fs::path dir = base / "ProyecThor";
#endif
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}

static fs::path MacrosDir() {
    fs::path dir = GetAppDataDir() / "assets" / "macros";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}

static fs::path MacroFilePath(const std::string& name) {
    return MacrosDir() / (name + ".macro.json");
}

// ─────────────────────────────────────────────────────────────────────────────
//  MacroCueType <-> string (persistencia)
// ─────────────────────────────────────────────────────────────────────────────
static const char* CueTypeKey(MacroCueType t) {
    switch (t) {
        case MacroCueType::ChangeBackground: return "background";
        case MacroCueType::SetOverlay:       return "overlay";
        case MacroCueType::ClearOverlay:     return "clearOverlay";
        case MacroCueType::ShowText:         return "showText";
        case MacroCueType::ClearText:        return "clearText";
        case MacroCueType::ChangeClockStyle: return "clockStyle";
    }
    return "showText";
}

static MacroCueType CueTypeFromKey(const std::string& k) {
    if (k == "background")    return MacroCueType::ChangeBackground;
    if (k == "overlay")       return MacroCueType::SetOverlay;
    if (k == "clearOverlay")  return MacroCueType::ClearOverlay;
    if (k == "showText")      return MacroCueType::ShowText;
    if (k == "clearText")     return MacroCueType::ClearText;
    if (k == "clockStyle")    return MacroCueType::ChangeClockStyle;
    return MacroCueType::ShowText;
}

const char* MacroCueTypeLabel(MacroCueType t) {
    switch (t) {
        case MacroCueType::ChangeBackground: return "Cambiar fondo";
        case MacroCueType::SetOverlay:       return "Poner overlay";
        case MacroCueType::ClearOverlay:     return "Quitar overlay";
        case MacroCueType::ShowText:         return "Mostrar texto";
        case MacroCueType::ClearText:        return "Limpiar texto";
        case MacroCueType::ChangeClockStyle: return "Cambiar estilo del reloj";
    }
    return "?";
}

void SortCues(Macro& m) {
    std::sort(m.cues.begin(), m.cues.end(),
        [](const MacroCue& a, const MacroCue& b) { return a.timeSeconds < b.timeSeconds; });
}

// ─────────────────────────────────────────────────────────────────────────────
//  Persistencia
// ─────────────────────────────────────────────────────────────────────────────
std::vector<std::string> ListMacroNames() {
    std::vector<std::string> names;
    try {
        for (const auto& e : fs::directory_iterator(MacrosDir())) {
            if (!e.is_regular_file()) continue;
            std::string fn = e.path().filename().string();
            const std::string suffix = ".macro.json";
            if (fn.size() > suffix.size() &&
                fn.compare(fn.size() - suffix.size(), suffix.size(), suffix) == 0) {
                names.push_back(fn.substr(0, fn.size() - suffix.size()));
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "[MacroTypes] " << ex.what() << "\n";
    }
    std::sort(names.begin(), names.end());
    return names;
}

bool SaveMacro(const Macro& m) {
    if (m.name.empty()) return false;

    json j;
    j["name"] = m.name;
    j["cues"] = json::array();
    for (const auto& c : m.cues) {
        json jc;
        jc["time"]               = c.timeSeconds;
        jc["type"]               = CueTypeKey(c.type);
        jc["param"]              = c.param;
        jc["isVideo"]            = c.paramIsVideo;
        jc["textStandalone"]     = c.textStandalone;
        jc["transitionName"]     = c.transitionName;
        jc["transitionDuration"] = c.transitionDuration;
        j["cues"].push_back(jc);
    }

    std::ofstream f(MacroFilePath(m.name));
    if (!f.is_open()) return false;
    f << j.dump(2);
    return true;
}

bool LoadMacro(const std::string& name, Macro& out) {
    std::ifstream f(MacroFilePath(name));
    if (!f.is_open()) return false;

    try {
        json j;
        f >> j;
        out.name = j.value("name", name);
        out.cues.clear();
        for (const auto& jc : j.value("cues", json::array())) {
            MacroCue c;
            c.timeSeconds        = jc.value("time", 0.0f);
            c.type               = CueTypeFromKey(jc.value("type", "showText"));
            c.param              = jc.value("param", "");
            c.paramIsVideo       = jc.value("isVideo", false);
            c.textStandalone     = jc.value("textStandalone", false);
            c.transitionName     = jc.value("transitionName", "");
            c.transitionDuration = jc.value("transitionDuration", -1.0f);
            out.cues.push_back(std::move(c));
        }
    } catch (const std::exception& ex) {
        std::cerr << "[MacroTypes] Error leyendo " << name << ": " << ex.what() << "\n";
        return false;
    }

    SortCues(out);
    return true;
}

bool DeleteMacroFile(const std::string& name) {
    std::error_code ec;
    return fs::remove(MacroFilePath(name), ec);
}

bool RenameMacroFile(const std::string& oldName, const std::string& newName) {
    if (newName.empty() || oldName == newName) return false;
    std::error_code ec;
    fs::rename(MacroFilePath(oldName), MacroFilePath(newName), ec);
    return !ec;
}

// ─────────────────────────────────────────────────────────────────────────────
//  MacroPlayer
// ─────────────────────────────────────────────────────────────────────────────
void MacroPlayer::Play(const Macro& m, bool autoAdvance) {
    m_Macro       = m;
    SortCues(m_Macro);
    m_AutoAdvance = autoAdvance;
    m_CurrentIndex = -1;
    m_Playing     = !m_Macro.cues.empty();
    m_StartTime   = std::chrono::steady_clock::now();

    // Modo manual: la primera cue se aplica de una, como la primera
    // diapositiva de una presentacion (no hace falta apretar "Siguiente"
    // para ver algo en pantalla).
    if (m_Playing && !m_AutoAdvance)
        ApplyCue(0);
}

void MacroPlayer::Stop() {
    m_Playing      = false;
    m_CurrentIndex = -1;
}

void MacroPlayer::SetAutoAdvance(bool v) {
    m_AutoAdvance = v;
    if (v) {
        // Al pasar a automatico, el playhead retoma desde "ahora" en vez de
        // recontar todas las cues ya pasadas manualmente.
        m_StartTime = std::chrono::steady_clock::now();
    }
}

float MacroPlayer::GetElapsed() const {
    if (!m_Playing) return 0.0f;
    return std::chrono::duration<float>(std::chrono::steady_clock::now() - m_StartTime).count();
}

void MacroPlayer::Update() {
    if (!m_Playing || !m_AutoAdvance) return;

    float elapsed = GetElapsed();
    size_t next = static_cast<size_t>(m_CurrentIndex + 1);
    while (next < m_Macro.cues.size() && m_Macro.cues[next].timeSeconds <= elapsed) {
        ApplyCue((int)next);
        next = static_cast<size_t>(m_CurrentIndex + 1);
    }

    // FIX: antes, apenas se disparaba la ULTIMA cue, esto ponia
    // m_Playing=false — lo que en ViewPanel::RenderControlOverlays hacia
    // que el transporte entero (Anterior/Siguiente/Auto/Manual/Detener)
    // desapareciera solo y volviera a la pantalla de "elegi un macro",
    // justo al llegar (o avanzar manualmente hasta) el ultimo keyframe. El
    // while de arriba ya deja de avanzar por si solo al llegar al final
    // (no hay mas cues que cumplan la condicion) — no hace falta ademas
    // marcar el macro como "detenido". Se queda en la ultima cue, visible,
    // hasta que el operador aprieta Detener a proposito.
}

void MacroPlayer::Next() {
    if (m_Macro.cues.empty()) return;
    int next = m_CurrentIndex + 1;
    if (next >= (int)m_Macro.cues.size()) return;
    m_Playing = true;
    ApplyCue(next);
}

void MacroPlayer::Previous() {
    if (m_Macro.cues.empty()) return;
    int prev = m_CurrentIndex - 1;
    if (prev < 0) return;
    m_Playing = true;
    ApplyCue(prev);
}

void MacroPlayer::ApplyCue(int index) {
    if (index < 0 || index >= (int)m_Macro.cues.size()) return;
    m_CurrentIndex = index;

    const MacroCue& cue = m_Macro.cues[index];
    auto& core = PresentationCore::Get();

    // Transicion por cue: se deja pendiente para que la consuma quien
    // dispare la transicion real (TransitionPanel::Trigger, en frontend/) —
    // backend/core no conoce UI::TransitionType, solo pasa el nombre.
    if (!cue.transitionName.empty())
        core.SetPendingTransitionOverride(cue.transitionName, cue.transitionDuration);

    switch (cue.type) {
        case MacroCueType::ChangeBackground:
            // allowAudio=false: un macro anima fondos/overlays, no es el
            // flujo de "enviar al monitor" (los fondos nunca deben sonar).
            core.SetBackgroundMedia(cue.param, cue.paramIsVideo, /*allowAudio=*/false);
            break;
        case MacroCueType::SetOverlay:
            core.SetOverlayMedia(cue.param);
            break;
        case MacroCueType::ClearOverlay:
            core.StopOverlayMedia();
            break;
        case MacroCueType::ShowText:
            core.SetLayer2_Text(cue.param);
            if (cue.textStandalone)
                core.SetLayer0_Color(0.0f, 0.0f, 0.0f);
            break;
        case MacroCueType::ClearText:
            core.ClearLayer2();
            break;
        case MacroCueType::ChangeClockStyle:
            core.SetClockStyleCue(cue.param);
            break;
    }
}

} // namespace ProyecThor::Core
