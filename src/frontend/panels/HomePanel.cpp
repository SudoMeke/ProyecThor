#include "HomePanel.h"
#include "UIManager.h"
#include "DesignSystem.h"
#include "frontend/ui/UIStrings.h"
#include "backend/core/PresentationCore.h"
#include "backend/media/VLCBasePlayer.h"
#include <imgui.h>
#include <algorithm>

namespace ProyecThor::UI {

namespace DS = ProyecThor::UI::DS;

HomePanel::HomePanel() {
    // Ya no se construye ningun VLCBasePlayer propio: el preview de
    // biblioteca usa el player real de PresentationCore (forceSilent=true,
    // ver PresentationCoreImpl::preview), que es estructuralmente mudo y
    // se re-silencia solo cada frame via PresentationCore::Update().
}

HomePanel::~HomePanel() {
    // Nada que detener aca: el ciclo de vida del preview real lo maneja
    // PresentationCore/PresentationCoreImpl.
}

void HomePanel::RenderHomeContent()
{
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
        ImVec2      sz  = ImGui::GetContentRegionAvail();
        const char* msg = "Seleccione un archivo multimedia, cancion o pasaje";
        ImVec2      ts  = ImGui::CalcTextSize(msg);
        ImVec2      cur = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(cur.x + (sz.x - ts.x) * 0.5f, cur.y + (sz.y - ts.y) * 0.5f));
        ImGui::TextDisabled("%s", msg);
    }
}

void HomePanel::Render()
{
    // ── Pump incondicional ──────────────────────────────────────────────────
    // Estas tres llamadas deben correr SIEMPRE, sin importar que seccion del
    // sidebar este activa: decodifican/avanzan el video en vivo del
    // proyector, sincronizan el conteo de OClock hacia LAN/pantalla, y
    // alimentan el streaming LAN. Si quedaran atadas a "esta pestaña esta
    // seleccionada", se congelarian apenas el operador mira otra seccion.
    Core::PresentationCore::Get().Update();
    m_OClock.Update();
    m_StreamingPanel.Update();

    // ── GlassRenderer para el efecto "liquid glass" de OClock/Anuncios ──────
    // Solo hace falta mientras esas dos secciones estan activas (es el fondo
    // borroso detras de ellas, puramente cosmetico).
    if (m_CurrentSection == HomeSection::Clock || m_CurrentSection == HomeSection::Announcements)
    {
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
    }

    bool visible = false;
    if (m_UIManagerRef)
    {
        visible = DS::BeginGlassPanel(GetName().c_str(), m_UIManagerRef->GetGlassRenderer(),
                                      nullptr, 0, ImVec2(0.0f, 0.0f));
    }
    else
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        visible = ImGui::Begin(GetName().c_str());
        ImGui::PopStyleVar();
    }

    if (!visible)
    {
        if (m_UIManagerRef) DS::EndGlassPanel();
        else                ImGui::End();
        return;
    }

    constexpr float k_BarH = 64.0f;
    const float     totalW = ImGui::GetContentRegionAvail().x;

    // ── Barra de iconos arriba ──────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::BeginChild("##homeTopBar", ImVec2(totalW, k_BarH), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    RenderHomeSidebar(m_CurrentSection);

    ImGui::EndChild();

    // ── Divisor horizontal con gradiente (mismo estilo que el vertical de
    //    antes, ejes intercambiados: izquierda/derecha en vez de arriba/abajo) ──
    {
        ImVec2      p  = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 colLeft  = IM_COL32(60, 80, 160,  0);
        ImU32 colMid   = IM_COL32(60, 80, 160, 80);
        ImU32 colRight = IM_COL32(60, 80, 160,  0);
        float midX     = p.x + totalW * 0.5f;
        // AddRectFilledMultiColor(p0, p1, upperLeft, upperRight, bottomRight, bottomLeft)
        dl->AddRectFilledMultiColor(
            p,               { midX, p.y + 1.f },
            colLeft, colMid, colMid, colLeft);
        dl->AddRectFilledMultiColor(
            { midX, p.y },   { p.x + totalW, p.y + 1.f },
            colMid, colRight, colRight, colMid);
    }
    ImGui::Dummy(ImVec2(totalW, 1.0f));

    // ── Contenido de la seccion activa ───────────────────────────────────────
    // Alto tomado recien aca (no precalculado) para que ya incluya cualquier
    // ItemSpacing vertical consumido por la barra/divisor de arriba.
    constexpr float kContentMarginX = 18.0f;
    constexpr float kContentMarginY = 16.0f;
    const float     contentH = ImGui::GetContentRegionAvail().y;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kContentMarginX, kContentMarginY));
    ImGui::BeginChild("##homeContent", ImVec2(0.f, contentH),
                      ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();

    switch (m_CurrentSection)
    {
        case HomeSection::Home:          RenderHomeContent();                     break;
        case HomeSection::Clock:         m_OClock.Render(m_GlassRenderer);        break;
        case HomeSection::Announcements: m_Announcements.Render(m_GlassRenderer); break;
        case HomeSection::QuickNotes:    m_QuickNotes.Render();                   break;
        case HomeSection::Capture:       m_CapturePanel.RenderContent();          break;
        case HomeSection::Streaming:     m_StreamingPanel.RenderContent();        break;
    }

    ImGui::EndChild();

    if (m_UIManagerRef) DS::EndGlassPanel();
    else                ImGui::End();
}

} // namespace ProyecThor::UI
