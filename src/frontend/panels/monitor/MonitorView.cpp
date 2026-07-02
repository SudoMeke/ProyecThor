#include "MonitorView.h"
#include "MonitorTheme.h"
#include "MonitorQueueIO.h"
#include "MonitorQueueHelpers.h"
#include "backend/core/PresentationCore.h"
#include "backend/media/VLCBasePlayer.h"
#include "StyleGeneralApp.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include "frontend/ui/bin/StyleGeneralApp.h"
#include "backend/core/AppPaths.h"

namespace ProyecThor::UI {

using namespace MonitorTheme;

void MonitorView::Render(Core::VLCBasePlayer* player)
{
    if (!player) {
        ImGui::PushStyleColor(ImGuiCol_Text, k_TextDim);
        ImGui::TextUnformatted("No hay reproductor disponible");
        ImGui::PopStyleColor();
        return;
    }

    Core::VLCBasePlayer* bg            = Core::PresentationCore::Get().GetBackgroundPlayer();
    bool                 isSharedPlayer = (player == bg);

    // ── Cambio de seleccion → cargar en preview ────────────────────────────
    static std::string s_LastTitle;
    auto currentSel = Core::PresentationCore::Get().PeekSelection();
    if (currentSel.title != s_LastTitle)
    {
        s_LastTitle = currentSel.title;

        if (!isSharedPlayer)
        {
           if (currentSel.type == Core::ItemType::Video && !currentSel.title.empty())
{
    std::string path = currentSel.title;
    if (path.rfind("http", 0) != 0)
        path = GetAssetsPath() + "/videos/" + path;

    player->Play(path, /*loop=*/false, /*startMuted=*/true);
    m_PreviewPlaying = true;
}
            else
            {
                player->SetPause(true);
                player->SetMute(true);
                m_PreviewPlaying = false;
            }
        }
    }

    // ── Inicializacion unica ──────────────────────────────────────────────────
    if (!m_Initialized)
    {
        m_Initialized = true;
        LoadPlayQueue();
        if (!isSharedPlayer)
        {
            player->SetPause(true);
            player->SetMute(true);
        }
    }

    // ── Layout ────────────────────────────────────────────────────────────────
    const float totalW   = ImGui::GetContentRegionAvail().x;
    const float totalH   = ImGui::GetContentRegionAvail().y;
    const float queueW   = std::min(320.0f, totalW * 0.32f);
    const float mainW    = totalW - queueW - 6.0f;
    const float centerW  = k_CenterW;
    const float sideW    = std::max(80.0f, (mainW - centerW) * 0.5f);
    const float ctrlH    = k_ControlsH;
    const float rowGap   = 6.0f;
    const float monitorH = std::max(totalH - ctrlH - rowGap * 2.0f, 60.0f);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, { 0.0f, 0.0f, 0.0f, 0.0f });
    ImGui::BeginChild("##main_col", { mainW, totalH }, false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.0f, rowGap });

    RenderPreviewMonitor(player, sideW, monitorH);
    ImGui::SameLine(0, 0);
    RenderCenterColumn(centerW, monitorH, player);
    ImGui::SameLine(0, 0);
    RenderLiveMonitor(player, sideW, monitorH);

    RenderPreviewControls(player, sideW);
    ImGui::SameLine(0, 0);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, { 0.0f, 0.0f, 0.0f, 0.0f });
    ImGui::BeginChild("##ctr_spacer", { centerW, ctrlH }, false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SameLine(0, 0);
    RenderLiveControls(player, sideW);

    ImGui::PopStyleVar();
    ImGui::EndChild();

    ImGui::SameLine(0, 6);
    RenderQueue(queueW);
}

// ---------------------------------------------------------------------------
bool MonitorView::DrawIconButton(const char* iconName, float size,
                                  ImVec4 bgCol, ImVec4 hov, ImVec4 act,
                                  ImVec2 btnSize, bool isActiveState)
{
    ImTextureID tex = (ImTextureID)0;
    auto it = StyleGeneralApp::Icons.find(iconName);
    if (it != StyleGeneralApp::Icons.end() && it->second.textureID)
        tex = (ImTextureID)(intptr_t)it->second.textureID;

    ImVec4 finalBg = isActiveState ? act : bgCol;

    ImGui::PushStyleColor(ImGuiCol_Button,        finalBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  act);

    bool pressed = ImGui::Button("", btnSize);
    bool isHeld  = ImGui::IsItemActive();

    ImVec2 p = ImGui::GetItemRectMin();
    ImVec2 s = ImGui::GetItemRectSize();

    float offsetY = isHeld ? 2.0f : 0.0f;
    ImU32 tintCol = isHeld
        ? ImGui::GetColorU32(ImVec4(0.8f, 0.8f, 0.8f, 1.0f))
        : ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

    ImGui::GetWindowDrawList()->AddImage(
        tex,
        { p.x + (s.x - size) * 0.5f, p.y + (s.y - size) * 0.5f + offsetY },
        { p.x + (s.x + size) * 0.5f, p.y + (s.y + size) * 0.5f + offsetY },
        ImVec2(0, 0), ImVec2(1, 1), tintCol);

    ImGui::PopStyleColor(3);
    return pressed;
}

} // namespace ProyecThor::UI