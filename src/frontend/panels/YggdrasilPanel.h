#pragma once
#include "IPanel.h"
#include "backend/core/OSCReceiver.h"
#include <functional>
#include <string>
#include <vector>

namespace ProyecThor::UI {

// ── YggdrasilPanel ───────────────────────────────────────────────────────────
// Control de dispositivos externos por OSC (Open Sound Control): el "arbol
// del mundo" que conecta ProyecThor con luces/consolas/controladores. Es su
// propio modo de workspace (ver WorkspaceMode en UIManager.h/RenderAll) --
// no un panel dockeado: al activarlo reemplaza TODO el contenido de abajo,
// dibujando a pantalla completa (mismo criterio que Hub.cpp).
//
// Dos direcciones independientes:
//  - Enviar: una lista de "luces" (mensajes OSC guardados: direccion +
//    argumentos) que el operador dispara a mano -- ver RenderSendSection.
//  - Recibir + OSC Learn: una "Control List" de parametros en vivo de
//    ProyecThor (por ahora, intensidad/cantidad de los efectos de Shaders)
//    que se pueden vincular a una direccion OSC entrante apretando
//    "Aprender" y moviendo el fader/control externo -- ver
//    RenderControlListSection y ApplyReceivedMessages.
class YggdrasilPanel : public IPanel {
public:
    YggdrasilPanel();
    ~YggdrasilPanel() override;

    void        Render()  override;
    std::string GetName() const override { return "Yggdrasil"; }

private:
    struct BindableParam {
        std::string                name;  // debe matchear OSCBinding::paramName
        std::function<float()>     get;
        std::function<void(float)> set;
    };

    void BuildParamRegistry();
    void ApplyReceivedMessages();

    void RenderConnectionSection();
    void RenderControlListSection();
    void RenderSendSection();

    Core::OSCReceiver           m_Receiver;
    std::vector<BindableParam>  m_Params;
    std::vector<Core::OSCReceivedMessage> m_DrainBuffer; // reusado cada frame

    int         m_LearningIndex = -1; // indice en m_Params en modo "Aprender"
    std::string m_ListenStatus;       // texto de estado de la escucha (error o "Escuchando en puerto N")
};

} // namespace ProyecThor::UI
