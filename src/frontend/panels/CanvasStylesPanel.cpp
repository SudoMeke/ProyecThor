#include "CanvasStylesPanel.h"
#include "layers/LayersStyleTab.h"

namespace ProyecThor::UI {

CanvasStylesPanel::CanvasStylesPanel()
    : m_StyleTab(std::make_unique<LayersStyleTab>())
{}

CanvasStylesPanel::~CanvasStylesPanel() = default;

void CanvasStylesPanel::RenderContent() {
    m_StyleTab->Render();
}

} // namespace ProyecThor::UI