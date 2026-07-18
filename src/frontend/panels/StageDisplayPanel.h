#pragma once
#include <string>

namespace ProyecThor::UI {

    // Configura y controla el Monitor de Control (Stage): activarlo/apagarlo,
    // elegir a que pantalla fisica (o LAN) se sirve, y que se muestra en el
    // (reloj, texto en vivo, proxima linea, organizados en una grilla de
    // celdas). El render real de la grilla ocurre en UIManager.cpp (bloque
    // "StageLive"); este panel solo edita la configuracion persistida en
    // SettingsManager y controla el arranque/parada real via PresentationCore.
    // Ya no es un IPanel independiente: vive como seccion del sidebar del
    // hub de Control (ver ControlPanel.h/.cpp).
    class StageDisplayPanel {
    public:
        StageDisplayPanel()  = default;
        ~StageDisplayPanel() = default;

        void        RenderContent();
        std::string GetName() const { return "Stage Display"; }

    private:
        void RenderActivationCard();
        void RenderTemplateSelector();
        void RenderCellPreview();
        void RenderCellAssignments();

        void ToggleStageDisplay(bool active);
        void CycleStageMonitor(int direction);
        void CaptureAndPushLANFrame(int w, int h, int quality);

        // ── Estado del monitor de control (Stage / monitor de confianza) ─────
        // El "activo" fisico vive en PresentationCore (SetStaging/IsStaging),
        // no en un bool local — asi este panel y el viewport StageLive de
        // UIManager.cpp siempre coinciden.
        int        m_StageMonitorIndex   = 0;
        bool       m_StageUseLAN         = false; // se sirve por LAN en vez de pantalla fisica
        int        m_LANPort             = 8080;
        double     m_LANLastCaptureTime  = 0.0;
    };

} // namespace ProyecThor::UI
