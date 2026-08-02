#include "StylesHubPanel.h"
#include "TransitionPanel.h"
#include "UIManager.h"
#include "DesignSystem.h"
#include "frontend/ui/IconRail.h"
#include "frontend/ui/AppIcons.h"
#include "frontend/panels/home/HomeIcons.h"
#include "backend/settings/SettingsManager.h"
#include "backend/core/PresentationCore.h"
#include <imgui.h>

namespace ProyecThor::UI {

StylesHubPanel::StylesHubPanel(UIManager* uiManager)
    : m_UIManager(uiManager)
{
    // Se registra a si mismo (direccion de sus propios miembros) para que
    // ViewPanel pueda pedir "Limpiar anuncios"/"Limpiar captura" sin
    // depender de StylesHubPanel directamente — ver PresentationCore::
    // SetAnnouncementsRef/SetCapturePanelRef.
    Core::PresentationCore::Get().SetAnnouncementsRef(&m_Announcements);
    Core::PresentationCore::Get().SetCapturePanelRef(&m_Capture);

    m_Styles.SetUIManager(uiManager);
}

void StylesHubPanel::RenderTransitionQuickBar()
{
    if (!m_TransitionsRef) return;

    const ImU32  kAccent  = IM_COL32(94, 107, 255, 255);
    const ImVec4 kAccentV = ImGui::ColorConvertU32ToFloat4(kAccent);
    const ImVec4 kMutedV  = ImGui::ColorConvertU32ToFloat4(DS::TextSecondary);
    constexpr float kBtnSz    = 26.0f;
    constexpr float kGap      = 4.0f;
    constexpr float kSliderW  = 90.0f;

    TransitionType current  = m_TransitionsRef->GetCurrentType();
    float          duration = m_TransitionsRef->GetDuration();
    bool           isAdvanced = current != TransitionType::None && current != TransitionType::Fade;

    float rowY = ImGui::GetCursorPosY();

    if (DS::GlassIconButton("##transNone", "", "—", "Sin transicion", { kBtnSz, kBtnSz },
                            current == TransitionType::None ? kAccentV : kMutedV))
        m_TransitionsRef->SetType(TransitionType::None);
    ImGui::SameLine(0.0f, kGap);

    if (DS::GlassIconButton("##transFade", "", "~", "Disolver", { kBtnSz, kBtnSz },
                            current == TransitionType::Fade ? kAccentV : kMutedV))
        m_TransitionsRef->SetType(TransitionType::Fade);
    ImGui::SameLine(0.0f, kGap);

    if (DS::GlassIconButton("##transAdv", "", "…", "Avanzado (Zoom, Slide, Cover...)", { kBtnSz, kBtnSz },
                            isAdvanced ? kAccentV : kMutedV))
        ImGui::OpenPopup("##transAdvancedPopup");
    ImGui::SameLine(0.0f, kGap * 2.0f);

    // Slider chico centrado verticalmente contra los botones de icono (su
    // alto propio, thumbR*2+6, es menor que kBtnSz).
    ImGui::SetCursorPosY(rowY + (kBtnSz - 20.0f) * 0.5f);
    if (DS::ModernSlider("##quickTransDur", &duration, 0.1f, 3.0f, kSliderW, kAccent))
        m_TransitionsRef->SetDuration(duration);
    ImGui::SameLine(0.0f, 6.0f);

    ImGui::SetCursorPosY(rowY + (kBtnSz - ImGui::GetTextLineHeight()) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, kMutedV);
    ImGui::Text("%.2fs", duration);
    ImGui::PopStyleColor();

    ImGui::SetCursorPosY(rowY + kBtnSz);

    if (ImGui::BeginPopup("##transAdvancedPopup"))
    {
        ImGui::SetNextItemWidth(340.0f);
        m_TransitionsRef->RenderContent();
        ImGui::EndPopup();
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1.0f, 1.0f, 1.0f, 0.06f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

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

    const float     railH   = IconRailThickness(false);
    const float     totalW  = ImGui::GetContentRegionAvail().x;
    const float     totalH  = ImGui::GetContentRegionAvail().y;

    // ── Rail de iconos arriba ────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::BeginChild("##stylesRail", ImVec2(totalW, railH), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    {
        // Transiciones ya no es una pestaña propia: sus controles (duracion +
        // 3 botones) ahora viven arriba del contenido de Estilos (ver mas
        // abajo), con el panel completo (tipos complejos: Zoom, Slide,
        // Cover...) disponible como popup desde el 3er boton.
        static const IconRailItem kItems[] = {
            { (int)StylesSection::Backgrounds,   AppIcons::DrawIcon_Layers,    "Fondos"   },
            { (int)StylesSection::Styles,        AppIcons::DrawIcon_TextAa,    "Estilos"  },
            { (int)StylesSection::Shaders,       AppIcons::DrawIcon_Shader,    "Shaders"  },
            { (int)StylesSection::Announcements, HomeIcons::DrawIcon_Megaphone,"Anuncios" },
            { (int)StylesSection::Capture,       HomeIcons::DrawIcon_Camera,   "Captura"  },
        };
        const auto& hubSettings = ProyecThor::Settings::SettingsManager::Get().GetSettings().stylesHub;
        int currentIndex = (int)m_CurrentSection;
        RenderIconRail(kItems, 5, currentIndex, IconRailOrientation::Horizontal, hubSettings.categoryColor);
        m_CurrentSection = (StylesSection)currentIndex;
    }

    ImGui::EndChild();

    // ── Divisor horizontal con gradiente ────────────────────────────────────
    {
        ImVec2      p  = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 colLeft  = IM_COL32(60, 80, 160,  0);
        ImU32 colMid   = IM_COL32(60, 80, 160, 80);
        ImU32 colRight = IM_COL32(60, 80, 160,  0);
        float midX     = p.x + totalW * 0.5f;
        dl->AddRectFilledMultiColor(p, { midX, p.y + 1.f }, colLeft, colMid, colMid, colLeft);
        dl->AddRectFilledMultiColor({ midX, p.y }, { p.x + totalW, p.y + 1.f }, colMid, colRight, colRight, colMid);
        ImGui::Dummy(ImVec2(totalW, 1.0f));
    }

    // ── Contenido de la seccion activa ───────────────────────────────────────
    constexpr float kContentMarginX = 18.0f;
    constexpr float kContentMarginY = 16.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kContentMarginX, kContentMarginY));
    ImGui::BeginChild("##stylesContent", ImVec2(0.f, 0.f),
                      ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();

    switch (m_CurrentSection)
    {
        case StylesSection::Backgrounds: m_Backgrounds.RenderContent(); break;
        case StylesSection::Styles:
            RenderTransitionQuickBar();
            m_Styles.RenderContent();
            break;
        case StylesSection::Shaders:     m_Shaders.RenderContent();    break;
        case StylesSection::Transitions:
            if (m_TransitionsRef) m_TransitionsRef->RenderContent();
            break;
        case StylesSection::Announcements:
            if (m_UIManager) m_Announcements.Render(m_UIManager->GetGlassRenderer());
            break;
        case StylesSection::Capture:
            m_Capture.RenderContent();
            break;
    }

    ImGui::EndChild();

    if (m_UIManager) DS::EndGlassPanel(); else ImGui::End();
}

} // namespace ProyecThor::UI
