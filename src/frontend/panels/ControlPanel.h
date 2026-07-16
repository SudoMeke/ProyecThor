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

        // ── Estado del monitor de control (Stage / monitor de confianza) ─────
        // El "activo" fisico ahora vive en PresentationCore (SetStaging/IsStaging),
        // no en un bool local — asi este panel y el nuevo viewport StageLive de
        // UIManager.cpp siempre coinciden.
        int        m_StageMonitorIndex = 0;
        bool       m_StageUseLAN       = false;  // el monitor de control se sirve por LAN en vez de pantalla fisica
        int        m_LANPort           = 8080;
        double     m_LANLastCaptureTime = 0.0;

        void ToggleSecondaryDisplay(bool active);
        void ToggleStageDisplay(bool active);
        void CycleTargetMonitor(int direction);
        void CycleStageMonitor(int direction);
        void CaptureAndPushLANFrame(int w, int h, int quality);

        void RenderControlContent(float dt); // contenido de la seccion "Control" (antes era Render())
        void RenderMonitorInfo();
        void RenderOutputQuality();
        void RenderProjectButton(float dt);
        void RenderStageSection(float dt);
        void RenderActionRow(float dt);
    };

} // namespace ProyecThor::UI