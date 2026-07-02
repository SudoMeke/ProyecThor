#include "MonitorView.h"
#include "MonitorTheme.h"
#include "backend/core/PresentationCore.h"
#include "backend/media/VLCBasePlayer.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>

#include "MonitorDesign.h"
#include "MonitorUIHelpers.h"

// =============================================================================
//  MonitorVideoPanels.cpp
//  RenderPreviewMonitor y RenderLiveMonitor.
// =============================================================================

namespace ProyecThor::UI {

// Alias corto para evitar colision con constantes de MonitorDesign.h
// que tambien viven en ProyecThor::UI
namespace MT = MonitorTheme;
using namespace Design;
using namespace Components;

// =============================================================================
//  Monitor de Preview (PVW)
// =============================================================================
void MonitorView::RenderPreviewMonitor(Core::VLCBasePlayer* player, float w, float h)
{
    void* texID       = player ? player->GetTextureID() : nullptr;
    bool  hasTex      = (texID != nullptr);
    float borderAlpha = hasTex ? 0.55f : 0.18f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, MT::k_Bg3);
    ImGui::PushStyleColor(ImGuiCol_Border,
        ImVec4(MT::k_PrevAccent.x, MT::k_PrevAccent.y, MT::k_PrevAccent.z, borderAlpha));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, hasTex ? 1.5f : 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   MT::k_RLg);

    ImGui::BeginChild("##mon_prev", { w, h }, true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2      wp = ImGui::GetWindowPos();
    dl->AddRectFilledMultiColor(
        wp,
        { wp.x + w, wp.y + h },
        IM_COL32(0, 0, 0, 110), IM_COL32(0, 0, 0, 0),
        IM_COL32(0, 0, 0, 0),   IM_COL32(0, 0, 0, 110));

    DrawVideoFrame(player, w, h, "SIN SEÑAL", "PVW", MT::k_PrevAccent, false);

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

// =============================================================================
//  Monitor Live (PGM)
// =============================================================================
void MonitorView::RenderLiveMonitor(Core::VLCBasePlayer* /*unused*/, float w, float h)
{
    Core::VLCBasePlayer* bg    = Core::PresentationCore::Get().GetBackgroundPlayer();
    void*                texID = bg ? bg->GetTextureID() : nullptr;
    bool                 live  = (texID != nullptr) && m_LivePlaying;

    float borderAlpha = live ? 0.75f : 0.18f;
    float borderSize  = live ? 2.0f  : 1.0f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, MT::k_Bg3);
    ImGui::PushStyleColor(ImGuiCol_Border,
        ImVec4(MT::k_LiveAccent.x, MT::k_LiveAccent.y, MT::k_LiveAccent.z, borderAlpha));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, borderSize);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   MT::k_RLg);

    ImGui::BeginChild("##mon_live", { w, h }, true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2      wp = ImGui::GetWindowPos();
    dl->AddRectFilledMultiColor(
        wp,
        { wp.x + w, wp.y + h },
        IM_COL32(0, 0, 0, 110), IM_COL32(0, 0, 0, 0),
        IM_COL32(0, 0, 0, 0),   IM_COL32(0, 0, 0, 110));

    if (live) {
        dl->AddRectFilledMultiColor(
            wp,
            { wp.x + w, wp.y + 4.0f },
            ImGui::ColorConvertFloat4ToU32(ImVec4(MT::k_LiveAccent.x, MT::k_LiveAccent.y, MT::k_LiveAccent.z, 0.40f)),
            ImGui::ColorConvertFloat4ToU32(ImVec4(MT::k_LiveAccent.x, MT::k_LiveAccent.y, MT::k_LiveAccent.z, 0.40f)),
            IM_COL32(0, 0, 0, 0),
            IM_COL32(0, 0, 0, 0));
    }

    DrawVideoFrame(bg, w, h, "SIN SEÑAL", "PGM", MT::k_LiveAccent, live);

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

} // namespace ProyecThor::UI