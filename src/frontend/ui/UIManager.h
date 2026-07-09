#pragma once

#include <vector>
#include <memory>
#include <GLFW/glfw3.h>
#include "IPanel.h"
#include "../toolbar/ConfigPanel.h"
#include "settings/SettingsPanel.h"
#include "panels/capture/CapturePanel.h"
#include "panels/TransitionPanel.h"
#include "Hub.h"
#include "GlassRenderer.h"
#include "panels/DatabasePanel.h"
#include "panels/WikiPanel.h"

namespace ProyecThor::UI {

enum class ActiveLeftPanel {
    Library,
    Canva,
    None
};

class UIManager {
public:
    UIManager();
    ~UIManager();
uint64_t m_LastTransitionTrigger = 0;
float    m_LastBgColor[3]        = { 0.0f, 0.0f, 0.0f };
float    m_OutgoingBgColor[3]    = { 0.0f, 0.0f, 0.0f };
bool     m_LastBgWasVideo        = false;
bool     m_OutgoingBgWasVideo    = false;
    bool Initialize(GLFWwindow* window);
    std::shared_ptr<TransitionPanel> GetTransitionPanelOwned() const { return m_TransitionPanelOwned; }
    void AddPanel(std::shared_ptr<ProyecThor::UI::IPanel> panel);
    void RenderAll();
    void Shutdown();

    GlassRenderer& GetGlassRenderer() { return m_GlassRenderer; }

    bool m_FocusControlNextFrame = false;

    ActiveLeftPanel GetActiveLeftPanel() const { return m_ActiveLeftPanel; }
    void SetActiveLeftPanel(ActiveLeftPanel p) { m_ActiveLeftPanel = p; }

    void OpenHub();

private:
    void BeginDockspace();
    void EndDockspace();
    void ApplyProfessionalTheme();
    void RenderMainMenuBar();
 DatabasePanel m_DatabasePanel;
    WikiPanel     m_WikiPanel;
    GLFWwindow*                          m_Window               = nullptr;
    std::vector<std::shared_ptr<IPanel>> m_Panels;
    bool                                 m_ShowConfig           = false;
    Settings::SettingsPanel              m_SettingsPanel;
    ActiveLeftPanel                      m_ActiveLeftPanel      = ActiveLeftPanel::Library;
    std::shared_ptr<TransitionPanel>     m_TransitionPanelOwned;
    TransitionPanel*                     m_TransitionPanel      = nullptr;
    std::string                          m_LastProjectedText;
    std::string                          m_OutgoingText;
    float                                m_TransitionLastTime   = 0.0f;
    bool                                 m_ResetLayout          = true;
    GlassRenderer                        m_GlassRenderer;

    Hub   m_Hub;
    bool  m_HubMode = true;
};

} // namespace ProyecThor::UI