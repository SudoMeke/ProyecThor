#pragma once
#include "../IPanel.h"
#include "backend/core/PresentationCore.h"
#include "AudioMeters.h"
#include <string>
#include <unordered_map>
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

    // Misma instancia que Herramientas (retirado) -- Chat aparece en varios
    // lugares pero es un unico servidor real. Ver cableado en main.cpp.
    void SetTeamChatPanelRef(TeamChatPanel* ref) { m_TeamChatPanelRef = ref; }

private:
    UIManager*      m_UIManager        = nullptr;
    TeamChatPanel*  m_TeamChatPanelRef = nullptr;

    void RenderContent(float panelW, float panelH);

    // Riel vertical de iconos a la derecha del video ("Limpiar <tipo>",
    // contenido en vivo) + franja horizontal debajo del transporte
    // (configuracion/vista: proporcion, ajustes, que fuente previsualizar,
    // Overlays, Chat, Pads) -- separados a proposito para no mezclar "accion
    // destructiva" con "ajuste de vista".
    void RenderQuickActionsClear(float railW);
    void RenderQuickActionsConfig(float stripH);

    // Herramienta inline activa (ver RenderInlineTool) -- en vez de abrir un
    // popup flotante separado, Overlays/Chat/Pads se muestran EN EL MISMO
    // panel, ocupando el espacio libre entre el transporte y la franja de
    // config de abajo (pedido explicito: "que muestren el contenido abajo,
    // no como panel aparte sino como si fuera parte del mismo panel").
    // Click de nuevo en el mismo boton = cerrar (volver a None).
    enum class InlineTool { None, Overlays, Chat, Pads, Clock };
    InlineTool m_ActiveTool = InlineTool::None;

    // Alto minimo que se le reserva siempre al transporte (progreso + pads +
    // fader) aunque haya una herramienta inline abierta -- ver RenderContent
    // de cada seccion en Render().
    static constexpr float kLiveTransportMinH = 120.0f;

    void RenderInlineTool(float w, float h);
    void RenderOverlaysContent();
    void RenderChatContent();
    void RenderPadsContent();
    void RenderClockContent();
    std::unordered_map<std::string, ImTextureID> m_OverlayThumbCache;

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

    AudioMeters m_AudioMeters;
    bool        m_LivePlaying = false;
    bool        m_LiveMuted   = false;
    float       m_LiveVolume  = 0.8f;
};

} // namespace ProyecThor::UI