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

namespace ProyecThor::Core { class VLCBasePlayer; }

namespace ProyecThor::UI {

class UIManager;

class PreviewPanel : public IPanel {
public:
    PreviewPanel();
    virtual ~PreviewPanel();

    Announcements m_Announcements;

    void        Render()  override;
    std::string GetName() const override { return "PreviewPanel"; }
void SetAudioPanel(AudioPanel* ap) { m_AudioPanelRef = ap; }
    bool m_ShowOClock        = true;
    bool m_ShowQuickNotes    = true;
    bool m_ShowAnnouncements = true;

    UIManager*  m_UIManagerRef  = nullptr;

private:

    AudioPanel* m_AudioPanelRef = nullptr;
    MonitorView  m_MonitorView;
    MediaView    m_MediaView;
    BibleView    m_BibleView;
    SongView     m_SongView;
    DocumentView m_DocumentView;
    OClock       m_OClock;
    QuickNotes   m_QuickNotes;

    GlassRenderer m_GlassRenderer;
    bool          m_GlassInitialized = false;
};

} // namespace ProyecThor::UI