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
    int targetSecs  = static_cast<int>(m_TargetTime.count());
    int elapsedSecs = std::max<int>(
        0, (int)std::chrono::duration_cast<std::chrono::seconds>(m_ElapsedTime).count());

    // El "cruce a final" (m_IsOvertime) es siempre elapsed >= target, sin
    // importar el sentido. Lo que cambia es que numero se muestra:
    //  - CountUp:   se muestra el elapsed tal cual (sigue subiendo en overtime).
    //  - CountDown: se muestra target-elapsed mientras sea >= 0; una vez
    //               cruzado el 0, se muestra el excedente con signo "-".
    int displaySecs;
    if (m_Direction == OClockDirection::CountDown) {
        int remaining = targetSecs - elapsedSecs;
        displaySecs = (remaining >= 0) ? remaining : -remaining;
    } else {
        displaySecs = elapsedSecs;
    }

    int mins = displaySecs / 60;
    int secs = displaySecs % 60;
    char buffer[20];

    if (m_ShowSignPrefix && m_IsOvertime) {
        char sign = (m_Direction == OClockDirection::CountDown) ? '-' : '+';
        snprintf(buffer, sizeof(buffer), "%c%02d:%02d", sign, mins, secs);
    } else {
        snprintf(buffer, sizeof(buffer), "%02d:%02d", mins, secs);
    }
    return std::string(buffer);
}

float OClock::GetProgressRatio() const {
    double targetSecs = static_cast<double>(m_TargetTime.count());
    if (targetSecs <= 0.0) return 0.0f;
    double elapsedSecs = m_ElapsedTime.count();
    return static_cast<float>(std::clamp(elapsedSecs / targetSecs, 0.0, 1.0));
}

// ── Título / mensaje ─────────────────────────────────────────────────────

std::string OClock::GetCurrentTitle() const {
    if (m_TitleIndex < 0 || m_TitleIndex >= (int)m_Titles.size()) return "";
    return m_Titles[m_TitleIndex];
}

void OClock::AdvanceTitle() {
    if (m_Titles.empty()) return;
    m_TitleIndex = (m_TitleIndex + 1) % (int)m_Titles.size();
}

void OClock::SyncTransmission(const std::string& timeStr) {
    auto& core = Core::PresentationCore::Get();

    bool wasMain = (m_PrevTransmitMode == OClockTransmitMode::MainOnly || m_PrevTransmitMode == OClockTransmitMode::Both);
    bool wasLAN  = (m_PrevTransmitMode == OClockTransmitMode::LANOnly  || m_PrevTransmitMode == OClockTransmitMode::Both);
    bool isMain  = (m_TransmitMode     == OClockTransmitMode::MainOnly || m_TransmitMode     == OClockTransmitMode::Both);
    bool isLAN   = (m_TransmitMode     == OClockTransmitMode::LANOnly  || m_TransmitMode     == OClockTransmitMode::Both);

    bool transmitting = isMain || isLAN;

    // Estilo: si el usuario definio un "estilo final" explicito, se usa
    // completo (color/tamano/alineacion propios) al llegar al final. Si no
    // definio uno, se mantiene el comportamiento clasico: estilo normal +
    // color de peligro forzado via colorOverride.
    bool usingFinalStyle = m_IsOvertime && !m_FinalStyleName.empty();

    if (transmitting) {
        const std::string& styleToApply = usingFinalStyle ? m_FinalStyleName : m_StyleName;
        if (!styleToApply.empty())
            core.ApplyStyleByName(styleToApply);
    }

    ImVec4 dangerV4 = ImGui::ColorConvertU32ToFloat4(DS::DangerColor);
    float  dangerRGBA[4] = { dangerV4.x, dangerV4.y, dangerV4.z, dangerV4.w };
    const float* colorOverride = (m_IsOvertime && !usingFinalStyle) ? dangerRGBA : nullptr;

    // Título activo + tiempo, combinados en un solo bloque de texto.
    std::string title    = GetCurrentTitle();
    std::string fullText = title.empty() ? timeStr : (title + "\n" + timeStr);

    if (isMain) {
        core.SetLiveQuickNote(fullText, colorOverride);
    } else if (wasMain) {
        core.ClearQuickNote();
    }

    if (isLAN) {
        core.SetLiveQuickNoteLAN(fullText, colorOverride);
    } else if (wasLAN) {
        core.ClearQuickNoteLAN();
    }

    m_PrevTransmitMode = m_TransmitMode;
}

