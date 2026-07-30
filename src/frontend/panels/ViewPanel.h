#pragma once
#include "../IPanel.h"
#include "backend/core/PresentationCore.h"
#include "AudioMeters.h"
#include <string>
#include <imgui.h>

namespace ProyecThor::UI {

class UIManager;
class TeamChatPanel;

class ViewPanel : public IPanel {
public:
    explicit ViewPanel(UIManager* uiManager = nullptr) : m_UIManager(uiManager) {}
    ~ViewPanel() override = default;

    void        Render() override;
    std::string GetName() const override { return "Vista en Vivo"; }

    // Misma instancia que Yggdrasil/Herramientas (retirado) -- Chat aparece
    // en varios lugares pero es un unico servidor real. Ver cableado en
    // main.cpp.
    void SetTeamChatPanelRef(TeamChatPanel* ref) { m_TeamChatPanelRef = ref; }

private:
    UIManager*      m_UIManager        = nullptr;
    TeamChatPanel*  m_TeamChatPanelRef = nullptr;

    void RenderContent(float panelW, float panelH);

    // Riel vertical de iconos a la derecha del video ("Limpiar <tipo>",
    // contenido en vivo) + franja horizontal abajo de TODO el panel
    // (configuracion/vista: proporcion, ajustes, que fuente previsualizar,
    // mostrar tira de Stage, calidad, Chat, Pads) -- separados a proposito
    // para no mezclar "accion destructiva" con "ajuste de vista". La franja
    // de config es horizontal (no un segundo riel vertical) para no
    // restarle ancho al video en las dos puntas.
    void RenderQuickActionsClear(float railW);
    void RenderQuickActionsConfig(float stripH);

    // Popup de acceso rapido a "Calidad de salida" (mismos valores que
    // Ajustes > Proyeccion, ver ProjectionQualityPresets.h) — pensado para
    // bajar la calidad sin tener que salir de Vista en Vivo, ej. en una PC
    // de bajos recursos durante el evento.
    void RenderQualityPopup();

    // Chat y Pads -- movidos aca desde ViewToolsPanel (retirado, ver
    // UIManager.cpp), accesibles como popup desde un boton del riel
    // izquierdo en vez de ocupar su propio panel/dock permanente.
    void RenderChatPopup();
    void RenderPadsPopup();

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

    // Que fuente previsualiza el video de "Vista en Vivo" — no confundir con
    // los puntos de estado "Publico"/"Stage" (ahora en la toolbar superior,
    // ver UIManager::RenderModeToolbarStatusActions), que prenden/apagan las
    // salidas reales. Esto solo cambia que ve el OPERADOR aca, para poder
    // llevar constancia de Publico y Stage sin pararse frente al segundo
    // monitor (ver boton "vaPreviewSource" en RenderQuickActions).
    enum class PreviewSource { Publico, Stage };
    PreviewSource m_PreviewSource = PreviewSource::Publico;

    // Tira de preview de Stage, arriba del video de "Público" -- a
    // diferencia de m_PreviewSource (que REEMPLAZA que se ve en el video
    // principal), esto se ve EN SIMULTANEO con Público, para poder tener
    // ambas salidas a la vista sin pararse frente al segundo monitor.
    // Toggle en el riel izquierdo (ver "vaStageStrip" en RenderQuickActionsConfig).
    bool m_ShowStageStrip = false;

    AudioMeters m_AudioMeters;
    bool        m_LivePlaying = false;
    bool        m_LiveMuted   = false;
    float       m_LiveVolume  = 0.8f;
};

} // namespace ProyecThor::UI