#pragma once
#include "IPanel.h"
#include <imgui.h>
#include <string>

namespace ProyecThor::UI {

    class UIManager;

    class ControlPanel : public IPanel {
    public:
        explicit ControlPanel(UIManager* uiManager);
        ~ControlPanel() override = default;

        void        Render()  override;
        std::string GetName() const override { return "Control"; }

    private:
        UIManager* m_UIManager       = nullptr;

        // ── Estado de proyeccion (pantalla principal / publico) ──────────────
        bool       m_isProjecting    = false;
        float      m_PulseTime       = 0.0f;

        // ── Estado del monitor de control (Stage / monitor de confianza) ─────
        bool       m_isStageActive     = false;
        int        m_StageMonitorIndex = 0;

        void ToggleSecondaryDisplay(bool active);
        void ToggleStageDisplay(bool active);
        void CycleTargetMonitor(int direction);
        void CycleStageMonitor(int direction);

        void RenderDivider();
        void RenderMonitorInfo();
        void RenderProjectButton(float dt);
        void RenderStatusBar();
        void RenderStageSection(float dt);
        void RenderActionRow(float dt);
    };

} // namespace ProyecThor::UI