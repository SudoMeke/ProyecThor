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
        bool       m_isProjecting    = false;

        float      m_PulseTime       = 0.0f;
        float      m_PressAnim       = 0.0f;
        bool       m_WasPressed      = false;

        float      m_HoverClearText  = 0.0f;
        float      m_HoverStopVideo  = 0.0f;
        float      m_HoverMonitor    = 0.0f;

        void ToggleSecondaryDisplay(bool active);
void RenderDivider();
        void RenderProjectButton(float dt);
        void RenderActionRow(float dt);
        void RenderStatusBar();
        void RenderMonitorInfo();

        bool RenderIconButton(const char* id,
                              float cx, float cy, float radius,
                              float& hoverAnim,
                              ImVec4 colorBase, ImVec4 colorHover,
                              float dt);
    };

} // namespace ProyecThor::UI