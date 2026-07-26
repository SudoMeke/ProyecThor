#pragma once
#include "IPanel.h"
#include "backend/core/OSCReceiver.h"
#include "StreamingPanel.h"
#include "TeamChatPanel.h"
#include <functional>
#include <string>
#include <vector>

namespace ProyecThor::UI {

// ── YggdrasilPanel ───────────────────────────────────────────────────────────
// El "arbol del mundo" que conecta ProyecThor con el resto: OSC (luces/
// controladores externos), Red (transmision LAN) y Chat, en un rail propio
// -- Red y Chat vivian antes en Biblioteca/Herramientas respectivamente, se
// mudaron aca por pedido (comparten servidor/puerto entre si, ver
// TeamChatPanel.h). Es su propio modo de workspace (ver WorkspaceMode en
// UIManager.h/RenderAll), no un panel dockeado: al activarlo reemplaza TODO
// el contenido de abajo, dibujando a pantalla completa (mismo criterio que
// Hub.cpp).
//
// OSC tiene dos direcciones independientes:
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
    enum class Section { OSC, Red, Chat };

    struct BindableParam {
        std::string                name;  // debe matchear OSCBinding::paramName
        std::function<float()>     get;
        std::function<void(float)> set;
    };

    void BuildParamRegistry();
    void ApplyReceivedMessages();

    void RenderRail();
    void RenderOSCSection();
    void RenderConnectionSection();
    void RenderControlListSection();
    void RenderSendSection();

    Section m_Section = Section::OSC;

    Core::OSCReceiver           m_Receiver;
    std::vector<BindableParam>  m_Params;
    std::vector<Core::OSCReceivedMessage> m_DrainBuffer; // reusado cada frame

    int         m_LearningIndex = -1; // indice en m_Params en modo "Aprender"
    std::string m_ListenStatus;       // texto de estado de la escucha (error o "Escuchando en puerto N")

    // Mudados desde LibraryPanel/ViewToolsPanel (ver comentario de arriba).
    StreamingPanel m_Red;
    TeamChatPanel  m_Chat;
};

} // namespace ProyecThor::UI
