#pragma once
#include "IPanel.h"
#include "styles/CanvaStyleEditor.h"
#include <string>
#include <memory>

namespace ProyecThor::UI {

class LayersStyleTab; // Tu clase original que renderiza los estilos

class CanvasStylesPanel : public IPanel {
public:
    CanvasStylesPanel();
    ~CanvasStylesPanel() override;

    void Render() override;
    std::string GetName() const override { return "Estilos"; }

private:
    std::unique_ptr<LayersStyleTab> m_StyleTab;
};

} // namespace ProyecThor::UI