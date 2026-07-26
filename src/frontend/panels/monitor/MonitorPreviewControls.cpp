#include "MonitorView.h"
#include "MonitorTheme.h"
#include "backend/core/PresentationCore.h"
#include "backend/media/VLCBasePlayer.h"
#include <imgui.h>
#include <algorithm>
#include <cstdint>

#include "MonitorDesign.h"
#include "MonitorUIHelpers.h"

namespace ProyecThor::UI {

namespace MT = MonitorTheme;
using namespace Design;
using namespace Components;

void MonitorView::RenderPreviewControls(Core::VLCBasePlayer* player, float w)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  { MT::k_PadLg, MT::k_Pad });
    ImGui::PushStyleColor(ImGuiCol_ChildBg, MT::k_Bg1);
    ImGui::PushStyleColor(ImGuiCol_Border,  MT::k_BorderSubtle);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   MT::k_R);

    ImGui::BeginChild("##ctrl_prev", { w, MT::k_ControlsH }, true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const float innerW = w - MT::k_PadLg * 2.0f;

    // ── Cabecera ──────────────────────────────────────────────────────────────
    {
        ImVec2 headerPos = ImGui::GetCursorScreenPos();
        DrawStatusDot(
            ImGui::GetWindowDrawList(),
            { headerPos.x + 7.0f, headerPos.y + 9.0f },
            4.0f, MT::k_PrevAccent, m_PreviewPlaying);

        ImGui::SetCursorPosX(MT::k_PadLg + 18.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, MT::k_PrevAccent);
        ImGui::TextUnformatted("PREVIEW");
        ImGui::PopStyleColor();

        const char* badge = "MONITOR ONLY";
        float badgeX = MT::k_PadLg + innerW - ImGui::CalcTextSize(badge).x;
        ImGui::SameLine();
        ImGui::SetCursorPosX(badgeX);
        ImGui::PushStyleColor(ImGuiCol_Text, MT::k_TextDim);
        ImGui::TextUnformatted(badge);
        ImGui::PopStyleColor();
    }

    DrawAccentLine(innerW, MT::k_PrevAccentDim, 1.0f);

    if (!player) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, MT::k_TextDim);
        ImGui::TextUnformatted("No hay reproductor asignado");
        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
        return;
    }

    Core::VLCBasePlayer* bg     = Core::PresentationCore::Get().GetBackgroundPlayer();
    bool                 shared = (player == bg) && m_LivePlaying;
    ImGui::BeginDisabled(shared);

    int64_t curMs = player->GetTime();
    int64_t lenMs = player->GetLength();
    float   pos   = (lenMs > 0)
        ? std::clamp(static_cast<float>(curMs) / static_cast<float>(lenMs), 0.0f, 1.0f)
        : 0.0f;

    ImGui::Spacing();
    ImGui::SetCursorPosX(MT::k_PadLg);

    if (BMSlider("##tl_prev", &pos, 0.0f, 1.0f, "",
                 MT::k_PrevTrack, MT::k_PrevGrab,
                 { MT::k_PrevGrab.x * 1.2f, MT::k_PrevGrab.y * 1.2f, MT::k_PrevGrab.z * 1.2f, 1.0f },
                 innerW))
    {
        player->SetPosition(pos);
        curMs = static_cast<int64_t>(pos * static_cast<float>(lenMs));
    }

    DrawTimeRow(innerW, MT::k_PadLg, curMs, lenMs);
    ImGui::Spacing();

    float gap = MT::k_Gap;
    float btnH = MT::k_TransportH;
    // 5 botones en una sola fila (INICIO | -10s | PLAY/PAUSA | +10s | STOP) —
    // el play/pausa ya no tiene fila propia: entra al centro de la barra de
    // navegacion para ahorrar el alto de una fila completa.
    float navBtnW = (innerW - (gap * 4.0f)) / 5.0f;
    float iconSize = 14.0f;

    ImGui::Spacing();

    ImGui::PushID("btn_inicio_prev");
    if (DrawIconButton("skip_prev", iconSize, MT::k_NeutBtn, MT::k_NeutBtnHov, MT::k_NeutBtnAct, {navBtnW, btnH})) {
        player->SetPosition(0.0f);
    }
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    ImGui::PushID("btn_replay_prev");
    if (DrawIconButton("replay_10", iconSize, MT::k_NeutBtn, MT::k_NeutBtnHov, MT::k_NeutBtnAct, {navBtnW, btnH})) {
        int64_t t = std::max(static_cast<int64_t>(0), curMs - 10000);
        player->SetPosition(lenMs > 0 ? static_cast<float>(t) / static_cast<float>(lenMs) : 0.0f);
    }
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    const char* iconToUse = m_PreviewPlaying ? "pause" : "play";
    ImGui::PushID("btn_main_transport_prev");
    if (DrawIconButton(iconToUse, iconSize, MT::k_PrevBtn, MT::k_PrevBtnHov, MT::k_PrevBtnAct, {navBtnW, btnH}, m_PreviewPlaying)) {
        if (m_PreviewPlaying) {
            player->SetPause(true);
            m_PreviewPlaying = false;
        } else {
            player->SetMute(true);
            player->SetVolume(0);
            player->SetPause(false);
            m_PreviewPlaying = true;
        }
    }
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    ImGui::PushID("btn_fwd_prev");
    if (DrawIconButton("forward_10", iconSize, MT::k_NeutBtn, MT::k_NeutBtnHov, MT::k_NeutBtnAct, {navBtnW, btnH})) {
        int64_t t = curMs + 10000;
        if (lenMs > 0 && t > lenMs) t = lenMs;
        player->SetPosition(lenMs > 0 ? static_cast<float>(t) / static_cast<float>(lenMs) : 0.0f);
    }
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    ImGui::PushID("btn_stop_prev");
    if (DrawIconButton("stop", iconSize, MT::k_NeutBtn, MT::k_NeutBtnHov, MT::k_NeutBtnAct, {navBtnW, btnH})) {
        player->SetPosition(0.0f);
        player->SetPause(true);
        m_PreviewPlaying = false;
    }
    ImGui::PopID();

    ImGui::EndDisabled();
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

} // namespace ProyecThor::UI