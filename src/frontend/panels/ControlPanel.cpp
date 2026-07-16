#include "ControlPanel.h"
#include "UIManager.h"
#include "UIStrings.h"
#include "DesignSystem.h"
#include "frontend/ui/IconRail.h"
#include "frontend/ui/AppIcons.h"
#include "../settings/SettingsManager.h"
#include "../settings/ProjectionQualityPresets.h"
#include "frontend/ui/bin/StyleGeneralApp.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>

#include "backend/core/PresentationCore.h"
#include "stb_image_write.h"

namespace ProyecThor::UI {

namespace DS = ProyecThor::UI::DS;

static constexpr float kPulseSpeed = 2.0f;
static constexpr float kIconBtnSize = 32.0f;
static constexpr float kArrowBtnSize = 24.0f;

// =============================================================================
//  Utilidades de Color — mismo patron que el resto de paneles (CapturePanel,
//  LibraryPanel, etc): todos los colores salen de DS:: (DesignSystem), que a
//  su vez se recalcula desde el tema activo en DS::SyncFromTheme.
// =============================================================================
static ImVec4 ToVec4(ImU32 col) {
    return ImGui::ColorConvertU32ToFloat4(col);
}
static ImU32 ColA(ImU32 col, int a) {
    return (col & 0x00FFFFFFu) | (static_cast<ImU32>(std::clamp(a, 0, 255)) << 24);
}
static ImVec4 Brighten(const ImVec4& c, float amount)
{
    return ImVec4(
        std::clamp(c.x + amount, 0.0f, 1.0f),
        std::clamp(c.y + amount, 0.0f, 1.0f),
        std::clamp(c.z + amount, 0.0f, 1.0f),
        c.w);
}

// =============================================================================
//  ThemeIconButton — boton de icono generico, coloreado por parametros (los
//  call-sites pasan colores derivados de DS::).
// =============================================================================
static bool ThemeIconButton(const char* id,
                            const char* iconKey,
                            const char* fallbackGlyph,
                            const char* tooltip,
                            ImVec2      size,
                            ImVec4      bgColor,
                            ImVec4      hoverColor,
                            ImVec4      activeColor,
                            ImVec4      tint,
                            bool        toggledOn = false)
{
    ImVec4 restColor = toggledOn ? activeColor : bgColor;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);
    ImGui::PushStyleColor(ImGuiCol_Button,        restColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  activeColor);
    ImGui::PushStyleColor(ImGuiCol_Text,          tint);

    auto it = StyleGeneralApp::Icons.find(iconKey);
    bool hasIcon = (it != StyleGeneralApp::Icons.end() && it->second.textureID != nullptr);
    std::string label = (hasIcon ? "" : std::string(fallbackGlyph)) + "##" + id;

    bool clicked = ImGui::Button(label.c_str(), size);

    if (hasIcon)
    {
        ImVec2 bMin = ImGui::GetItemRectMin();
        ImVec2 bMax = ImGui::GetItemRectMax();
        const float minSide  = std::min(size.x, size.y);
        const float iconSide = minSide * 0.50f;
        const ImVec2 center  = { (bMin.x + bMax.x) * 0.5f, (bMin.y + bMax.y) * 0.5f };
        const ImVec2 pMin    = { center.x - iconSide * 0.5f, center.y - iconSide * 0.5f };
        const ImVec2 pMax    = { center.x + iconSide * 0.5f, center.y + iconSide * 0.5f };

        ImGui::GetWindowDrawList()->AddImage(
            it->second.textureID, pMin, pMax,
            ImVec2(0, 0), ImVec2(1, 1),
            ImGui::ColorConvertFloat4ToU32(tint));
    }

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();

    if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        ImGui::SetTooltip("%s", tooltip);

    return clicked;
}

