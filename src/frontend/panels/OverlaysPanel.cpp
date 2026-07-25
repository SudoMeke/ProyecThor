#include "OverlaysPanel.h"
#include "layers/LayersOverlayTab.h"

namespace ProyecThor::UI {

OverlaysPanel::OverlaysPanel()
    : m_Tab(std::make_unique<LayersOverlayTab>())
{}

OverlaysPanel::~OverlaysPanel() = default;

void OverlaysPanel::RenderContent() {
    m_Tab->Render();
}

} // namespace ProyecThor::UI
