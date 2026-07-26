#pragma once
#include "capture/CapturePanel.h"
#include "backend/core/StreamEncoder.h"
#include <string>
#include <vector>

namespace ProyecThor::UI {

// ── BroadcastPanel ───────────────────────────────────────────────────────────
// Transmision en vivo por RTMP, estilo OBS. Vive dentro del rail de
// Yggdrasil (Capture/Layer/Iniciar, ver YggdrasilPanel.cpp) -- no es un
// IPanel/modo propio, mismo criterio que StreamingPanel ("Red") y
// TeamChatPanel ("Chat") dentro de ese mismo rail.
//  - Capture: la MISMA fuente de captura que ya usa el resto de ProyecThor
//    (CapturePanel — camara/ventana/monitor), pero el boton clave aca es
//    "Mostrar en Layer" en vez de "Enviar a Proyector".
//  - Layer: preview de lo que se va a transmitir (por ahora, la fuente de
//    Capture activa a pantalla completa -- ver nota de alcance mas abajo).
//  - Iniciar: servidor + clave de stream, bitrate/fps, y el boton de
//    arrancar/detener la transmision real (StreamEncoder, subproceso ffmpeg).
//
// Alcance actual: UNA sola fuente activa (la de Capture), sin composicion
// de multiples capas/posiciones todavia -- eso es lo que en OBS seria una
// "escena" con varios items; ProyecThor::UI::CapturePanel ya modela eso
// para el proyector via CaptureSceneSettings, pero extenderlo a multiples
// fuentes SIMULTANEAS en Streaming es un paso aparte, no incluido aca.
class BroadcastPanel {
public:
    ~BroadcastPanel();

    // Llamar UNA VEZ POR FRAME sin importar que pestaña de Yggdrasil este
    // activa: si hay una transmision en curso, sigue empujando frames
    // aunque el operador este mirando OSC/Red/Chat -- mismo criterio que
    // StreamingPanel::Update()/TeamChatPanel::Update().
    void Update();

    void RenderCaptureSection();
    void RenderLayerSection();
    void RenderStartSection();

private:
    CapturePanel m_Capture;      // instancia propia, independiente de la de Diseño/Captura
    bool         m_ShowInLayer = false;

    Core::StreamEncoder        m_Encoder;
    std::string                m_StatusMessage;
    bool                       m_StatusIsError = false;
    std::vector<unsigned char> m_ReadbackBuffer; // reusado entre frames, evita reallocs
};

} // namespace ProyecThor::UI
