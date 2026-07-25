#pragma once
#include "IPanel.h"
#include "frontend/views/QuickNotes.h"
#include "TeamChatPanel.h"
#include "frontend/ui/GlassRenderer.h"
#include <string>

namespace ProyecThor::UI {

class UIManager;

// Hub debajo de "Vista en Vivo": Control Overlays (transporte de macros,
// antes vivia adentro de ViewPanel) + Notas + Chat + Pads. Mismo patron de
// rail de iconos que StylesHubPanel/HomePanel/LibraryPanel.
//
// Red y Reloj se mudaron al sidebar izquierdo de Biblioteca (ver
// LibraryPanel::LibrarySideMode) — el operador las pedia "al lado de la
// biblioteca de contenido" en vez de en este hub.
//
// Pads: 8 botones tipo pad MIDI con icono elegible — ver RenderPads() y
// Settings::PadSettings.
enum class ViewToolsSection { ControlOverlays = 0, QuickNotes = 1, Chat = 2, Pads = 3 };

class ViewToolsPanel : public IPanel {
public:
    explicit ViewToolsPanel(UIManager* uiManager);
    ~ViewToolsPanel() override = default;

    void        Render()  override;
    std::string GetName() const override { return "Herramientas"; }

private:
    UIManager*        m_UIManager = nullptr;
    ViewToolsSection   m_CurrentSection = ViewToolsSection::ControlOverlays;

    QuickNotes     m_QuickNotes;
    TeamChatPanel  m_TeamChatPanel;

    // "Control Overlays" — transporte para el macro en reproduccion (ver
    // backend/core/MacroTypes.h): elegir/arrancar un macro, y en modo
    // manual avanzar/retroceder cue por cue con transicion, como pasar
    // diapositivas. Antes vivia dentro de ViewPanel; se movio aca junto con
    // Notas/Chat para que el operador tenga todo en un solo lugar debajo
    // del video, sin que ViewPanel cargue con layout que no es "el video
    // en si".
    void RenderControlOverlays(float w, float h);

    // 8 pads tipo MIDI — ver comentario de ViewToolsSection arriba.
    void RenderPads(float w, float h);
};

} // namespace ProyecThor::UI