// =============================================================================
//  Tarjetas — mismo lenguaje visual que DS::BeginGlassPanel (fill, borde,
//  highlight, sombra), anidado como child dentro del panel.
// =============================================================================
static bool BeginCard(const char* id, float minHeight = 0.0f)
{
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    float  w  = ImGui::GetContentRegionAvail().x;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, DS::RadiusMedium);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.f, 14.f));

    bool open = ImGui::BeginChild(id,
                             minHeight > 0.0f ? ImVec2(0.f, minHeight) : ImVec2(0.f, 0.f),
                             true,
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImVec2 p1 = ImVec2(p0.x + w, p0.y + ImGui::GetWindowSize().y);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilledMultiColor(p0, p1, DS::GlassFillTop, DS::GlassFillTop, DS::GlassFillBot, DS::GlassFillBot);
    dl->AddRect(p0, p1, DS::GlassBorder, DS::RadiusMedium, 0, 1.0f);

    float hx0 = p0.x + DS::RadiusMedium, hx1 = p1.x - DS::RadiusMedium;
    if (hx1 > hx0) {
        dl->AddLine(ImVec2(hx0, p0.y + 1.0f), ImVec2(hx1, p0.y + 1.0f), DS::GlassHighlight, 1.0f);
        dl->AddLine(ImVec2(hx0, p1.y - 1.0f), ImVec2(hx1, p1.y - 1.0f), IM_COL32(0, 0, 0, 255), 1.0f);
    }

    return open;
}

static void EndCard()
{
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

// =============================================================================
//  DetectCurrentMonitorIndex — que pantalla fisica ocupa la ventana principal
//  (la que corre esta app). Sirve para marcar "(este monitor)" en los
//  selectores y asi evitar que el usuario elija por error la misma pantalla
//  donde ve el panel de control como destino publico o de stage.
// =============================================================================
static int DetectCurrentMonitorIndex()
{
    GLFWwindow* win = glfwGetCurrentContext();
    if (!win) return -1;

    int wx, wy, ww, wh;
    glfwGetWindowPos(win, &wx, &wy);
    glfwGetWindowSize(win, &ww, &wh);
    const int cx = wx + ww / 2;
    const int cy = wy + wh / 2;

    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

    for (int i = 0; i < monitorCount; i++) {
        int mx, my;
        glfwGetMonitorPos(monitors[i], &mx, &my);
        if (const GLFWvidmode* vm = glfwGetVideoMode(monitors[i])) {
            if (cx >= mx && cx < mx + vm->width && cy >= my && cy < my + vm->height)
                return i;
        }
    }
    return -1;
}

// =============================================================================
//  MonitorSelector
//  includeLAN: agrega una entrada final "Red (LAN)"; al elegirla, onPick
//  recibe el sentinela `monitorCount` (fuera de rango de pantallas fisicas).
// =============================================================================
static void MonitorSelector(const char* idPrefix, int selected, int monitorCountOverride,
                             bool includeLAN, int currentAppMonitor,
                             std::function<void(int)> onCycle,
                             std::function<void(int)> onPick)
{
    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    if (monitorCountOverride >= 0) monitorCount = std::min(monitorCount, monitorCountOverride);

    float rowW = ImGui::GetContentRegionAvail().x;
    const float comboW = rowW - 2.0f * (kArrowBtnSize + 6.0f);

    std::string prevId = std::string(idPrefix) + "Prev";
    std::string nextId = std::string(idPrefix) + "Next";
    std::string comboId = std::string("##") + idPrefix + "sel";

    ImVec4 arrowBg   = ToVec4(DS::BtnDefaultFill);
    ImVec4 arrowText = ToVec4(DS::TextSecondary);

    if (ThemeIconButton(prevId.c_str(), "arrow_back", "<", "Opcion anterior",
                        ImVec2(kArrowBtnSize, kArrowBtnSize),
                        arrowBg, Brighten(arrowBg, 0.05f), Brighten(arrowBg, 0.1f), arrowText))
    {
        onCycle(-1);
    }

    ImGui::SameLine(0.0f, 6.0f);

    static thread_local std::vector<std::string> labels;
    static thread_local std::vector<const char*> ptrs;
    labels.clear(); ptrs.clear();
    for (int i = 0; i < monitorCount; i++) {
        std::string l = "Pantalla " + std::to_string(i + 1) + ": " + glfwGetMonitorName(monitors[i]);
        if (i == currentAppMonitor) l += " (este monitor)";
        labels.push_back(std::move(l));
    }
    if (includeLAN)
        labels.push_back("Red (LAN) - navegador/celular");
    for (const auto& l : labels) ptrs.push_back(l.c_str());

    const int totalItems = (int)ptrs.size();
    int sel = std::clamp(selected, 0, std::max(0, totalItems - 1));

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ToVec4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ToVec4(DS::BtnHoverFill));
    ImGui::PushStyleColor(ImGuiCol_PopupBg,        ToVec4(DS::GlassFillTop));
    ImGui::PushStyleColor(ImGuiCol_Border,         ToVec4(DS::GlassBorder));

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(10.0f, 6.0f));

    ImGui::SetNextItemWidth(comboW);
    if (ImGui::Combo(comboId.c_str(), &sel, ptrs.data(), totalItems))
        onPick(sel);

    if (ImGui::IsItemHovered()) {
        if (sel < monitorCount) {
            if (const GLFWvidmode* vm = glfwGetVideoMode(monitors[sel]))
                ImGui::SetTooltip("%dx%d", vm->width, vm->height);
        } else if (includeLAN) {
            ImGui::SetTooltip("Cualquier dispositivo en la misma red WiFi podra verlo desde su navegador.");
        }
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);

    ImGui::SameLine(0.0f, 6.0f);

    if (ThemeIconButton(nextId.c_str(), "arrow_forward", ">", "Opcion siguiente",
                        ImVec2(kArrowBtnSize, kArrowBtnSize),
                        arrowBg, Brighten(arrowBg, 0.05f), Brighten(arrowBg, 0.1f), arrowText))
    {
        onCycle(1);
    }
}

