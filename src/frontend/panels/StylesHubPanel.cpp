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
#include <algorithm>

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

    if (DS::GlassIconButton("##transNone", "", "—", "Sin transición", { kBtnSz, kBtnSz },
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
}

// Icono chico en el rail de arriba (junto a Fondos/Estilos/Shaders/...) que
// abre RenderTransitionQuickBar como popup flotante -- reemplaza la barra
// fija que antes vivia arriba del contenido de Estilos, empujandolo hacia
// abajo para algo que en la practica solo importa para canciones (multi-
// slide). Se dibuja siempre (no solo en la seccion Estilos) porque configura
// una transicion global, no algo propio de esa pestaña.
void StylesHubPanel::RenderTransitionRailButton()
{
    if (!m_TransitionsRef) return;

    const ImVec4 kMutedV = ImGui::ColorConvertU32ToFloat4(DS::TextSecondary);
    constexpr float kBtnSz = 26.0f;

    if (DS::GlassIconButton("##transRail", "", "~", "Transición", { kBtnSz, kBtnSz }, kMutedV))
        ImGui::OpenPopup("##transQuickPopup");

    if (ImGui::BeginPopup("##transQuickPopup")) {
        RenderTransitionQuickBar();
        ImGui::EndPopup();
    }
}

// Divisor con gradiente entre el rail de iconos y el contenido -- horizontal
// (linea abajo del rail) cuando el rail va arriba, vertical (linea al lado)
// cuando el rail pasa a columna izquierda. length = ancho o alto disponible
// segun corresponda.
static void RenderStylesHubDivider(bool vertical, float length)
{
    ImVec2      p      = ImGui::GetCursorScreenPos();
    ImDrawList* dl     = ImGui::GetWindowDrawList();
    ImU32       colEdge = IM_COL32(60, 80, 160,  0);
    ImU32       colMid  = IM_COL32(60, 80, 160, 80);

    if (vertical) {
        float midY = p.y + length * 0.5f;
        dl->AddRectFilledMultiColor(p, { p.x + 1.f, midY }, colEdge, colEdge, colMid, colMid);
        dl->AddRectFilledMultiColor({ p.x, midY }, { p.x + 1.f, p.y + length }, colMid, colMid, colEdge, colEdge);
        ImGui::Dummy(ImVec2(1.0f, length));
    } else {
        float midX = p.x + length * 0.5f;
        dl->AddRectFilledMultiColor(p, { midX, p.y + 1.f }, colEdge, colMid, colMid, colEdge);
        dl->AddRectFilledMultiColor({ midX, p.y }, { p.x + length, p.y + 1.f }, colMid, colEdge, colEdge, colMid);
        ImGui::Dummy(ImVec2(length, 1.0f));
    }
}

void StylesHubPanel::Render()
{
    // Alt Gr + 2: si Diseño esta colapsado (o pasando el punto medio de la
    // animacion), no dibujar la ventana ni sus tabs internas.
    if (m_UIManager && m_UIManager->IsPanelCollapsedForRender(GetName()))
        return;

    bool visible = m_UIManager
        ? DS::BeginGlassPanel(GetName().c_str(), m_UIManager->GetGlassRenderer(),
                              nullptr, 0, ImVec2(0.0f, 0.0f))
        : ImGui::Begin(GetName().c_str());

    if (!visible) {
        if (m_UIManager) DS::EndGlassPanel(); else ImGui::End();
        return;
    }

    const float totalW = ImGui::GetContentRegionAvail().x;
    const float totalH = ImGui::GetContentRegionAvail().y;

    // Cuando el panel queda mas alto que ancho (columna angosta -- ej.
    // "Diseño" apilado junto a "Vista en Vivo" en Ajustes > Apariencia >
    // Entorno de trabajo > Simple), el rail de iconos pasa a vertical en el
    // lateral izquierdo: una fila horizontal de 6 iconos + el boton de
    // transicion no entra en una columna angosta sin amontonarse/cortarse.
    const bool vertical = totalH > totalW * 1.15f;

    // Transiciones tiene pestaña propia (catalogo con nombre, ver
    // LayersTransitionsTab) junto a Estilos, para gestionar/crear
    // transiciones guardadas. El icono chico + popup rapido
    // (RenderTransitionRailButton) se mantiene ademas, como atajo para
    // tocar tipo/duracion de la transicion activa sin abrir la pestaña.
    static const IconRailItem kItems[] = {
        { (int)StylesSection::Backgrounds,   AppIcons::DrawIcon_Layers,    "Fondos"      },
        { (int)StylesSection::Styles,        AppIcons::DrawIcon_TextAa,    "Estilos"     },
        { (int)StylesSection::Shaders,       AppIcons::DrawIcon_Shader,    "Shaders"     },
        { (int)StylesSection::Transitions,   AppIcons::DrawIcon_Swap,      "Transiciones"},
        { (int)StylesSection::Announcements, HomeIcons::DrawIcon_Megaphone,"Anuncios"    },
        { (int)StylesSection::Capture,       HomeIcons::DrawIcon_Camera,   "Captura"     },
    };
    const auto& hubSettings  = ProyecThor::Settings::SettingsManager::Get().GetSettings().stylesHub;
    int         currentIndex = (int)m_CurrentSection;

    constexpr float kContentMarginX = 18.0f;
    constexpr float kContentMarginY = 16.0f;

    if (vertical)
    {
        const float railW = IconRailThickness(true);

        // ── Rail de iconos a la izquierda ───────────────────────────────
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::BeginChild("##stylesRail", ImVec2(railW, totalH), false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        RenderIconRail(kItems, 6, currentIndex, IconRailOrientation::Vertical, hubSettings.categoryColor);
        m_CurrentSection = (StylesSection)currentIndex;

        // Boton de transicion -- abajo del todo de la columna, no hay
        // "derecha" a la que pegarlo como en el rail horizontal.
        ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(), totalH - 34.0f));
        ImGui::SetCursorPosX(std::max(0.0f, (railW - 26.0f) * 0.5f));
        RenderTransitionRailButton();

        ImGui::EndChild();
        ImGui::SameLine();
        RenderStylesHubDivider(true, totalH);
        ImGui::SameLine();
    }
    else
    {
        const float railH = IconRailThickness(false);

        // ── Rail de iconos arriba ────────────────────────────────────────
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::BeginChild("##stylesRail", ImVec2(totalW, railH), false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        RenderIconRail(kItems, 6, currentIndex, IconRailOrientation::Horizontal, hubSettings.categoryColor);
        m_CurrentSection = (StylesSection)currentIndex;

        ImGui::SameLine(std::max(ImGui::GetCursorPosX(), totalW - 30.0f));
        RenderTransitionRailButton();

        ImGui::EndChild();
        RenderStylesHubDivider(false, totalW);
    }

    // ── Contenido de la seccion activa ───────────────────────────────────────
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kContentMarginX, kContentMarginY));
    ImGui::BeginChild("##stylesContent", ImVec2(0.f, 0.f),
                      ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();

    switch (m_CurrentSection)
    {
        case StylesSection::Backgrounds: m_Backgrounds.RenderContent(); break;
        case StylesSection::Styles:
            m_Styles.RenderContent();
            break;
        case StylesSection::Shaders:     m_Shaders.RenderContent();    break;
        case StylesSection::Transitions:
            m_TransitionsTab.RenderContent();
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
