#include "BackgroundsPanel.h"
#include "layers/LayersBgTab.h"
#include "layers/LayersTheme.h"
#include <imgui.h>

namespace ProyecThor::UI {

BackgroundsPanel::BackgroundsPanel()
    : m_BgTab(std::make_unique<LayersBgTab>())
{}

BackgroundsPanel::~BackgroundsPanel() = default;

void BackgroundsPanel::Render() {
    using LP = ::ProyecThor::UI::LP;

    // Estilos consistentes con tu paleta
    ImGui::PushStyleColor(ImGuiCol_WindowBg,      LP::Surface0);
    ImGui::PushStyleColor(ImGuiCol_TitleBg,       ImVec4(0.07f, 0.08f, 0.10f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.09f, 0.10f, 0.13f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));

    // Dejamos que ImGui maneje el scroll natural de la ventana
    if (ImGui::Begin(GetName().c_str(), nullptr, ImGuiWindowFlags_None)) {
        m_BgTab->Render();
    }
    ImGui::End();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
}

} // namespace ProyecThor::UI