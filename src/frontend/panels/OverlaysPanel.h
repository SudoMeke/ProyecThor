#pragma once
#include <string>
#include <memory>

namespace ProyecThor::UI {

class LayersOverlayTab;

// Ya no es un IPanel independiente: vive como seccion del sidebar del hub de
// Diseño (ver StylesHubPanel.h/.cpp), igual que BackgroundsPanel/CanvasStylesPanel.
class OverlaysPanel {
public:
    OverlaysPanel();
    ~OverlaysPanel();

    void RenderContent();
    std::string GetName() const { return "Overlays"; }

private:
    std::unique_ptr<LayersOverlayTab> m_Tab;
};

} // namespace ProyecThor::UI