void OClock::RenderStyleSelector() {
    auto& core = Core::PresentationCore::Get();
    std::vector<std::string> styleNames = core.GetSavedStyleNames();

    // Un solo combo reutilizable para "estilo normal" y "estilo final".
    auto renderCombo = [&](const char* label, const char* comboId,
                           std::string& target, const char* emptyHint) {
        ImGui::Spacing();
        ImGui::TextColored(ToVec4(DS::TextHint), "%s", label);

        std::string preview = target.empty() ? "Usar estilo actual" : target;

        ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.05f, 0.09f, 0.13f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.07f, 0.12f, 0.17f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
        ImGui::SetNextItemWidth(-1.0f);

        if (ImGui::BeginCombo(comboId, preview.c_str())) {
            bool noneSelected = target.empty();
            if (ImGui::Selectable("Usar estilo actual", noneSelected))
                target.clear();
            if (noneSelected) ImGui::SetItemDefaultFocus();

            for (const auto& name : styleNames) {
                bool sel = (target == name);
                if (ImGui::Selectable(name.c_str(), sel))
                    target = name;
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);

        if (target.empty())
            ImGui::TextColored(ToVec4(DS::TextHint), "%s", emptyHint);
    };

    renderCombo("Estilo para el público", "##oclockStyle", m_StyleName,
        "Sin estilo fijo: heredara el ultimo estilo activo (Biblia/Cancion).");

    renderCombo("Estilo al llegar al final", "##oclockFinalStyle", m_FinalStyleName,
        "Sin estilo final: se usara el estilo normal + color de peligro (comportamiento clasico).");
}

void OClock::RenderDirectionSelector() {
    ImGui::Spacing();
    ImGui::TextColored(ToVec4(DS::TextHint), "Sentido del conteo");
    ImGui::Spacing();

    float w    = ImGui::GetContentRegionAvail().x;
    float gap  = 8.0f;
    float half = (w - gap) * 0.5f;
    float cardH = 52.0f;

    struct DirOpt { const char* label; const char* sub; OClockDirection dir; };
    DirOpt opts[2] = {
        { "Ascendente",  "Cuenta desde 0 hacia el objetivo",   OClockDirection::CountUp   },
        { "Descendente", "Cuenta regresiva desde el objetivo", OClockDirection::CountDown },
    };

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 rowStart = ImGui::GetCursorScreenPos();

    for (int i = 0; i < 2; ++i) {
        auto& opt   = opts[i];
        bool active = (m_Direction == opt.dir);

        ImVec2 p0 = ImVec2(rowStart.x + i * (half + gap), rowStart.y);
        ImVec2 p1 = ImVec2(p0.x + half, p0.y + cardH);

        ImU32 bg  = active ? ColA(DS::AccentColor, 45) : ImU32(IM_COL32(255, 255, 255, 10));
        ImU32 bdr = active ? ColA(DS::AccentColor, 200) : ColA(DS::TextHint, 120);

        dl->AddRectFilled(p0, p1, bg, DS::RadiusMedium);
        dl->AddRect(p0, p1, bdr, DS::RadiusMedium, 0, active ? 1.5f : 1.0f);

        ImGui::SetCursorScreenPos(p0);
        ImGui::PushID(i);
        bool clicked = ImGui::InvisibleButton("##dir", ImVec2(half, cardH));
        ImGui::PopID();
        if (clicked) m_Direction = opt.dir;

        ImU32 labelCol = active ? DS::AccentLight : DS::TextSecondary;
        dl->AddText(ImVec2(p0.x + 10.0f, p0.y + 8.0f), labelCol, opt.label);

        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.80f,
                    ImVec2(p0.x + 10.0f, p0.y + 28.0f),
                    ColA(DS::TextHint, 210), opt.sub, nullptr, half - 20.0f);
    }

    ImGui::SetCursorScreenPos(ImVec2(rowStart.x, rowStart.y + cardH));
    ImGui::Dummy(ImVec2(w, cardH));
}

