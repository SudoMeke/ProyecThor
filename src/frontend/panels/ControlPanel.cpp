#include "ControlPanel.h"
#include "UIManager.h"
#include "UIStrings.h"
#include "DesignSystem.h"
#include "ControlWidgets.h"
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

namespace ProyecThor::UI {

static constexpr float kPulseSpeed = 2.0f;

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

    const float railW  = IconRailThickness(true);
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

    DS::GlassSectionHeader("CONTROL DE PROYECCIÓN");
    RenderProjectButton(dt);

    ImGui::Spacing();
    DS::GlassSectionHeader("ENRUTAMIENTO DE PANTALLAS");
    RenderMonitorInfo();
    RenderOutputQuality();

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

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 hp = ImGui::GetCursorScreenPos();
    ControlIcons::DrawRoute(dl, ImVec2(hp.x + 9.0f, hp.y + 9.0f), 9.0f, ImGui::ColorConvertFloat4ToU32(ToVec4(DS::TextSecondary)));
    ImGui::Indent(24.0f);
    ImGui::TextUnformatted("Pantalla pública (lo que ve la audiencia)");
    ImGui::Unindent(24.0f);
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

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 hp = ImGui::GetCursorScreenPos();
    ControlIcons::DrawQuality(dl, ImVec2(hp.x + 9.0f, hp.y + 9.0f), 9.0f, ImGui::ColorConvertFloat4ToU32(ToVec4(DS::TextSecondary)));
    ImGui::Indent(24.0f);
    ImGui::TextUnformatted("Calidad de salida (video de fondo)");
    ImGui::Unindent(24.0f);
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

void ControlPanel::RenderProjectButton(float /*dt*/) {
    auto& core = Core::PresentationCore::Get();
    bool isProjecting = core.IsProjecting();
    auto& settings = ProyecThor::Settings::SettingsManager::Get().GetSettings();

    int monitorCount = 0;
    glfwGetMonitors(&monitorCount);
    int targetMonitor = std::clamp(settings.projection.targetMonitor < 0 ? 1 : settings.projection.targetMonitor,
                                   0, std::max(0, monitorCount - 1));

    BeginCard("ProjectCard", 120.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 hp = ImGui::GetCursorScreenPos();
    ControlIcons::DrawScreenCast(dl, ImVec2(hp.x + 9.0f, hp.y + 9.0f), 9.0f, ImGui::ColorConvertFloat4ToU32(ToVec4(DS::TextSecondary)));
    ImGui::Indent(24.0f);
    ImGui::TextUnformatted(isProjecting ? "Proyección activa" : "Proyección inactiva");
    ImGui::Unindent(24.0f);
    ImGui::TextDisabled("Monitor público: ");
    ImGui::SameLine();
    ImGui::TextColored(ToVec4(DS::TextPrimary), "Pantalla %d", targetMonitor + 1);
    ImGui::Spacing();

    ImVec4 baseColor  = isProjecting ? ToVec4(ColA(DS::DangerColor, 217)) : ToVec4(DS::AccentColorDim);
    ImVec4 hoverColor = Brighten(baseColor, 0.10f);
    ImVec4 activeColor = Brighten(baseColor, -0.08f);
    const char* buttonText = isProjecting ? "DETENER PROYECCIÓN" : "INICIAR PROYECCIÓN";
    const char* helpText = isProjecting ? "Corta todas las salidas públicas" : "Enciende la proyección hacia el público";
    DrawIconFn icon = isProjecting ? ControlIcons::DrawStop : ControlIcons::DrawPlay;

    if (IconLabelButton("projectToggle", buttonText, icon, ImVec2(-1.0f, 42.0f),
                        baseColor, hoverColor, activeColor, ToVec4(DS::TextPrimary))) {
        ToggleSecondaryDisplay(!isProjecting);
    }

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
