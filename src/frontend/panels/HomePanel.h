#pragma once
#include <memory>
#include <string>
#include "IPanel.h"
#include "monitor/MonitorView.h"
#include "frontend/views/MediaView.h"
#include "frontend/views/BibleView.h"
#include "frontend/views/SongView.h"
#include "frontend/views/DocumentView.h"
#include "backend/media/VLCBasePlayer.h"
#include "frontend/views/Audio.h"

namespace ProyecThor::Core { class VLCBasePlayer; }

namespace ProyecThor::UI {

class UIManager;

// Home ya no es un hub multi-seccion: Reloj/Anuncios/Notas/Captura/
// Transmision se movieron a ViewToolsPanel (debajo de "Vista en Vivo") y a
// StylesHubPanel (Diseño), asi que Home queda solo con el contenido de
// biblioteca que ya tenia (ver RenderHomeContent) — sin rail de iconos ni
// pestañas propias.
class HomePanel : public IPanel {
public:
    HomePanel();
    virtual ~HomePanel();

    void        Render()  override;
    std::string GetName() const override { return "Home"; }
    void SetAudioPanel(AudioPanel* ap) { m_AudioPanelRef = ap; }

    UIManager*  m_UIManagerRef  = nullptr;

private:
    void RenderHomeContent(); // contenido de la seccion "Home" (preview en vivo)

    AudioPanel* m_AudioPanelRef = nullptr;
    MonitorView  m_MonitorView;
    MediaView    m_MediaView;
    BibleView    m_BibleView;
    SongView     m_SongView;
    DocumentView m_DocumentView;
};

} // namespace ProyecThor::UI
