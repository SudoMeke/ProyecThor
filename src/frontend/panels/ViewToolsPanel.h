#pragma once
#include "IPanel.h"
#include "frontend/views/OClock.h"
#include "frontend/views/QuickNotes.h"
#include "StreamingPanel.h"
#include "TeamChatPanel.h"
#include "frontend/ui/GlassRenderer.h"
#include <string>

namespace ProyecThor::UI {

class UIManager;

// Hub debajo de "Vista en Vivo": Control Overlays (transporte de macros,
// antes vivia adentro de ViewPanel) + Red/Notas/Reloj (antes vivian como
// secciones de Home). Mismo patron de rail de iconos que StylesHubPanel/
// HomePanel/LibraryPanel.
//
// Estas 4 cosas comparten un rasgo: el operador las quiere "al lado del
// video" durante un evento en vivo, no mezcladas con la biblioteca de
// contenido (que es lo unico que le queda a Home ahora).
enum class ViewToolsSection { ControlOverlays = 0, Streaming = 1, QuickNotes = 2, Clock = 3, Chat = 4 };

class ViewToolsPanel : public IPanel {
public:
    explicit ViewToolsPanel(UIManager* uiManager);
    ~ViewToolsPanel() override = default;

    void        Render()  override;
    std::string GetName() const override { return "Herramientas"; }

private:
    UIManager*        m_UIManager = nullptr;
    ViewToolsSection   m_CurrentSection = ViewToolsSection::ControlOverlays;

    OClock         m_OClock;
    QuickNotes     m_QuickNotes;
    StreamingPanel m_StreamingPanel;
    TeamChatPanel  m_TeamChatPanel;

    // "Control Overlays" — transporte para el macro en reproduccion (ver
    // backend/core/MacroTypes.h): elegir/arrancar un macro, y en modo
    // manual avanzar/retroceder cue por cue con transicion, como pasar
    // diapositivas. Antes vivia dentro de ViewPanel; se movio aca junto con
    // Red/Notas/Reloj para que el operador tenga las 4 en un solo lugar
    // debajo del video, sin que ViewPanel cargue con layout que no es
    // "el video en si".
    void RenderControlOverlays(float w, float h);
};

} // namespace ProyecThor::UI
