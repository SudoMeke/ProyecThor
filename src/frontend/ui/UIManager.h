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
#include "panels/PerformancePanel.h"
#include "panels/YggdrasilPanel.h"
#include "panels/LibraryManagerPanel.h"
#include "panels/BibleFullscreenPanel.h"

namespace ProyecThor::UI {

enum class ActiveLeftPanel {
    Library,
    Canva,
    None
};

// ── Modo de workspace ────────────────────────────────────────────────────────
// La toolbar de segundo nivel (ver RenderModeToolbar) reemplaza TODO el
// contenido de abajo segun el modo activo -- no son paneles dockeados mas,
// son secciones completas de la app:
//  - Hub: pantalla de inicio/novedades (Hub.cpp), tal cual ya existia.
//  - Projector: el workspace de siempre (Biblioteca/Home/Vista en Vivo/
//    Herramientas/Diseño dockeados), antes controlado por el bool m_HubMode.
//  - Yggdrasil: OSC, Red, Chat y Streaming (RTMP), todo en un rail propio
//    (YggdrasilPanel) -- Streaming fue su propio modo un tiempo, se
//    combino aca por pedido.
//  - Biblioteca: ver/gestionar (renombrar, borrar) Video/Imagen/Audio ya
//    importados, sin seleccionar nada para Vista en Vivo (LibraryManagerPanel).
//  - Biblia: el mismo BibleView de Home, a pantalla completa
//    (BibleFullscreenPanel).
enum class WorkspaceMode {
    Hub,
    Projector,
    Yggdrasil,
    Biblioteca,
    Biblia,
};

class UIManager {
public:
    UIManager();
    ~UIManager();
// textTransitionTrigger visto en el ultimo frame (ver RenderAll()) —
// solo texto: el fondo/video tiene su propio crossfade independiente.
uint64_t m_LastTransitionTrigger = 0;
    bool Initialize(GLFWwindow* window);
    std::shared_ptr<TransitionPanel> GetTransitionPanelOwned() const { return m_TransitionPanelOwned; }
    void AddPanel(std::shared_ptr<ProyecThor::UI::IPanel> panel);
    void RenderAll();
    void Shutdown();
    void RequestSettings();

    GlassRenderer& GetGlassRenderer() { return m_GlassRenderer; }

    // Antes enfocaba "Control" (eliminado) al resetear el layout; ahora
    // enfoca "Vista en Vivo", que es el panel principal de ese dock.
    bool m_FocusViewNextFrame = false;

    ActiveLeftPanel GetActiveLeftPanel() const { return m_ActiveLeftPanel; }
    void SetActiveLeftPanel(ActiveLeftPanel p) { m_ActiveLeftPanel = p; }

    void OpenHub();

private:
    void BeginDockspace();
    void EndDockspace();
    void ApplyProfessionalTheme();
    void RenderMainMenuBar();
    void RenderModeToolbar();
    void RenderQuickSwitcher();
 DatabasePanel m_DatabasePanel;
    WikiPanel     m_WikiPanel;
    YggdrasilPanel      m_YggdrasilPanel;
    LibraryManagerPanel m_LibraryManagerPanel;
    BibleFullscreenPanel m_BiblePanel;
    GLFWwindow*                          m_Window               = nullptr;
    std::vector<std::shared_ptr<IPanel>> m_Panels;
    bool                                 m_ShowConfig           = false;
    Settings::SettingsPanel              m_SettingsPanel;
    PerformancePanel                     m_PerformancePanel;
    ActiveLeftPanel                      m_ActiveLeftPanel      = ActiveLeftPanel::Library;
    std::shared_ptr<TransitionPanel>     m_TransitionPanelOwned;
    TransitionPanel*                     m_TransitionPanel      = nullptr;
    std::string                          m_LastProjectedText;
    std::string                          m_OutgoingText;
    float                                m_TransitionLastTime   = 0.0f;
    bool                                 m_ResetLayout          = true;
    GlassRenderer                        m_GlassRenderer;

    // ── Pantalla completa (menu Ventana) ────────────────────────────────────
    // Geometria de la ventana ANTES de pasar a pantalla completa, para poder
    // restaurarla al salir (glfwSetWindowMonitor no la recuerda solo).
    int  m_WindowedX = 0, m_WindowedY = 0, m_WindowedW = 1280, m_WindowedH = 800;
    void ToggleFullscreen();

    Hub           m_Hub;
    WorkspaceMode m_Mode = WorkspaceMode::Hub;

    // Selector rapido (Alt+Espacio) — ver RenderQuickSwitcher.
    bool m_QuickSwitchOpen  = false;
    int  m_QuickSwitchIndex = 0;
};

} // namespace ProyecThor::UI