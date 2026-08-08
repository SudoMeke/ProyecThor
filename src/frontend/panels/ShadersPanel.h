#pragma once
#include <string>

namespace ProyecThor::UI {

// Vive como seccion del sidebar del hub de Diseño (ver StylesHubPanel.h):
// toggles + sliders para el post-proceso de la salida en vivo — FSR
// (BackgroundLayer, solo fondo) + CRT/Grano/FXAA (composite completo de
// "ProjectorLive", ver CompositePostChain.h).
class ShadersPanel {
public:
    void RenderContent();
    std::string GetName() const { return "Shaders"; }

private:
    // Zoom de tarjetas -- mismo control que Fondos/Estilos (ver
    // UI::LPZoomSlider), tambien determina cuantas columnas entran por fila
    // segun el ancho disponible (ver RenderContent).
    float m_ThumbZoom = 1.0f;
};

} // namespace ProyecThor::UI
