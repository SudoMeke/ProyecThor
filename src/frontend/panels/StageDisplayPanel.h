#pragma once
#include "IPanel.h"
#include <string>

namespace ProyecThor::UI {

    // Configura que se muestra en el Monitor de Control (Stage): reloj,
    // texto en vivo, proxima linea, organizados en una grilla de celdas.
    // El render real ocurre en UIManager.cpp (bloque "StageLive"), este
    // panel solo edita la configuracion persistida en SettingsManager.
    class StageDisplayPanel : public IPanel {
    public:
        StageDisplayPanel()  = default;
        ~StageDisplayPanel() override = default;

        void        Render()  override;
        std::string GetName() const override { return "Stage Display"; }

    private:
        void RenderTemplateSelector();
        void RenderCellPreview();
        void RenderCellAssignments();
    };

} // namespace ProyecThor::UI