// =============================================================================
//  Constructor
// =============================================================================
ControlPanel::ControlPanel(UIManager* uiManager)
    : m_UIManager(uiManager)
{}

// ─────────────────────────────────────────────────────────────────────────────
//  Render principal — hub de Control + Stage Display, rail de iconos a la
//  derecha (ver IconRail.h). Contenido de cada seccion en RenderControlContent()
//  / m_StageDisplay.RenderContent().
// ─────────────────────────────────────────────────────────────────────────────
void ControlPanel::Render() {
    static double s_LastTime = glfwGetTime();
    double now = glfwGetTime();
    float  dt  = static_cast<float>(now - s_LastTime);
    s_LastTime = now;
    dt = std::min(dt, 0.05f);

    bool visible = m_UIManager
        ? DS::BeginGlassPanel(GetName().c_str(), m_UIManager->GetGlassRenderer(),
                              nullptr, 0, ImVec2(16.0f, 16.0f))
        : ImGui::Begin(GetName().c_str());

    if (!visible) {
        if (m_UIManager) DS::EndGlassPanel(); else ImGui::End();
        return;
    }

    const float railW  = kIconRailVerticalSize;
    const float totalH = ImGui::GetContentRegionAvail().y;
    const float contentW = ImGui::GetContentRegionAvail().x - railW - 1.0f;

    // ── Contenido primero (a la izquierda) ──────────────────────────────────
    ImGui::BeginChild("##controlContent", ImVec2(contentW, totalH), false, 0);

    switch (m_CurrentSection) {
        case ControlSection::Control:      RenderControlContent(dt);       break;
        case ControlSection::StageDisplay: m_StageDisplay.RenderContent(); break;
    }

    ImGui::EndChild();

    // ── Divisor vertical con gradiente ──────────────────────────────────────
    ImGui::SameLine(0.f, 0.f);
    {
        ImVec2      p  = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 colTop   = IM_COL32(60, 80, 160,  0);
        ImU32 colMid   = IM_COL32(60, 80, 160, 80);
        ImU32 colBot   = IM_COL32(60, 80, 160,  0);
        float midY     = p.y + totalH * 0.5f;
        dl->AddRectFilledMultiColor(p, { p.x + 1.f, midY }, colTop, colTop, colMid, colMid);
        dl->AddRectFilledMultiColor({ p.x, midY }, { p.x + 1.f, p.y + totalH }, colMid, colMid, colBot, colBot);
    }
    ImGui::SameLine(0.f, 1.0f);

    // ── Rail de iconos a la derecha ──────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::BeginChild("##controlRail", ImVec2(railW, totalH), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    {
        static const IconRailItem kItems[] = {
            { (int)ControlSection::Control,      AppIcons::DrawIcon_Mixer,   "Control" },
            { (int)ControlSection::StageDisplay, AppIcons::DrawIcon_Monitor, "Stage"   },
        };
        const auto& hubSettings = ProyecThor::Settings::SettingsManager::Get().GetSettings().controlHub;
        int currentIndex = (int)m_CurrentSection;
        RenderIconRail(kItems, 2, currentIndex, IconRailOrientation::Vertical, hubSettings.categoryColor);
        m_CurrentSection = (ControlSection)currentIndex;
    }

    ImGui::EndChild();

    if (m_UIManager) DS::EndGlassPanel(); else ImGui::End();
}

