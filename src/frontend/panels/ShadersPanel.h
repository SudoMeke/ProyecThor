#pragma once
#include <string>

namespace ProyecThor::UI {

// Vive como seccion del sidebar del hub de Diseño (ver StylesHubPanel.h),
// igual patron que OverlaysPanel: toggles + sliders para el post-proceso de
// la salida en vivo — FSR (BackgroundLayer, solo fondo) + CRT/Grano/FXAA
// (composite completo de "ProjectorLive", ver CompositePostChain.h).
class ShadersPanel {
public:
    void RenderContent();
    std::string GetName() const { return "Shaders"; }
};

} // namespace ProyecThor::UI
