// OClock.cpp
//
// El modo de transmisión "Solo LAN" usa dos métodos nuevos de
// PresentationCore (implementados en PresentationCore.h/.cpp):
//
//   core.SetLiveQuickNoteLAN(const std::string& text);
//   core.ClearQuickNoteLAN();
//
// Estos escriben en PresentationState::lanQuickNoteText, un campo aparte de
// currentText que el SnapshotProvider de ToggleNetworkStream prioriza SOLO
// para el JSON que reciben los clientes de red — nunca se dibuja en la
// pantalla principal/proyector, que sigue leyendo currentText/showText como
// siempre.
// ─────────────────────────────────────────────────────────────────────────

#include "OClock.h"
#include "GlassRenderer.h"
#include "DesignSystem.h"
#include "backend/core/PresentationCore.h"
#include "frontend/ui/UIStrings.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>

namespace ProyecThor::UI {

static ImU32 ColU32(float r, float g, float b, float a = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
}
static ImU32 ColA(ImU32 col, int a) {
    return (col & 0x00FFFFFFu) | (static_cast<ImU32>(std::clamp(a, 0, 255)) << 24);
}
// DS:: expone colores como ImU32; ImGui::TextColored/PushStyleColor piden ImVec4.
static ImVec4 ToVec4(ImU32 col) {
    return ImGui::ColorConvertU32ToFloat4(col);
}

// Sombra suave reutilizando el mismo patrón visual que el resto de paneles.
static void DrawSoftShadow(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float rounding) {
    for (float i = 1.0f; i <= 5.0f; i += 1.0f) {
        int alpha = static_cast<int>(34.0f - (i * 5.0f));
        dl->AddRectFilled(
            ImVec2(p0.x - i, p0.y - i + 3.0f),
            ImVec2(p1.x + i, p1.y + i + 3.0f),
            IM_COL32(0, 0, 0, std::max(0, alpha)), rounding + i);
    }
}

static constexpr int kPresetMinutes[] = { 5, 10, 15, 20, 30, 45 };

// ── Ciclo de vida / lógica de tiempo ────────────────────────────────────────

OClock::OClock() : m_ElapsedTime(std::chrono::seconds(0)) {}

void OClock::Start(int minutes, int seconds) {
    if (m_PausedElapsed.count() <= 0.0) {
        m_TargetTime = std::chrono::minutes(minutes) + std::chrono::seconds(seconds);
    }
    auto now    = std::chrono::steady_clock::now();
    m_StartTime = now - std::chrono::duration_cast<std::chrono::steady_clock::duration>(m_PausedElapsed);
    m_IsRunning = true;
}

void OClock::Stop() {
    m_PausedElapsed = m_ElapsedTime;
    m_IsRunning      = false;
}

void OClock::Reset() {
    m_IsRunning     = false;
    m_IsOvertime    = false;
    m_ElapsedTime   = std::chrono::seconds(0);
    m_PausedElapsed = std::chrono::seconds(0);
}

void OClock::ApplyPreset(int minutes) {
    m_InputMin = minutes;
    m_InputSec = 0;
}

std::string OClock::GetFormattedTime() const {
    int totalSecs = std::max<int>(
        0, (int)std::chrono::duration_cast<std::chrono::seconds>(m_ElapsedTime).count());
    int mins = totalSecs / 60;
    int secs = totalSecs % 60;
    char buffer[20];
    if (m_ShowSignPrefix && m_IsOvertime)
        snprintf(buffer, sizeof(buffer), "+%02d:%02d", mins, secs);
    else
        snprintf(buffer, sizeof(buffer), "%02d:%02d", mins, secs);
    return std::string(buffer);
}

float OClock::GetProgressRatio() const {
    double targetSecs = static_cast<double>(m_TargetTime.count());
    if (targetSecs <= 0.0) return 0.0f;
    double elapsedSecs = m_ElapsedTime.count();
    return static_cast<float>(std::clamp(elapsedSecs / targetSecs, 0.0, 1.0));
}

void OClock::SyncTransmission(const std::string& timeStr) {
    auto& core = Core::PresentationCore::Get();

    bool wasMain = (m_PrevTransmitMode == OClockTransmitMode::MainOnly || m_PrevTransmitMode == OClockTransmitMode::Both);
    bool wasLAN  = (m_PrevTransmitMode == OClockTransmitMode::LANOnly  || m_PrevTransmitMode == OClockTransmitMode::Both);
    bool isMain  = (m_TransmitMode     == OClockTransmitMode::MainOnly || m_TransmitMode     == OClockTransmitMode::Both);
    bool isLAN   = (m_TransmitMode     == OClockTransmitMode::LANOnly  || m_TransmitMode     == OClockTransmitMode::Both);

    if (isMain) {
        core.SetLiveQuickNote(timeStr);
    } else if (wasMain) {
        core.ClearQuickNote();
    }

    if (isLAN) {
        core.SetLiveQuickNoteLAN(timeStr); // ver NOTA arriba
    } else if (wasLAN) {
        core.ClearQuickNoteLAN();          // ver NOTA arriba
    }

    m_PrevTransmitMode = m_TransmitMode;
}

// ── Render ───────────────────────────────────────────────────────────────

void OClock::Render(GlassRenderer& glass) {
    const auto& str  = ProyecThor::UI::GetUIStrings();
    auto&       core = Core::PresentationCore::Get();

    // ── Actualizar tiempo (cuenta ASCENDENTE) ───────────────────────────────
    if (m_IsRunning) {
        auto now      = std::chrono::steady_clock::now();
        m_ElapsedTime = now - m_StartTime;
        m_IsOvertime  = m_ElapsedTime >= m_TargetTime;
    }

    std::string timeStr = GetFormattedTime();
    float       t       = static_cast<float>(ImGui::GetTime());

    // ── Ventana de vidrio ────────────────────────────────────────────────
    if (!DS::BeginGlassPanel(str.oclockTitle, glass)) {
        DS::EndGlassPanel();
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.157f, 0.784f, 0.847f, 1.0f)); // acento cian
    ImGui::TextUnformatted(str.oclockTitle);
    ImGui::PopStyleColor();
    DS::GlassSeparator();

