#include "StylesHubPanel.h"
#include "TransitionPanel.h"
#include "UIManager.h"
#include "DesignSystem.h"
#include "frontend/ui/IconRail.h"
#include "frontend/ui/AppIcons.h"
#include "backend/settings/SettingsManager.h"
#include <imgui.h>

namespace ProyecThor::UI {

namespace DS = ProyecThor::UI::DS;

StylesHubPanel::StylesHubPanel(UIManager* uiManager)
    : m_UIManager(uiManager)
{}

void StylesHubPanel::Render()
{
    bool visible = m_UIManager
        ? DS::BeginGlassPanel(GetName().c_str(), m_UIManager->GetGlassRenderer(),
                              nullptr, 0, ImVec2(0.0f, 0.0f))
        : ImGui::Begin(GetName().c_str());

    if (!visible) {
        if (m_UIManager) DS::EndGlassPanel(); else ImGui::End();
        return;
    }

    constexpr float railW  = kIconRailVerticalSize;
    const float     totalH = ImGui::GetContentRegionAvail().y;

    // ── Rail de iconos a la izquierda ────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::BeginChild("##stylesRail", ImVec2(railW, totalH), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    {
        static const IconRailItem kItems[] = {
            { (int)StylesSection::Backgrounds,  AppIcons::DrawIcon_Layers,  "Fondos"   },
            { (int)StylesSection::Styles,       AppIcons::DrawIcon_Palette,"Estilos"  },
            { (int)StylesSection::Transitions,  AppIcons::DrawIcon_Swap,   "Trans."   },
        };
        const auto& hubSettings = ProyecThor::Settings::SettingsManager::Get().GetSettings().stylesHub;
        int currentIndex = (int)m_CurrentSection;
        RenderIconRail(kItems, 3, currentIndex, IconRailOrientation::Vertical, hubSettings.categoryColor);
        m_CurrentSection = (StylesSection)currentIndex;
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

    // ── Contenido de la seccion activa ───────────────────────────────────────
    constexpr float kContentMarginX = 18.0f;
    constexpr float kContentMarginY = 16.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kContentMarginX, kContentMarginY));
    ImGui::BeginChild("##stylesContent", ImVec2(0.f, totalH),
                      ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();

    switch (m_CurrentSection)
    {
        case StylesSection::Backgrounds: m_Backgrounds.RenderContent(); break;
        case StylesSection::Styles:      m_Styles.RenderContent();     break;
        case StylesSection::Transitions:
            if (m_TransitionsRef) m_TransitionsRef->RenderContent();
            break;
    }

    ImGui::EndChild();

    if (m_UIManager) DS::EndGlassPanel(); else ImGui::End();
}

} // namespace ProyecThor::UI
