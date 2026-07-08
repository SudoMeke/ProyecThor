#include "MonitorView.h"
#include "MonitorTheme.h"
#include "backend/core/PresentationCore.h"
#include "backend/media/VLCBasePlayer.h"
#include "backend/core/AppPaths.h"
#include <imgui.h>
#include <algorithm>

#include "MonitorDesign.h"
#include "MonitorUIHelpers.h"

// =============================================================================
//  MonitorCenterColumn.cpp
//  Columna central con botones TRANSMITIR, LOOP y CONTENER/ESTIRAR. (Iconos Puros)
// =============================================================================

namespace ProyecThor::UI {

namespace MT = MonitorTheme;
using namespace Design;
using namespace Components;

void MonitorView::RenderCenterColumn(float w, float h, Core::VLCBasePlayer* previewPlayer)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, { 0.0f, 0.0f, 0.0f, 0.0f });
    ImGui::BeginChild("##center_col", { w, h }, false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const float hPad    = 8.0f;
    const float btnW    = w - hPad * 2.0f;
    const float mainH   = 50.0f;
    const float loopH   = 32.0f;
    const float strH    = 44.0f;
    const float spacing = 8.0f;
    const float totalH  = mainH + spacing + loopH + spacing + strH;
    float       startY  = std::max(0.0f, (h - totalH) * 0.5f);

    ImGui::SetCursorPosY(startY);

    // ── TRANSMITIR ────────────────────────────────────────────────────────────
    ImGui::SetCursorPosX(hPad);
    ImGui::PushID("btn_transmit");
    if (DrawIconButton("arrow_forward", 28.0f, MT::k_LiveBtn, MT::k_LiveBtnHov, MT::k_LiveBtnAct, { btnW, mainH }))
    {
        auto sel = Core::PresentationCore::Get().PeekSelection();
        if (!sel.title.empty())
        {
            Core::PresentationCore::Get().StopBackgroundMedia();
            std::string finalPath = sel.title;
            if (finalPath.rfind("http", 0) != 0)
                finalPath = VideosPath() + finalPath;

            // El target de mute/volumen se fija ANTES de proyectar. Asi
            // BackgroundLayer lo guarda como estado propio y lo respeta en
            // cualquier swap futuro (doble buffer, siguiente clip de cola),
            // en vez de perderse si se tocara el player directo.
            Core::PresentationCore::Get().SetLiveMute(m_LiveMuted);
            Core::PresentationCore::Get().SetLiveVolume(
                m_LiveMuted ? 0 : static_cast<int>(m_LiveVolume * 100.0f));

            Core::PresentationCore::Get().SetBackgroundMedia(finalPath, true);
            Core::PresentationCore::Get().SetProjecting(true);
            m_LivePlaying = true;

            Core::VLCBasePlayer* newBg = Core::PresentationCore::Get().GetBackgroundPlayer();
            if (previewPlayer && previewPlayer != newBg) {
                previewPlayer->SetPause(true);
                m_PreviewPlaying = false;
            }
            if (newBg)
                newBg->SetPause(false);
        }
    }
    ImGui::PopID();

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + spacing);

    // ── LOOP ──────────────────────────────────────────────────────────────────
    ImGui::SetCursorPosX(hPad);
    ImVec4 loopBase = m_LoopEnabled ? MT::k_AmberBtn    : MT::k_NeutBtn;
    ImVec4 loopHov  = m_LoopEnabled ? MT::k_AmberBtnHov : MT::k_NeutBtnHov;
    ImVec4 loopAct  = m_LoopEnabled ? MT::k_AmberBtnAct : MT::k_NeutBtnAct;

    ImGui::PushID("btn_loop");
    // Pasamos m_LoopEnabled para mantener el estado "activo" visualmente hundido si es necesario
    if (DrawIconButton("repeat", 18.0f, loopBase, loopHov, loopAct, { btnW, loopH }, m_LoopEnabled))
    {
        m_LoopEnabled = !m_LoopEnabled;
    }
    ImGui::PopID();

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + spacing);

    // ── CONTENER / ESTIRAR ────────────────────────────────────────────────────
    ImGui::SetCursorPosX(hPad);
    bool   isStretch = Core::PresentationCore::Get().GetStretchToFill();
    ImVec4 sBase     = isStretch ? MT::k_PrevBtn    : MT::k_NeutBtn;
    ImVec4 sHov      = isStretch ? MT::k_PrevBtnHov : MT::k_NeutBtnHov;
    ImVec4 sAct      = isStretch ? MT::k_PrevBtnAct : MT::k_NeutBtnAct;

    // Cambiamos dinámicamente el icono dependiendo del estado
    const char* stretchIcon = isStretch ? "original_screen" : "fit_screen";

    ImGui::PushID("btn_stretch");
    if (DrawIconButton(stretchIcon, 24.0f, sBase, sHov, sAct, { btnW, strH }, isStretch))
    {
        Core::PresentationCore::Get().SetStretchToFill(!isStretch);
    }
    ImGui::PopID();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace ProyecThor::UI