void ControlPanel::RenderControlContent(float dt) {
    if (Core::PresentationCore::Get().IsProjecting())
        m_PulseTime += dt * kPulseSpeed;
    else
        m_PulseTime = std::fmod(m_PulseTime + dt * 0.5f, 6.2831853f);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    // Reducir spacing vertical radicalmente
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12.f, 10.f));

    ImGui::BeginChild("ControlPad", ImGui::GetContentRegionAvail(), false, 0);

    DS::GlassSectionHeader("ACCIONES RÁPIDAS");
    RenderActionRow(dt);

    ImGui::Spacing();
    DS::GlassSectionHeader("CONTROL DE PROYECCIÓN");
    RenderProjectButton(dt);

    ImGui::Spacing();
    DS::GlassSectionHeader("ENRUTAMIENTO DE PANTALLAS");
    RenderMonitorInfo();
    RenderOutputQuality();

    ImGui::Spacing();
    DS::GlassSectionHeader("MONITOR DE CONTROL");
    RenderStageSection(dt);

    ImGui::EndChild();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tarjetas más compactas
// ─────────────────────────────────────────────────────────────────────────────
void ControlPanel::RenderMonitorInfo() {
    int monitorCount = 0;
    glfwGetMonitors(&monitorCount);
    auto& settings = ProyecThor::Settings::SettingsManager::Get().GetSettings();
    int currentAppMonitor = DetectCurrentMonitorIndex();

    int  sel = -1;
    bool sameAsControl = false;
    if (monitorCount >= 2) {
        int tgt = settings.projection.targetMonitor;
        sel = std::clamp(tgt < 0 ? 1 : tgt, 0, monitorCount - 1);
        sameAsControl = (sel == currentAppMonitor);
    }

    BeginCard("ProjCard", sameAsControl ? 156.0f : 120.0f);
    ImGui::TextUnformatted("Pantalla pública (lo que ve la audiencia)");
    ImGui::TextDisabled("Monitores detectados: ");
    ImGui::SameLine();
    ImGui::TextColored(ToVec4(DS::TextPrimary), "%d", monitorCount);
    ImGui::Spacing();

    if (monitorCount < 2) {
        ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(ColA(DS::DangerColor, 230)));
        ImGui::Text("Se necesita una segunda pantalla para proyectar.");
        ImGui::PopStyleColor();
    } else {
        MonitorSelector("mon", sel, monitorCount, /*includeLAN*/false, currentAppMonitor,
            [this](int dir) { CycleTargetMonitor(dir); },
            [this, &settings](int newSel) {
                settings.projection.targetMonitor = newSel;
                ProyecThor::Settings::SettingsManager::Get().Save();
                auto& core = Core::PresentationCore::Get();
                if (core.IsProjectorWindowActive()) {
                    core.DestroyProjectorWindow();
                    core.CreateProjectorWindow(newSel);
                }
            });

        if (sameAsControl) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(ColA(DS::DangerColor, 230)));
            ImGui::TextWrapped("Atención: elegiste la misma pantalla donde se ve este panel de control como destino público.");
            ImGui::PopStyleColor();
        }
    }
    EndCard();
}

void ControlPanel::RenderOutputQuality() {
    using namespace ProyecThor::Settings;
    auto& settings = ProyecThor::Settings::SettingsManager::Get().GetSettings();
    auto& p = settings.projection;

    BeginCard("QualityCard", 92.0f);
    ImGui::TextUnformatted("Calidad de salida (video de fondo)");
    ImGui::Spacing();

    static thread_local std::vector<std::string> labels;
    static thread_local std::vector<const char*> ptrs;
    labels.clear(); ptrs.clear();
    labels.push_back("Auto");
    for (const auto& preset : kQualityPresets) labels.push_back(preset.label);
    labels.push_back("Personalizado (ver Ajustes > Proyección)");
    for (const auto& l : labels) ptrs.push_back(l.c_str());

    // sel: 0=Auto, 1..N=presets, N+1=Custom
    auto mode = static_cast<OutputQualityMode>(p.outputQualityMode);
    int sel = 0;
    if (mode == OutputQualityMode::Preset)
        sel = 1 + std::clamp(p.outputPresetIndex, 0, kQualityPresetCount - 1);
    else if (mode == OutputQualityMode::Custom)
        sel = 1 + kQualityPresetCount;

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ToVec4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ToVec4(DS::BtnHoverFill));
    ImGui::PushStyleColor(ImGuiCol_PopupBg,        ToVec4(DS::GlassFillTop));
    ImGui::PushStyleColor(ImGuiCol_Border,         ToVec4(DS::GlassBorder));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(10.0f, 6.0f));

    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::Combo("##outputQuality", &sel, ptrs.data(), (int)ptrs.size())) {
        if (sel == 0) {
            p.outputQualityMode = (int)OutputQualityMode::Auto;
        } else if (sel <= kQualityPresetCount) {
            p.outputQualityMode = (int)OutputQualityMode::Preset;
            p.outputPresetIndex = sel - 1;
        } else {
            p.outputQualityMode = (int)OutputQualityMode::Custom;
        }
        ProyecThor::Settings::SettingsManager::Get().Save();
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    EndCard();
}

