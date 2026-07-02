#pragma once
#include "../IPanel.h"
#include "backend/core/PresentationCore.h"
#include <string>
#include <imgui.h>

namespace ProyecThor::UI {

class ViewPanel : public IPanel {
public:
    ViewPanel()  = default;
    ~ViewPanel() override = default;

    void        Render() override;
    std::string GetName() const override { return "Vista en Vivo"; }

private:
    void RenderContent(float panelW, float panelH);

    // Barra de streaming en red — extraída para no ensuciar RenderContent.
    // Recibe los límites del contenedor de video (p0/p1) y el estado ya leído.
    void RenderNetworkBar(
        const ImVec2&                   p0,
        const ImVec2&                   p1,
        const Core::PresentationState&  state,
        Core::PresentationCore&         core);
};

} // namespace ProyecThor::UI