    float w = ImGui::GetContentRegionAvail().x;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // ── Display grande del tiempo ──────────────────────────────────────────
    {
        ImVec2 dispPos = ImGui::GetCursorScreenPos();
        float  dispH   = 92.0f;
        ImVec2 dispEnd = ImVec2(dispPos.x + w, dispPos.y + dispH);

        DrawSoftShadow(dl, dispPos, dispEnd, DS::RadiusLarge);

        ImU32 bgCol, borderCol, textCol;
        if (m_IsOvertime) {
            float pulse = 0.55f + 0.35f * std::sin(t * 3.0f);
            bgCol     = ColU32(0.22f, 0.05f, 0.06f);
            borderCol = ColA(DS::DangerColor, static_cast<int>(90 + 90 * pulse));
            textCol   = DS::DangerColor;
        } else if (m_IsRunning) {
            bgCol     = ColU32(0.04f, 0.16f, 0.17f);
            borderCol = ColA(DS::AccentColor, 130);
            textCol   = DS::AccentLight;
        } else {
            bgCol     = ColU32(0.08f, 0.09f, 0.12f);
            borderCol = ColA(DS::TextHint, 150);
            textCol   = DS::TextSecondary;
        }

        dl->AddRectFilled(dispPos, dispEnd, bgCol, DS::RadiusLarge);
        dl->AddRect(dispPos, dispEnd, borderCol, DS::RadiusLarge, 0, 1.5f);

        bool bigFont = (ImGui::GetIO().Fonts->Fonts.Size > 1);
        if (bigFont) ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);

