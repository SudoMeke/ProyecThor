#pragma once
#include "IPanel.h"
#include "StageDisplayPanel.h"
#include <imgui.h>
#include <string>

namespace ProyecThor::UI {

    class UIManager;

    // Hub de Control: reune Control (este panel) + Stage Display en un solo
    // panel con rail de iconos a la derecha (ver ControlPanel.cpp::Render()).
    enum class ControlSection { Control = 0, StageDisplay = 1 };

    class ControlPanel : public IPanel {
    public:
        explicit ControlPanel(UIManager* uiManager);
        ~ControlPanel() override = default;

        void        Render()  override;
        std::string GetName() const override { return "Control"; }

    private:
        UIManager* m_UIManager       = nullptr;

        ControlSection    m_CurrentSection = ControlSection::Control;
        StageDisplayPanel m_StageDisplay;

        // ── Estado de proyeccion (pantalla principal / publico) ──────────────
        float      m_PulseTime       = 0.0f;

        void ToggleSecondaryDisplay(bool active);
        void CycleTargetMonitor(int direction);

        void RenderControlContent(float dt); // contenido de la seccion "Control" (antes era Render())
        void RenderMonitorInfo();
        void RenderOutputQuality();
        void RenderProjectButton(float dt);
    };

} // namespace ProyecThor::UI