void OClock::RenderTitleSection() {
    ImGui::Spacing();
    DS::GlassSectionHeader("TÍTULO / MENSAJE");
    ImGui::TextColored(ToVec4(DS::TextHint),
        "Se muestra arriba del reloj. Usa \"Avanzar\" para ir pasando mensajes.");
    ImGui::Spacing();

    float w = ImGui::GetContentRegionAvail().x;
    float addBtnW = 90.0f;

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.05f, 0.09f, 0.13f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.07f, 0.12f, 0.17f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);

    ImGui::SetNextItemWidth(w - addBtnW - 8.0f);
    ImGui::InputTextWithHint("##oclock_title_input", "Nuevo mensaje...",
        m_TitleInputBuf, sizeof(m_TitleInputBuf));

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    ImGui::SameLine(0, 8);
    if (DS::GlassButton("Agregar", ImVec2(addBtnW, 0.0f), DS::AccentColorDim)) {
        std::string text(m_TitleInputBuf);
        if (!text.empty()) {
            m_Titles.push_back(text);
            if (m_TitleIndex < 0) m_TitleIndex = 0; // el primer mensaje se activa solo
            m_TitleInputBuf[0] = '\0';
        }
    }

    if (!m_Titles.empty()) {
        ImGui::Spacing();
        for (int i = 0; i < (int)m_Titles.size(); ++i) {
            ImGui::PushID(i);
            bool isActive = (i == m_TitleIndex);

            ImGui::PushStyleColor(ImGuiCol_Text,
                isActive ? ToVec4(DS::AccentLight) : ToVec4(DS::TextSecondary));
            if (ImGui::Selectable(m_Titles[i].c_str(), isActive, 0, ImVec2(w - 60.0f, 0.0f)))
                m_TitleIndex = i;
            ImGui::PopStyleColor();

            ImGui::SameLine(w - 50.0f);
            if (DS::GlassButton("X", ImVec2(40.0f, 0.0f), DS::DangerColor)) {
                m_Titles.erase(m_Titles.begin() + i);
                if (m_TitleIndex == i)
                    m_TitleIndex = m_Titles.empty() ? -1 : std::min(i, (int)m_Titles.size() - 1);
                else if (m_TitleIndex > i)
                    m_TitleIndex--;
                ImGui::PopID();
                break; // el vector cambio de tamano: cortamos el loop de este frame
            }
            ImGui::PopID();
        }
    }

    ImGui::Spacing();
    float btnW = (w - 8.0f) * 0.5f;

    ImGui::BeginDisabled(m_Titles.empty());
    if (DS::GlassButton("Avanzar ▶", ImVec2(btnW, 34.0f), DS::AccentColor))
        AdvanceTitle();
    ImGui::SameLine(0, 8);
    if (DS::GlassButton("Quitar título", ImVec2(btnW, 34.0f), DS::AccentColorDim))
        m_TitleIndex = -1;
    ImGui::EndDisabled();
}

// ── Render ───────────────────────────────────────────────────────────────

void OClock::Render(GlassRenderer& glass) {
    const auto& str  = ProyecThor::UI::GetUIStrings();
    auto&       core = Core::PresentationCore::Get();

    // ── Actualizar tiempo ────────────────────────────────────────────────
    // m_ElapsedTime/m_IsOvertime son independientes del sentido de
    // visualizacion: siempre representan "cuanto paso desde Start()" y "si
    // ya cruzamos el objetivo". GetFormattedTime() decide como mostrarlo.
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

    // ── Título / mensaje activo, arriba del display ─────────────────────
    std::string activeTitle = GetCurrentTitle();
    if (!activeTitle.empty()) {
        ImVec2 titleSz = ImGui::CalcTextSize(activeTitle.c_str());
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (w - titleSz.x) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::AccentLight));
        ImGui::TextUnformatted(activeTitle.c_str());
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

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

    ImGui::BeginGroup();
    ImGui::TextColored(ToVec4(DS::TextHint), "%s", str.minutes);
    ImGui::SetNextItemWidth(halfW);
    ImGui::InputInt("##oclock_min", &m_InputMin, 0, 0);
    ImGui::EndGroup();

    ImGui::SameLine(0, 8);

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

    // ── Sentido del conteo (nuevo) ───────────────────────────────────────
    RenderDirectionSelector();

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
    ImGui::Checkbox("Prefijo signo en overtime", &m_ShowSignPrefix);
    RenderStyleSelector();

    // ── Título / mensaje editable (nuevo) ────────────────────────────────
    RenderTitleSection();

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