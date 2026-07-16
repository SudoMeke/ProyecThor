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
        float      m_PulseTime       = 0.0f;

        // ── Estado del monitor de control (Stage / monitor de confianza) ─────
        bool       m_isStageActive     = false;
        int        m_StageMonitorIndex = 0;
        bool       m_StageUseLAN       = false;  // el monitor de control se sirve por LAN en vez de pantalla fisica
        int        m_LANPort           = 8080;
        double     m_LANLastCaptureTime = 0.0;

        void ToggleSecondaryDisplay(bool active);
        void ToggleStageDisplay(bool active);
        void CycleTargetMonitor(int direction);
        void CycleStageMonitor(int direction);
        void CaptureAndPushLANFrame(int w, int h, int quality);

        void RenderDivider();
        void RenderMonitorInfo();
        void RenderOutputQuality();
        void RenderProjectButton(float dt);
        void RenderStatusBar();
        void RenderStageSection(float dt);
        void RenderActionRow(float dt);
    };

} // namespace ProyecThor::UI