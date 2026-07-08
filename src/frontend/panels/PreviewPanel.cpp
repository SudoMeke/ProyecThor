#include "PreviewPanel.h"
#include "backend/core/PresentationCore.h"
#include "backend/media/VLCBasePlayer.h"
#include "frontend/ui/UIManager.h"
#include <imgui.h>
#include <algorithm>

namespace ProyecThor::UI {

PreviewPanel::PreviewPanel() {
    // Ya no se construye ningun VLCBasePlayer propio: el preview de
    // biblioteca usa el player real de PresentationCore (forceSilent=true,
    // ver PresentationCoreImpl::preview), que es estructuralmente mudo y
    // se re-silencia solo cada frame via PresentationCore::Update().
}

PreviewPanel::~PreviewPanel() {
    // Nada que detener aca: el ciclo de vida del preview real lo maneja
    // PresentationCore/PresentationCoreImpl.
}

void PreviewPanel::Render()
{
    // Ya no hace falta llamar UpdateTexture() manualmente: PresentationCore
    // ::Update() ya actualiza el preview real (ver BackgroundLayer::Update()
    // -> Active().UpdateTexture()).
    Core::PresentationCore::Get().Update();

    ImGui::Begin("Preview");

    auto selection = Core::PresentationCore::Get().PeekSelection();
    Core::VLCBasePlayer* previewPlayer = Core::PresentationCore::Get().GetPreviewPlayer();

    if (selection.type == Core::ItemType::Audio)
    {
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
        m_MediaView.Render(previewPlayer);
    }
    else if (selection.type == Core::ItemType::Video)
    {
        m_MonitorView.Render(previewPlayer);
        ImGui::Separator();
        ImGui::Spacing();
        m_MediaView.Render(previewPlayer);
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