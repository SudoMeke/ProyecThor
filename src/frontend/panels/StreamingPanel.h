#pragma once
#include "../IPanel.h"
#include "backend/core/NetworkStreamServer.h"
#include <string>
#include <vector>
#include <mutex>
#include <imgui.h>

namespace ProyecThor::UI {

class StreamingPanel : public IPanel {
public:
    StreamingPanel()  = default;
    ~StreamingPanel() override = default;

    void        Render() override;
    std::string GetName() const override { return "Transmisión en Red"; }

private:
    void RenderServerControl();
    void RenderLayerSelector();
    void RenderQualitySelector();
    void RenderURLSection();

    // Captura el framebuffer OpenGL, comprime a JPEG y llama core.PushFrame()
    void CaptureAndPushFrame(int w, int h, int quality);

    // Genera y cachea la textura OpenGL del QR cuando la URL cambia
    void RebuildQRTexture(const std::string& url);
    void DrawQR(ImDrawList* dl, ImVec2 origin, float size);

    int  m_Port        = 8080;

    // Config local — se sincroniza con el servidor al cambiar
    Core::StreamConfig m_Config;
    bool               m_ConfigDirty = false;

    // QR
    std::string           m_QRCachedURL;
    // Módulos del QR: 0 = claro, 1 = oscuro. Tamaño = m_QRSize * m_QRSize
    std::vector<uint8_t>  m_QRModules;
    int                   m_QRSize = 0;
};

} // namespace ProyecThor::UI