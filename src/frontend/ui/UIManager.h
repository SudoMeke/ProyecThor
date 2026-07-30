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
#include "panels/BibleFullscreenPanel.h"
#include "panels/StreamingPanel.h"
#include "panels/TeamChatPanel.h"
#include "panels/BroadcastPanel.h"
#include "frontend/views/QuickNotes.h"

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
//  - Biblia: el mismo BibleView de Home, a pantalla completa
//    (BibleFullscreenPanel).
enum class WorkspaceMode {
    Hub,
    Projector,
    Yggdrasil,
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

    // Red (LAN)/Chat/Streaming viven aca (no en Yggdrasil ni en Biblioteca/
    // Herramientas) para que Update() corra SIEMPRE, sin importar el
    // WorkspaceMode activo -- una transmision o el chat no se pueden pausar
    // solo porque el operador esta mirando Proyector. Yggdrasil,
    // LibraryPanel (grupo "Red") y ViewPanel (popup "Chat", ver
    // RenderChatPopup) reciben un puntero a la MISMA instancia (ver
    // main.cpp), asi que aparecen "en varias partes" pero comparten un
    // unico servidor de verdad.
    StreamingPanel& GetRedPanel()      { return m_Red; }
    TeamChatPanel&  GetChatPanel()     { return m_Chat; }
    BroadcastPanel& GetBroadcastPanel() { return m_Broadcast; }

private:
    void BeginDockspace();
    void EndDockspace();
    void ApplyProfessionalTheme();
    void RenderMainMenuBar();
    void RenderModeToolbar();
    void RenderQuickSwitcher();

    // Puntos "Publico"/"Stage" + "Borrar Todo" — antes vivian en ViewPanel
    // (arriba del video), pedido explicito de subirlos a la toolbar
    // superior (lado derecho) para liberarle mas espacio a "Vista en Vivo".
    void RenderModeToolbarStatusActions(float winW, float railH);
    void ToggleAudience(bool active);
    void ToggleStageQuick(bool active);

    // Ventana flotante de Notas -- boton propio en RenderModeToolbar (junto
    // a los 5 modos), abre una ventana centrada tipo "Preferencias" (ver
    // Settings::SettingsPanel::Render) con QuickNotes adentro, en vez de
    // vivir dockeada en Home o en un panel propio.
    void         RenderNotesWindow();
    bool         m_ShowNotes = false;
    QuickNotes   m_NotesPanel;

 DatabasePanel m_DatabasePanel;
    WikiPanel     m_WikiPanel;
    YggdrasilPanel      m_YggdrasilPanel;
    BibleFullscreenPanel m_BiblePanel;

    // Ver comentario de los getters (GetRedPanel/GetChatPanel/GetBroadcastPanel).
    StreamingPanel m_Red;
    TeamChatPanel  m_Chat;
    BroadcastPanel m_Broadcast;
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