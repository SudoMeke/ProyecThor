#pragma once
#include "IPanel.h"
#include "BackgroundsPanel.h"
#include "CanvasStylesPanel.h"
#include <string>

namespace ProyecThor::UI {

class UIManager;
class TransitionPanel;

// Hub de Diseño: reune Fondos + Estilos + Transiciones en un solo panel con
// rail de iconos a la izquierda (igual que Biblioteca/Home).
enum class StylesSection { Backgrounds = 0, Styles = 1, Transitions = 2 };

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

private:
    UIManager*    m_UIManager = nullptr;
    StylesSection m_CurrentSection = StylesSection::Backgrounds;

    BackgroundsPanel   m_Backgrounds;
    CanvasStylesPanel  m_Styles;
    TransitionPanel*   m_TransitionsRef = nullptr;
};

} // namespace ProyecThor::UI
