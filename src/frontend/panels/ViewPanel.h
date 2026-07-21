#pragma once
#include "../IPanel.h"
#include "backend/core/PresentationCore.h"
#include "AudioMeters.h"
#include <string>
#include <imgui.h>

namespace ProyecThor::UI {

class UIManager;

class ViewPanel : public IPanel {
public:
    explicit ViewPanel(UIManager* uiManager = nullptr) : m_UIManager(uiManager) {}
    ~ViewPanel() override = default;

    void        Render() override;
    std::string GetName() const override { return "Vista en Vivo"; }

private:
    UIManager* m_UIManager = nullptr;

    void RenderContent(float panelW, float panelH);

    // Riel vertical de acciones rápidas, a la derecha de la vista en vivo
    // (mismo lenguaje visual que ControlPanel, estilo ProPresenter: iconos
    // apilados junto al video en lugar de en el panel de Control).
    void RenderQuickActions(float railW, float railH);

    // Barra de streaming en red — extraída para no ensuciar RenderContent.
    // Recibe los límites del contenedor de video (p0/p1) y el estado ya leído.
    void RenderNetworkBar(
        const ImVec2&                   p0,
        const ImVec2&                   p1,
        const Core::PresentationState&  state,
        Core::PresentationCore&         core);

    // Transporte + VU meters del player "general" (bg, el que va a
    // público) — antes vivian en Monitor (MonitorLiveControls, ver
    // RenderLiveMonitor/RenderLiveControls, ya eliminados de ahi). Se
    // movieron aca porque Monitor se quedaba sin aire en pantallas chicas,
    // y este panel ya es "donde el usuario ve lo que transmite".
    void RenderLiveTransport(float w, float h);

    // Dos puntos de estado arriba del video — reemplazan al boton "Iniciar
    // proyección" de ControlPanel (eliminado) y al "ACTIVAR STAGE" de
    // StageDisplayPanel (ahora en Ajustes). "Público" prende/corta la
    // salida real al publico; "Stage" el monitor de confianza del equipo.
    // Ambos usan la pantalla ya configurada en Ajustes (o la secundaria por
    // defecto) — cargar contenido (fondo/cancion/video) nunca los prende
    // solo; el operador decide con estos puntos cuando algo sale al aire.
    void RenderStatusDots(float w);
    void ToggleAudience(bool active);
    void ToggleStageQuick(bool active);

    AudioMeters m_AudioMeters;
    bool        m_LivePlaying = false;
    bool        m_LiveMuted   = false;
    float       m_LiveVolume  = 0.8f;
};

} // namespace ProyecThor::UI