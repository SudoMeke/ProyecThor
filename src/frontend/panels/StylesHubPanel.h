#pragma once
#include "IPanel.h"
#include "BackgroundsPanel.h"
#include "CanvasStylesPanel.h"
#include "OverlaysPanel.h"
#include "frontend/views/Announcements.h"
#include "capture/CapturePanel.h"
#include <string>

namespace ProyecThor::UI {

class UIManager;
class TransitionPanel;

// Hub de Diseño: reune Fondos + Estilos + Overlays + Transiciones + Anuncios
// + Captura en un solo panel con rail de iconos a la izquierda (igual que
// Biblioteca/Home). Anuncios y Captura se movieron aca desde Home, junto
// con el resto de las herramientas de "preparar/vestir" la salida en vivo.
enum class StylesSection {
    Backgrounds = 0, Styles = 1, Overlays = 2, Transitions = 3,
    Announcements = 4, Capture = 5,
};

class StylesHubPanel : public IPanel {
public:
    explicit StylesHubPanel(UIManager* uiManager);
    ~StylesHubPanel() override = default;

    void        Render()  override;
    std::string GetName() const override { return "Diseño"; }

    // TransitionPanel sigue siendo dueño de UIManager (su Update/Trigger ya
    // corre incondicionalmente cada frame, independiente de este hub) — acá
    // solo se recibe un puntero para dibujar su UI de configuracion.
    void SetTransitionPanel(TransitionPanel* tp) { m_TransitionsRef = tp; }

    // Usado por UIManager (bloque ProjectorLive) para llegar a Anuncios/
    // Captura en vivo sin que sean IPanel propios (mismo patron que HomePanel
    // usaba antes de que se mudaran aca).
    Announcements& GetAnnouncements() { return m_Announcements; }
    CapturePanel&  GetCapturePanel()  { return m_Capture; }

private:
    // Barra compacta de transicion (duracion + Sin transicion/Disolver/
    // Avanzado) dibujada arriba del contenido de Estilos — ver Render().
    // "Avanzado" abre el panel completo (m_TransitionsRef->RenderContent())
    // como popup, para los tipos complejos (Zoom, Slide, Cover, Uncover).
    void RenderTransitionQuickBar();

    UIManager*    m_UIManager = nullptr;
    StylesSection m_CurrentSection = StylesSection::Backgrounds;

    BackgroundsPanel   m_Backgrounds;
    CanvasStylesPanel  m_Styles;
    OverlaysPanel      m_Overlays;
    TransitionPanel*   m_TransitionsRef = nullptr;
    Announcements      m_Announcements;
    CapturePanel       m_Capture;
};

} // namespace ProyecThor::UI