void ControlPanel::RenderStageSection(float dt) {
    (void)dt;
    auto& core = Core::PresentationCore::Get();

    int monitorCount = 0;
    glfwGetMonitors(&monitorCount);
    const bool hasPhysicalOption = monitorCount >= 2;
    const int  currentAppMonitor = DetectCurrentMonitorIndex();

    // Sin segunda pantalla fisica, LAN es la unica opcion posible para el
    // monitor de control — no tiene sentido dejarlo "apagado" por eleccion.
    if (!hasPhysicalOption) m_StageUseLAN = true;
    if (hasPhysicalOption)  m_StageMonitorIndex = std::clamp(m_StageMonitorIndex, 0, monitorCount - 1);

    // El estado real de "activo" se consulta a la fuente correspondiente en
    // vez de fiarse solo del booleano local: si el usuario tambien controla
    // la transmision LAN desde el panel "Transmisión en Red", este panel debe
    // reflejar eso igual (evita que ambos paneles queden desincronizados).
    const bool stageActive = m_StageUseLAN ? core.IsStreamingNet() : core.IsStaging();

    const int  lanItemIndex = hasPhysicalOption ? monitorCount : 0;
    const int  sel          = m_StageUseLAN ? lanItemIndex : m_StageMonitorIndex;
    const bool showLanPanel = m_StageUseLAN && stageActive;

    BeginCard("StageCard", showLanPanel ? 226.0f : (hasPhysicalOption ? 190.0f : 168.0f));

    ImGui::TextUnformatted(stageActive ? "Stage activo" : "Stage inactivo");
    ImGui::TextDisabled("Monitor de confianza: ");
    ImGui::SameLine();
    if (m_StageUseLAN)
        ImGui::TextColored(ToVec4(DS::TextPrimary), "Red (LAN)");
    else
        ImGui::TextColored(ToVec4(DS::TextPrimary), "Pantalla %d", m_StageMonitorIndex + 1);
    ImGui::Spacing();

    if (!hasPhysicalOption) {
        ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextSecondary));
        ImGui::TextWrapped(
            "No se detectó una segunda pantalla física: el monitor de control "
            "estará disponible solo por LAN. Cualquier celular o tablet en la "
            "misma red WiFi podrá verlo desde su navegador.");
        ImGui::PopStyleColor();
    } else {
        MonitorSelector("stage", sel, -1, /*includeLAN*/true, currentAppMonitor,
            [this](int dir) { CycleStageMonitor(dir); },
            [this, stageActive](int newSel) {
                int mc = 0;
                glfwGetMonitors(&mc);
                const bool wantLAN = (newSel >= mc);
                auto& core = Core::PresentationCore::Get();

                if (stageActive) {
                    if (m_StageUseLAN) core.ToggleNetworkStream(false);
                    else               core.SetStaging(false);
                }

                m_StageUseLAN = wantLAN;
                if (!wantLAN) m_StageMonitorIndex = newSel;

                if (stageActive) {
                    if (wantLAN) core.ToggleNetworkStream(true, m_LANPort);
                    else         core.SetStaging(true, m_StageMonitorIndex);
                }
            });

        ImGui::Spacing();
        ImGui::TextDisabled("Muestra el contenido en vivo a un segundo público (músicos, camarógrafos, etc).");
    }
    ImGui::Spacing();

    if (showLanPanel) {
        auto state = core.GetState();

        ImGui::PushStyleColor(ImGuiCol_FrameBg, ToVec4(DS::BtnDefaultFill));
        ImGui::PushStyleColor(ImGuiCol_Border,  ToVec4(DS::GlassBorder));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);

        char urlBuf[256];
        std::strncpy(urlBuf, state.networkURL.c_str(), sizeof(urlBuf) - 1);
        urlBuf[sizeof(urlBuf) - 1] = '\0';

        const float btnW   = 90.0f;
        const float gap    = 8.0f;
        const float fieldW = ImGui::GetContentRegionAvail().x - btnW - gap;

        ImGui::SetNextItemWidth(fieldW);
        ImGui::InputText("##lanurl", urlBuf, sizeof(urlBuf), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);

        ImGui::SameLine(0.0f, gap);
        if (ImGui::Button("Copiar Link", ImVec2(btnW, 0.0f)))
            ImGui::SetClipboardText(state.networkURL.c_str());

        ImGui::TextDisabled("Más opciones de calidad/resolución en el panel \"Transmisión en Red\".");
        ImGui::Spacing();
    }

    const char* buttonText = stageActive ? "DETENER STAGE" : "ACTIVAR STAGE";
    ImVec4 btnColor    = stageActive ? ToVec4(ColA(DS::AccentColor, 217)) : ToVec4(DS::BtnDefaultFill);
    ImVec4 hoverColor  = Brighten(btnColor, 0.06f);
    ImVec4 activeColor = Brighten(btnColor, -0.06f);
    ImVec4 tintCol     = stageActive ? ToVec4(DS::TextPrimary) : ToVec4(DS::TextHint);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
    ImGui::PushStyleColor(ImGuiCol_Button, btnColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);
    ImGui::PushStyleColor(ImGuiCol_Text, tintCol);

    if (ImGui::Button(buttonText, ImVec2(-1.0f, 44.0f))) {
        ToggleStageDisplay(!stageActive);
    }

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();

    EndCard();

    // Si el monitor de control se sirve por LAN, este panel es responsable de
    // alimentar el servidor con frames de video (igual que hace StreamingPanel
    // cuando esta abierto), para que el mirror funcione aunque el usuario nunca
    // haya abierto el panel "Transmisión en Red".
    if (showLanPanel) {
        double now = ImGui::GetTime();
        if (now - m_LANLastCaptureTime >= (1.0 / 8.0)) {
            m_LANLastCaptureTime = now;
            CaptureAndPushLANFrame(1280, 720, 80);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Row de Acciones (¡AQUÍ SE AÑADE EL BOTÓN MUTE!)
// ─────────────────────────────────────────────────────────────────────────────
void ControlPanel::RenderActionRow(float dt) {
    (void)dt;
    bool stretchOn = Core::PresentationCore::Get().GetStretchToFill();
    bool isMuted   = Core::PresentationCore::Get().GetLiveMute(); // Leer estado vivo

    BeginCard("ActionsCard", 100.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.f, 6.f));

    ImVec4 baseFill    = ToVec4(DS::BtnDefaultFill);
    ImVec4 hoverClear  = ToVec4(DS::AccentColorDim);
    ImVec4 hoverStop   = ToVec4(ColA(DS::DangerColor, 89));
    ImVec4 hoverStretch= ToVec4(DS::AccentColorDim);
    ImVec4 activeStretch = ToVec4(ColA(DS::AccentColor, 140));
    ImVec4 hoverMute   = ToVec4(DS::DangerColor);
    ImVec4 activeMute  = Brighten(ToVec4(DS::DangerColor), 0.12f);
    ImVec4 textPrimary = ToVec4(DS::TextPrimary);
    ImVec4 textDanger  = ToVec4(DS::DangerColor);

    struct ActionDef {
        const char* id;
        const char* icon;
        const char* label;
        const char* tooltip;
        ImVec4 hoverColor;
        ImVec4 activeColor;
    };

    ActionDef actions[5] = {
        { "actClearText", "cleaning_services", "Limpiar", "Limpiar texto", hoverClear, baseFill },
        { "actClearBg",   "delete",            "Fondo",  "Quitar fondo",  hoverStop,  baseFill },
        { "actStretch",   stretchOn ? "original_screen" : "fit_screen", stretchOn ? "Normal" : "Ajustar", "Alternar proporción", hoverStretch, activeStretch },
        { "actMute",      isMuted ? "volume_off" : "volume_up", isMuted ? "Silencio" : "Audio", "Mutear / Desmutear audio vivo", isMuted ? hoverMute : hoverStretch, activeMute },
        { "actPrefs",     "settings",         "Ajustes", "Abrir preferencias", hoverClear, baseFill },
    };

    if (ImGui::BeginTable("ActionGrid", 5, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_NoPadOuterX)) {
        for (int i = 0; i < 5; i++) {
            ImGui::TableNextColumn();
            ImGui::BeginGroup();

            float cellWidth = ImGui::GetContentRegionAvail().x;
            bool toggledOn = (i == 2 && stretchOn) || (i == 3 && isMuted);
            ImVec4 activeCol = toggledOn ? actions[i].activeColor : actions[i].hoverColor;
            ImVec4 tint = (i == 3 && isMuted) ? textDanger : textPrimary;

            if (ThemeIconButton(actions[i].id, actions[i].icon, actions[i].label, actions[i].tooltip,
                                ImVec2(cellWidth, 34.0f),
                                baseFill, actions[i].hoverColor, activeCol, tint, toggledOn))
            {
                if (i == 0)      Core::PresentationCore::Get().ClearLayer2();
                else if (i == 1) Core::PresentationCore::Get().StopBackgroundMedia();
                else if (i == 2) Core::PresentationCore::Get().SetStretchToFill(!stretchOn);
                else if (i == 3) Core::PresentationCore::Get().SetLiveMute(!isMuted);
                else if (i == 4 && m_UIManager) m_UIManager->RequestSettings();
            }

            ImGui::Dummy(ImVec2(0, 2));
            float textWidth = ImGui::CalcTextSize(actions[i].label).x;
            float pad = std::max(0.0f, (cellWidth - textWidth) * 0.5f);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + pad);
            ImGui::TextDisabled("%s", actions[i].label);
            ImGui::EndGroup();
        }
        ImGui::EndTable();
    }

    ImGui::PopStyleVar();
    EndCard();
}

