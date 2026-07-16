#pragma once
#include <memory>
#include <string>
#include "IPanel.h"
#include "monitor/MonitorView.h"
#include "frontend/views/MediaView.h"
#include "frontend/views/BibleView.h"
#include "frontend/views/SongView.h"
#include "frontend/views/DocumentView.h"
#include "frontend/views/OClock.h"
#include "frontend/views/QuickNotes.h"
#include "frontend/views/Announcements.h"
#include "backend/media/VLCBasePlayer.h"
#include "frontend/views/Audio.h"
#include "frontend/ui/GlassRenderer.h" // OClock::Render() ahora necesita un GlassRenderer
#include "capture/CapturePanel.h"
#include "StreamingPanel.h"
#include "home/HomeSidebar.h"

namespace ProyecThor::Core { class VLCBasePlayer; }

namespace ProyecThor::UI {

class UIManager;

// Panel unico para el dock superior ("maintop"): reemplaza los 6 paneles que
// antes vivian ahi como pestañas nativas de ImGui (Preview, Reloj y
// Contadores, Anuncios, Notas Rapidas, Captura, Transmision en Red) por un
// sidebar de iconos igual al de Biblioteca (ver LibraryPanel/LibrarySidebar),
// donde cada seccion es mutuamente excluyente. Stage Display queda afuera,
// sigue siendo un panel independiente.
class HomePanel : public IPanel {
public:
    HomePanel();
    virtual ~HomePanel();

    Announcements m_Announcements;

    void        Render()  override;
    std::string GetName() const override { return "Home"; }
    void SetAudioPanel(AudioPanel* ap) { m_AudioPanelRef = ap; }

    UIManager*  m_UIManagerRef  = nullptr;

    // Usado por UIManager (bloque ProjectorLive) para llegar a la captura
    // en vivo sin que CapturePanel este registrado como IPanel propio.
    CapturePanel& GetCapturePanel() { return m_CapturePanel; }

private:
    void RenderHomeContent(); // contenido de la seccion "Home" (preview en vivo)

    AudioPanel* m_AudioPanelRef = nullptr;
    MonitorView  m_MonitorView;
    MediaView    m_MediaView;
    BibleView    m_BibleView;
    SongView     m_SongView;
    DocumentView m_DocumentView;
    OClock       m_OClock;
    QuickNotes   m_QuickNotes;
    CapturePanel   m_CapturePanel;
    StreamingPanel m_StreamingPanel;

    HomeSection m_CurrentSection = HomeSection::Home;

    GlassRenderer m_GlassRenderer;
    bool          m_GlassInitialized = false;
};

} // namespace ProyecThor::UI
