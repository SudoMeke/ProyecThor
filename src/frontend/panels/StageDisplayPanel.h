#pragma once
#include <string>

namespace ProyecThor::UI {

    // Configura que se muestra en el Monitor de Control (Stage): reloj,
    // texto en vivo, proxima linea, organizados en una grilla de celdas.
    // El render real ocurre en UIManager.cpp (bloque "StageLive"), este
    // panel solo edita la configuracion persistida en SettingsManager.
    // Ya no es un IPanel independiente: vive como seccion del sidebar del
    // hub de Control (ver ControlPanel.h/.cpp).
    class StageDisplayPanel {
    public:
        StageDisplayPanel()  = default;
        ~StageDisplayPanel() = default;

        void        RenderContent();
        std::string GetName() const { return "Stage Display"; }

    private:
        void RenderTemplateSelector();
        void RenderCellPreview();
        void RenderCellAssignments();
    };

} // namespace ProyecThor::UI
