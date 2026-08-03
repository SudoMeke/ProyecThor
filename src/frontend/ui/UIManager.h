#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <optional>
#include <string>
#include <GLFW/glfw3.h>
#include "IPanel.h"
#include "../toolbar/ConfigPanel.h"
#include "settings/SettingsPanel.h"
#include "panels/capture/CapturePanel.h"
#include "panels/TransitionPanel.h"
#include "Hub.h"
#include "GlassRenderer.h"
#include "panels/PerformancePanel.h"
#include "panels/StreamingPanel.h"
#include "panels/TeamChatPanel.h"
#include "panels/BroadcastPanel.h"
#include "panels/SyncPanel.h"
#include "panels/OSCPanel.h"
#include "frontend/views/QuickNotes.h"
#include "backend/core/SubtitleImporter.h"

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
// Yggdrasil (OSC/Red/Chat/Streaming) y Biblia (BibleView a pantalla completa)
// se retiraron del todo: OSC/Red/Streaming ahora son subcategorias de
// Ajustes > Proyeccion (ver CategoryProjection.cpp), Capture/Layer/Iniciar
// tambien viven en Vista en Vivo (ver ViewPanel::RenderStreamingPopup),
// Red/Chat ya estaban duplicados en Library/Vista en Vivo, y Biblia ya se
// puede buscar desde Home -- ninguno necesitaba su propio modo de workspace.
enum class WorkspaceMode {
    Hub,
    Projector,
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

    // ── Editor a pantalla completa (Overlay/Estilos) ──────────────────────
    // Permite a un panel (editor de Overlays, editor de Estilos) tomar TODA
    // el area de "main" por un frame, ocultando Biblioteca/Home/Diseño/etc.
    // La toolbar superior (RenderModeToolbar) sigue dibujandose siempre --
    // eso es una regla aparte, no se toca aca. El llamador es dueño del
    // ciclo de vida: entra al abrir el editor, sale al Guardar/Cancelar.
    void EnterFullscreenEditor(std::function<void()> renderFn) {
        m_FullscreenEditorActive   = true;
        m_FullscreenEditorRenderFn = std::move(renderFn);
    }
    void ExitFullscreenEditor() {
        m_FullscreenEditorActive = false;
        m_FullscreenEditorRenderFn = nullptr;
    }
    bool IsFullscreenEditorActive() const { return m_FullscreenEditorActive; }

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
    SyncPanel&      GetSyncPanel()      { return m_Sync; }
    OSCPanel&       GetOSCPanel()       { return m_OSC; }

private:
    void BeginDockspace();
    void EndDockspace();

    // Ventanas nativas de salida real ("ProjectorLive"/"StageLive") -- se
    // llama SIEMPRE, una vez por frame, sin importar si el operador esta
    // viendo el Hub, el workspace normal, o un editor a pantalla completa
    // (Overlays/Estilos). Antes este render vivia inline dentro del bloque
    // exclusivo del modo Workspace::Projector, asi que dejaba de dibujarse
    // (y ImGui llegaba a destruir esas ventanas nativas por no volver a
    // someterlas) apenas se abria un editor a pantalla completa o se volvia
    // al Hub mientras se estaba proyectando/haciendo Stage — ver RenderAll().
    void RenderLiveOutputWindows();
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

    // "Importar desde URL" (Archivo > Importar) -- descarga subtitulos via
    // yt-dlp (ver SubtitleImporter.h) en un hilo de fondo, ya que la
    // descarga depende de la red y puede tardar varios segundos; congelar
    // la UI mientras tanto no es aceptable. El resultado se entrega via
    // m_UrlImportResult protegido por mutex y se consume una sola vez en
    // RenderUrlImportModal, sin importar si la ventana sigue abierta.
    void        RenderUrlImportModal();
    bool        m_ShowUrlImport        = false;
    bool        m_UrlImportRunning     = false;
    char        m_UrlImportBuffer[512] = {};
    std::string m_UrlImportLastError;
    std::thread m_UrlImportThread;
    std::mutex  m_UrlImportMutex;
    std::optional<ProyecThor::Core::SubtitleFetchResult> m_UrlImportResult;

    // Popup de acceso rapido a "Estilos" -- boton propio en RenderModeToolbar
    // (junto a Notas), lista los estilos guardados (Diseño > Estilos, ver
    // Core::PresentationCore::GetSavedStyleNames/ApplyStyleByName) para
    // aplicar uno sin salir de donde este el operador.
    void RenderStylesPopup();

    // Ver comentario de los getters (GetRedPanel/GetChatPanel/GetBroadcastPanel/GetOSCPanel).
    StreamingPanel m_Red;
    TeamChatPanel  m_Chat;
    BroadcastPanel m_Broadcast;
    SyncPanel      m_Sync;
    OSCPanel       m_OSC;
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

    // Acceso rapido "Biblioteca" desde el Hub (ver Hub::LibraryOnlyRequested):
    // sigue siendo WorkspaceMode::Projector, pero RenderAll() solo somete el
    // panel de Biblioteca (el resto de m_Panels no se renderiza ese frame) y
    // BeginDockspace() lo dockea a pantalla completa en vez del layout de
    // 4 zonas de siempre. Se resetea a false al volver al Hub o al cambiar
    // de modo a mano (rail/RenderModeToolbar, quick switcher).
    bool m_LibraryOnlyMode = false;

    // Selector rapido (Alt+Espacio) — ver RenderQuickSwitcher.
    bool m_QuickSwitchOpen  = false;
    int  m_QuickSwitchIndex = 0;

    // Ver EnterFullscreenEditor/ExitFullscreenEditor.
    bool                   m_FullscreenEditorActive = false;
    std::function<void()>  m_FullscreenEditorRenderFn;
};

} // namespace ProyecThor::UI