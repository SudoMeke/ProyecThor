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
//  Columna central con botones TRANSMITIR y LOOP. (Iconos Puros)
//
//  CONTENER/ESTIRAR se quito de aca -- ya vive a la derecha de ViewPanel,
//  no hace falta duplicarlo en el Monitor.
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

    // Altos "ideales" -- si el alto disponible (h) no alcanza, se escalan
    // proporcionalmente hacia abajo (con un piso minimo) en vez de
    // desbordar/cortarse contra el borde del child (columna resizable).
    const float baseMainH   = 50.0f;
    const float baseLoopH   = 32.0f;
    const float baseSpacing = 8.0f;
    const float baseTotalH  = baseMainH + baseSpacing + baseLoopH;
    const float scale       = std::clamp(h / baseTotalH, 0.55f, 1.0f);

    const float mainH   = baseMainH * scale;
    const float loopH   = baseLoopH * scale;
    const float spacing = baseSpacing * scale;
    const float totalH  = mainH + spacing + loopH;
    float       startY  = std::max(0.0f, (h - totalH) * 0.5f);

    ImGui::SetCursorPosY(startY);

    // ── TRANSMITIR ────────────────────────────────────────────────────────────
    ImGui::SetCursorPosX(hPad);
    ImGui::PushID("btn_transmit");
    if (DrawIconButton("arrow_forward", mainH * 0.56f, MT::k_LiveBtn, MT::k_LiveBtnHov, MT::k_LiveBtnAct, { btnW, mainH }))
    {
        auto sel = Core::PresentationCore::Get().PeekSelection();
        if (!sel.title.empty())
        {
            // FIX: sin Stop() previo (corte a negro) — SetVideo() ya
            // maneja carga en frio o crossfade, y el guard de reentrancia
            // en Play() necesita que no se le limpie la ruta actual en
            // cada click repetido de "TRANSMITIR".
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

            Core::PresentationCore::Get().SetBackgroundMedia(finalPath, true, /*allowAudio=*/true);
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
    // El estado vive en PresentationCore (GetLiveLoop/SetLiveLoop) en vez de
    // un bool local: el enforcement (auto-restart al llegar al final) ahora
    // corre en ViewPanel::RenderLiveTransport, junto al resto de los
    // controles del player "general" — este boton y ese enforcement
    // necesitan ver el mismo flag aunque vivan en clases distintas.
    bool loopEnabled = Core::PresentationCore::Get().GetLiveLoop();
    ImGui::SetCursorPosX(hPad);
    ImVec4 loopBase = loopEnabled ? MT::k_AmberBtn    : MT::k_NeutBtn;
    ImVec4 loopHov  = loopEnabled ? MT::k_AmberBtnHov : MT::k_NeutBtnHov;
    ImVec4 loopAct  = loopEnabled ? MT::k_AmberBtnAct : MT::k_NeutBtnAct;

    ImGui::PushID("btn_loop");
    if (DrawIconButton("repeat", loopH * 0.56f, loopBase, loopHov, loopAct, { btnW, loopH }, loopEnabled))
    {
        Core::PresentationCore::Get().SetLiveLoop(!loopEnabled);
    }
    ImGui::PopID();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace ProyecThor::UI