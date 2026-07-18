#pragma once
#include "../IPanel.h"
#include "backend/core/PresentationCore.h"
#include <string>
#include <imgui.h>

namespace ProyecThor::UI {

class UIManager;

class ViewPanel : public IPanel {
public:
    explicit ViewPanel(UIManager* uiManager = nullptr) : m_UIManager(uiManager) {}
    ~ViewPanel() override = default;

    void        Render() override;
    std::string GetName() const override { return "Vista en Vivo"; }

private:
    UIManager* m_UIManager = nullptr;

    void RenderContent(float panelW, float panelH);

    // Riel vertical de acciones rápidas, a la derecha de la vista en vivo
    // (mismo lenguaje visual que ControlPanel, estilo ProPresenter: iconos
    // apilados junto al video en lugar de en el panel de Control).
    void RenderQuickActions(float railW, float railH);

    // Barra de streaming en red — extraída para no ensuciar RenderContent.
    // Recibe los límites del contenedor de video (p0/p1) y el estado ya leído.
    void RenderNetworkBar(
        const ImVec2&                   p0,
        const ImVec2&                   p1,
        const Core::PresentationState&  state,
        Core::PresentationCore&         core);
};

} // namespace ProyecThor::UI