void ControlPanel::RenderProjectButton(float /*dt*/) {
    auto& core = Core::PresentationCore::Get();
    bool isProjecting = core.IsProjecting();
    auto& settings = ProyecThor::Settings::SettingsManager::Get().GetSettings();

    int monitorCount = 0;
    glfwGetMonitors(&monitorCount);
    int targetMonitor = std::clamp(settings.projection.targetMonitor < 0 ? 1 : settings.projection.targetMonitor,
                                   0, std::max(0, monitorCount - 1));

    BeginCard("ProjectCard", 120.0f);
    ImGui::TextUnformatted(isProjecting ? "Proyección activa" : "Proyección inactiva");
    ImGui::TextDisabled("Monitor público: ");
    ImGui::SameLine();
    ImGui::TextColored(ToVec4(DS::TextPrimary), "Pantalla %d", targetMonitor + 1);
    ImGui::Spacing();

    ImVec4 baseColor  = isProjecting ? ToVec4(ColA(DS::DangerColor, 217)) : ToVec4(DS::AccentColorDim);
    ImVec4 hoverColor = Brighten(baseColor, 0.10f);
    ImVec4 activeColor = Brighten(baseColor, -0.08f);
    const char* buttonText = isProjecting ? "DETENER PROYECCIÓN" : "INICIAR PROYECCIÓN";
    const char* helpText = isProjecting ? "Corta todas las salidas públicas" : "Enciende la proyección hacia el público";

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.0f, 12.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
    ImGui::PushStyleColor(ImGuiCol_Button, baseColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextPrimary));

    if (ImGui::Button(buttonText, ImVec2(-1.0f, 42.0f))) {
        ToggleSecondaryDisplay(!isProjecting);
    }

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);

    ImGui::TextDisabled("%s", helpText);
    EndCard();
}

