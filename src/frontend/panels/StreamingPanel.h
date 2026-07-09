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

    void CaptureAndPushFrame(int w, int h, int quality);

    void RebuildQRTexture(const std::string& url);
    void DrawQR(ImDrawList* dl, ImVec2 origin, float size);

    int  m_Port        = 8080;

    // Config local — se sincroniza con el servidor al cambiar
    Core::StreamConfig m_Config;
    bool               m_ConfigDirty = false;

    // Throttle de captura: ImGui::GetTime() de la ultima vez que se
    // capturo/comprimio un frame. Usado en Render() para no capturar mas
    // rapido de lo que cada modo de transmision realmente necesita.
    double m_LastCaptureTime = 0.0;

    // QR
    std::string           m_QRCachedURL;
    std::vector<uint8_t>  m_QRModules;
    int                   m_QRSize = 0;
};

} // namespace ProyecThor::UI