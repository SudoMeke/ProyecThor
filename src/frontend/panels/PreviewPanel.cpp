#include "PreviewPanel.h"
#include "backend/core/PresentationCore.h"
#include "backend/media/VLCBasePlayer.h"
#include "frontend/ui/UIManager.h"
#include <imgui.h>
#include <algorithm>

namespace ProyecThor::UI {

PreviewPanel::PreviewPanel() {
    // 1 solo hilo de decode: el video en vivo (2 hilos, ver BackgroundLayer)
    // siempre tiene prioridad de CPU sobre el preview de biblioteca.
    m_PreviewPlayer = std::make_unique<Core::VLCBasePlayer>(1);
}

PreviewPanel::~PreviewPanel() {
    if (m_PreviewPlayer) m_PreviewPlayer->Stop();
}

void PreviewPanel::Render()
{
    if (m_PreviewPlayer)
        m_PreviewPlayer->UpdateTexture();

    Core::PresentationCore::Get().Update();

    ImGui::Begin("Preview");

    // PeekSelection() NO consume la seleccion — es correcto usarlo aqui.
    // GetSelection() la consume y limpia, lo que romperia la deteccion de tipo
    // en el siguiente frame.
    auto selection = Core::PresentationCore::Get().PeekSelection();

    if (selection.type == Core::ItemType::Audio)
    {
        // El AudioPanel vive en LibraryPanel; PreviewPanel solo tiene el puntero.
        // RenderPlayerView muestra disco, progreso, controles y ecualizador.
        if (m_AudioPanelRef)
        {
            m_AudioPanelRef->Update();
            m_AudioPanelRef->RenderPlayerView();
        }
        else
        {
            ImGui::TextDisabled("Reproductor de audio no disponible.");
        }
    }
    else if (selection.type == Core::ItemType::Bible)
    {
        m_BibleView.Render();
    }
    else if (selection.type == Core::ItemType::Song)
    {
        m_SongView.Render();
    }
    else if (selection.type == Core::ItemType::Image)
    {
        m_MediaView.Render(m_PreviewPlayer.get());
    }
    else if (selection.type == Core::ItemType::Video)
    {
        m_MonitorView.Render(m_PreviewPlayer.get());
        ImGui::Separator();
        ImGui::Spacing();
        m_MediaView.Render(m_PreviewPlayer.get());
    }
    else if (selection.type == Core::ItemType::Documents)
    {
        m_DocumentView.Render(selection.title, selection.contentData);
    }
    else
    {
        ImVec2      sz  = ImGui::GetWindowSize();
        const char* msg = "Seleccione un archivo multimedia, cancion o pasaje";
        ImVec2      ts  = ImGui::CalcTextSize(msg);
        ImGui::SetCursorPos(ImVec2((sz.x - ts.x) * 0.5f, (sz.y - ts.y) * 0.5f));
        ImGui::TextDisabled("%s", msg);
    }

    ImGui::End();

    if (m_ShowOClock)
    {
        // ── Preparar el GlassRenderer para el efecto "liquid glass" ────────
        // Mantiene el tamaño sincronizado con el display actual, y captura +
        // difumina lo que ya se dibujó este frame para usarlo como fondo
        // borroso detrás del panel de OClock.
        ImGuiIO& io   = ImGui::GetIO();
        int      dispW = static_cast<int>(io.DisplaySize.x);
        int      dispH = static_cast<int>(io.DisplaySize.y);

        if (dispW > 0 && dispH > 0)
        {
            if (!m_GlassInitialized)
            {
                m_GlassRenderer.Initialize(dispW, dispH);
                m_GlassInitialized = true;
            }
            else
            {
                m_GlassRenderer.Resize(dispW, dispH);
            }

            m_GlassRenderer.CaptureCurrentFrame();
            m_GlassRenderer.Blur();
        }

        m_OClock.Render(m_GlassRenderer);
    }
    if (m_ShowAnnouncements) m_Announcements.Render();
    if (m_ShowQuickNotes)    m_QuickNotes.Render();
}

} // namespace ProyecThor::UI