void ControlPanel::CycleTargetMonitor(int direction) {
    int monitorCount = 0;
    glfwGetMonitors(&monitorCount);
    if (monitorCount < 2)
        return;

    auto& settings = ProyecThor::Settings::SettingsManager::Get().GetSettings();
    int current = settings.projection.targetMonitor < 0 ? 1 : settings.projection.targetMonitor;
    current = std::clamp(current, 0, monitorCount - 1);
    current = (current + direction + monitorCount) % monitorCount;
    settings.projection.targetMonitor = current;
    ProyecThor::Settings::SettingsManager::Get().Save();

    auto& core = Core::PresentationCore::Get();
    if (core.IsProjectorWindowActive()) {
        core.DestroyProjectorWindow();
        core.CreateProjectorWindow(current);
    }
}

void ControlPanel::CycleStageMonitor(int direction) {
    int monitorCount = 0;
    glfwGetMonitors(&monitorCount);
    const bool hasPhysicalOption = monitorCount >= 2;

    const int totalItems = (hasPhysicalOption ? monitorCount : 0) + 1;
    if (totalItems <= 1)
        return; // sin pantallas fisicas, LAN es la unica opcion: nada que ciclar

    const int lanItemIndex = totalItems - 1;
    const int current = m_StageUseLAN ? lanItemIndex : std::clamp(m_StageMonitorIndex, 0, monitorCount - 1);
    const int next     = (current + direction + totalItems) % totalItems;
    const bool wantLAN = (next == lanItemIndex);

    auto& core = Core::PresentationCore::Get();
    const bool stageActive = m_StageUseLAN ? core.IsStreamingNet() : core.IsStaging();

    if (stageActive) {
        if (m_StageUseLAN) core.ToggleNetworkStream(false);
        else                core.SetStaging(false);
    }

    m_StageUseLAN = wantLAN;
    if (!wantLAN) m_StageMonitorIndex = next;

    if (stageActive) {
        if (wantLAN) core.ToggleNetworkStream(true, m_LANPort);
        else          core.SetStaging(true, m_StageMonitorIndex);
    }
}