        ImVec2 textSz  = ImGui::CalcTextSize(timeStr.c_str());
        ImVec2 textPos = ImVec2(
            dispPos.x + (w - textSz.x) * 0.5f,
            dispPos.y + (dispH - textSz.y) * 0.5f - (m_ShowProgressBar ? 6.0f : 0.0f));

        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), textPos, textCol, timeStr.c_str());

        if (bigFont) ImGui::PopFont();

        // ── Barra de progreso (objetivo) ────────────────────────────────
        if (m_ShowProgressBar) {
            float barH   = 6.0f;
            float barPad = 16.0f;
            ImVec2 bMin(dispPos.x + barPad, dispEnd.y - barH - 10.0f);
            ImVec2 bMax(dispEnd.x - barPad, bMin.y + barH);

            dl->AddRectFilled(bMin, bMax, ColA(DS::TextHint, 90), barH * 0.5f);

            if (m_IsOvertime) {
                dl->AddRectFilled(bMin, bMax, ColA(DS::DangerColor, 220), barH * 0.5f);
            } else {
                float ratio = GetProgressRatio();
                float fillX = bMin.x + (bMax.x - bMin.x) * ratio;
                if (fillX > bMin.x)
                    dl->AddRectFilled(bMin, ImVec2(fillX, bMax.y), ColA(DS::AccentColor, 230), barH * 0.5f);
            }
        }

        ImGui::Dummy(ImVec2(w, dispH));

        // Objetivo + estado, debajo del display
        char targetBuf[32];
        int  tgtSecs = static_cast<int>(m_TargetTime.count());
        snprintf(targetBuf, sizeof(targetBuf), "Objetivo: %02d:%02d", tgtSecs / 60, tgtSecs % 60);
        ImGui::TextColored(ToVec4(DS::TextSecondary), "%s", targetBuf);

        if (m_IsOvertime) {
            ImGui::SameLine();
            ImGui::TextColored(ToVec4(DS::DangerColor), "  •  Tiempo excedido");
        }
    }

    ImGui::Spacing();
    DS::GlassSeparator();

    // ── Configuración ────────────────────────────────────────────────────
    DS::GlassSectionHeader("CONFIGURACIÓN");

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.05f, 0.09f, 0.13f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.07f, 0.12f, 0.17f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);

    float halfW = (w - 8.0f) * 0.5f;

    // Minutos: label arriba, campo abajo (columna) — así el label nunca
    // compite por ancho con el stepper + los dígitos y no se corta.
    ImGui::BeginGroup();
    ImGui::TextColored(ToVec4(DS::TextHint), "%s", str.minutes);
    ImGui::SetNextItemWidth(halfW);
    ImGui::InputInt("##oclock_min", &m_InputMin, 0, 0); // sin +/- para ganar ancho
    ImGui::EndGroup();

    ImGui::SameLine(0, 8);

    // Segundos: mismo esquema.
    ImGui::BeginGroup();
    ImGui::TextColored(ToVec4(DS::TextHint), "%s", str.seconds);
    ImGui::SetNextItemWidth(halfW);
    ImGui::InputInt("##oclock_sec", &m_InputSec, 0, 0);
    ImGui::EndGroup();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    if (m_InputMin < 0)  m_InputMin = 0;
    if (m_InputSec < 0)  m_InputSec = 0;
    if (m_InputSec > 59) m_InputSec = 59;

    // ── Presets rápidos ─────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::TextColored(ToVec4(DS::TextHint), "Presets rápidos");
    ImGui::Spacing();

    int presetCount = static_cast<int>(sizeof(kPresetMinutes) / sizeof(kPresetMinutes[0]));
    float presetGap = 6.0f;
    float presetW   = (w - presetGap * (presetCount - 1)) / presetCount;

    for (int i = 0; i < presetCount; ++i) {
        bool active = (m_InputMin == kPresetMinutes[i] && m_InputSec == 0);
        char label[8];
        snprintf(label, sizeof(label), "%d'", kPresetMinutes[i]);
        if (DS::GlassButton(label, ImVec2(presetW, 30.0f), active ? DS::AccentColor : DS::AccentColorDim))
            ApplyPreset(kPresetMinutes[i]);
        if (i != presetCount - 1) ImGui::SameLine(0, presetGap);
    }

    ImGui::Spacing();

    // ── Opciones de visualización ───────────────────────────────────────
    ImGui::Checkbox("Barra de progreso", &m_ShowProgressBar);
    ImGui::SameLine(0, 16);
    ImGui::Checkbox("Prefijo \"+\" en overtime", &m_ShowSignPrefix);

    ImGui::Spacing();

    // ── Botones de control ─────────────────────────────────────────────────
    float btnW = (w - 8.0f) * 0.5f;
    float btnH = 40.0f;

    if (!m_IsRunning) {
        const char* startLabel = (m_PausedElapsed.count() > 0.0) ? "Reanudar" : str.start;
        if (DS::GlassButton(startLabel, ImVec2(btnW, btnH), DS::SuccessColor))
            Start(m_InputMin, m_InputSec);
    } else {
        if (DS::GlassButton(str.pause, ImVec2(btnW, btnH), ColU32(0.85f, 0.6f, 0.1f)))
            Stop();
    }

    ImGui::SameLine(0, 8);
    if (DS::GlassButton(str.reset, ImVec2(btnW, btnH), DS::DangerColor))
        Reset();

    ImGui::Spacing();
    DS::GlassSeparator();

    // ── Transmisión ──────────────────────────────────────────────────────
    DS::GlassSectionHeader("TRANSMITIR");

    bool netAvailable = core.IsStreamingNet();

    struct ModeOpt { const char* label; const char* sub; OClockTransmitMode mode; bool needsNet; };
    ModeOpt opts[4] = {
        { "Apagado",   "No se transmite",              OClockTransmitMode::Off,      false },
        { "Pantalla",  "Solo proyector principal",      OClockTransmitMode::MainOnly, false },
        { "Solo LAN",  "Solo dispositivos en red",      OClockTransmitMode::LANOnly,  true  },
        { "Ambos",     "Pantalla + red",                OClockTransmitMode::Both,     true  },
    };

    float cardGap = 8.0f;
    float cardW   = (w - cardGap * 3.0f) / 4.0f;
    float cardH   = 62.0f;

    ImVec2 rowStart = ImGui::GetCursorScreenPos();

    for (int i = 0; i < 4; ++i) {
        auto& opt = opts[i];
        bool  disabled = opt.needsNet && !netAvailable;
        bool  active   = (m_TransmitMode == opt.mode) && !disabled;

        ImVec2 p0 = ImVec2(rowStart.x + i * (cardW + cardGap), rowStart.y);
        ImVec2 p1 = ImVec2(p0.x + cardW, p0.y + cardH);

        ImU32 bg  = active ? ColA(DS::AccentColor, 45) : ImU32(IM_COL32(255, 255, 255, 10));
        ImU32 bdr = active ? ColA(DS::AccentColor, 200) : ColA(DS::TextHint, disabled ? 60 : 120);

        dl->AddRectFilled(p0, p1, bg, DS::RadiusMedium);
        dl->AddRect(p0, p1, bdr, DS::RadiusMedium, 0, active ? 1.5f : 1.0f);

        ImGui::SetCursorScreenPos(p0);
        ImGui::PushID(i);
        ImGui::BeginDisabled(disabled);
        bool clicked = ImGui::InvisibleButton("##mode", ImVec2(cardW, cardH));
        ImGui::EndDisabled();
        ImGui::PopID();

        if (clicked && !disabled) m_TransmitMode = opt.mode;

        ImU32 labelCol = disabled ? ColA(DS::TextHint, 130) : (active ? DS::AccentLight : DS::TextSecondary);
        ImVec2 labelSz = ImGui::CalcTextSize(opt.label);
        dl->AddText(ImVec2(p0.x + (cardW - labelSz.x) * 0.5f, p0.y + 12.0f), labelCol, opt.label);

        ImU32 subCol = disabled ? ColA(DS::TextHint, 100) : ColA(DS::TextHint, 220);
        float subScale = 0.82f;
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * subScale,
                    ImVec2(p0.x + 6.0f, p0.y + 34.0f), subCol, opt.sub, nullptr, cardW - 12.0f);
    }

    ImGui::SetCursorScreenPos(ImVec2(rowStart.x, rowStart.y + cardH));
    ImGui::Dummy(ImVec2(w, cardH));

    if (!netAvailable) {
        ImGui::Spacing();
        ImGui::TextColored(ToVec4(DS::TextHint), "Inicia el servidor en el panel de Transmisión para habilitar \"Solo LAN\" / \"Ambos\".");
        // Si el servidor de red se apaga estando en un modo que lo requiere, caemos a MainOnly/Off automáticamente.
        if (m_TransmitMode == OClockTransmitMode::LANOnly) m_TransmitMode = OClockTransmitMode::Off;
        if (m_TransmitMode == OClockTransmitMode::Both)    m_TransmitMode = OClockTransmitMode::MainOnly;
    }

    if (m_TransmitMode == OClockTransmitMode::LANOnly || m_TransmitMode == OClockTransmitMode::Both) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::DangerColor));
        ImGui::TextUnformatted("●");
        ImGui::SameLine(0, 4);
        ImGui::TextUnformatted(m_TransmitMode == OClockTransmitMode::LANOnly
            ? "Transmitiendo solo a la red local"
            : "Transmitiendo a pantalla + red local");
        ImGui::PopStyleColor();
    } else if (m_TransmitMode == OClockTransmitMode::MainOnly) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::DangerColor));
        ImGui::TextUnformatted("●");
        ImGui::SameLine(0, 4);
        ImGui::TextUnformatted(str.liveIndicator);
        ImGui::PopStyleColor();
    }

    SyncTransmission(timeStr);

    DS::EndGlassPanel();
}

} // namespace ProyecThor::UI