void ControlPanel::ToggleStageDisplay(bool active) {
    auto& core = Core::PresentationCore::Get();
    if (active) {
        if (m_StageUseLAN) {
            core.ToggleNetworkStream(true, m_LANPort);
            if (core.IsStreamingNet())
                std::cout << "[ControlPanel] Monitor de control (LAN) iniciado en puerto " << m_LANPort << ".\n";
            else
                std::cerr << "[ControlPanel] No se pudo iniciar el monitor de control por LAN (puerto "
                          << m_LANPort << " en uso?).\n";
            return;
        }

        int monitorCount = 0;
        glfwGetMonitors(&monitorCount);
        if (monitorCount < 2) {
            std::cerr << "[ControlPanel] No hay suficientes monitores para activar el stage.\n";
            return;
        }

        m_StageMonitorIndex = std::clamp(m_StageMonitorIndex, 0, monitorCount - 1);
        core.SetStaging(true, m_StageMonitorIndex);
        std::cout << "[ControlPanel] Monitor de control iniciado en monitor " << m_StageMonitorIndex << ".\n";
    } else {
        if (m_StageUseLAN) {
            core.ToggleNetworkStream(false);
            std::cout << "[ControlPanel] Monitor de control (LAN) detenido.\n";
        } else {
            core.SetStaging(false);
            std::cout << "[ControlPanel] Monitor de control detenido.\n";
        }
    }
}

void ControlPanel::CaptureAndPushLANFrame(int w, int h, int quality) {
    if (w <= 0 || h <= 0) return;

    auto& core = Core::PresentationCore::Get();
    std::vector<uint8_t> rgb;
    if (!core.RenderProjectorToFBO(w, h, rgb)) return;

    std::vector<uint8_t> jpeg;
    jpeg.reserve(static_cast<size_t>(w) * h / 4);

    auto stbCb = [](void* ctx, void* data, int size) {
        auto* buf = static_cast<std::vector<uint8_t>*>(ctx);
        const uint8_t* p = static_cast<const uint8_t*>(data);
        buf->insert(buf->end(), p, p + size);
    };
    stbi_write_jpg_to_func(stbCb, &jpeg, w, h, 3, rgb.data(), quality);

    core.PushFrame(std::move(jpeg));
}

void ControlPanel::ToggleSecondaryDisplay(bool active) {
    auto& core = Core::PresentationCore::Get();
    core.SetProjecting(active);
    if (active) {
        auto& settings = ProyecThor::Settings::SettingsManager::Get().GetSettings();

        int monitorCount = 0;
        glfwGetMonitors(&monitorCount);
        int monitorIndex = std::clamp(
            settings.projection.targetMonitor < 0 ? 1 : settings.projection.targetMonitor,
            0, std::max(0, monitorCount - 1));

        if (core.CreateProjectorWindow(monitorIndex))
            std::cout << "[ControlPanel] Proyección iniciada en monitor " << monitorIndex << ".\n";
        else
            std::cerr << "[ControlPanel] No se pudo crear la ventana de proyección.\n";
    } else {
        core.DestroyProjectorWindow();
        std::cout << "[ControlPanel] Proyección detenida.\n";
    }
}

} // namespace ProyecThor::UI
