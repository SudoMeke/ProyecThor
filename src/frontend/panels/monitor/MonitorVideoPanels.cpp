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
// Badge chico arriba a la izquierda -- mismo look que el que dibuja
// DrawVideoFrame (MonitorUIHelpers.cpp), duplicado aca porque Imagen/Audio no
// pasan por esa funcion (no tienen textura de video que mostrar).
static void DrawPreviewBadge(ImVec2 winPos, const char* label, ImVec4 accent)
{
    ImDrawList* dl      = ImGui::GetWindowDrawList();
    ImVec2      labelSz = ImGui::CalcTextSize(label);
    float bx = winPos.x + 8.0f;
    float by = winPos.y + 8.0f;
    float bw = labelSz.x + 12.0f;
    float bh = labelSz.y + 6.0f;

    ImVec4 badgeBg = { accent.x * 0.15f, accent.y * 0.15f, accent.z * 0.15f, 0.92f };
    dl->AddRectFilled({ bx, by }, { bx + bw, by + bh },
        ImGui::ColorConvertFloat4ToU32(badgeBg), 4.0f);
    dl->AddRect({ bx, by }, { bx + bw, by + bh },
        ImGui::ColorConvertFloat4ToU32({ accent.x, accent.y, accent.z, 0.70f }), 4.0f, 0, 1.0f);
    dl->AddText({ bx + 6.0f, by + 3.0f }, ImGui::ColorConvertFloat4ToU32(accent), label);
}

void MonitorView::RenderPreviewMonitor(Core::VLCBasePlayer* player, float w, float h)
{
    auto  sel    = Core::PresentationCore::Get().PeekSelection();
    bool  hasTex = false;
    if (sel.type == Core::ItemType::Image)
        hasTex = (m_ImageView.GetTextureID() != 0);
    else if (sel.type == Core::ItemType::Audio)
        hasTex = true; // el disco animado siempre "tiene señal" visual
    else
        hasTex = (player && player->GetTextureID() != nullptr);
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

    if (sel.type == Core::ItemType::Image) {
        m_ImageView.Render(w, h);
        DrawPreviewBadge(wp, "PVW", MT::k_PrevAccent);
    } else if (sel.type == Core::ItemType::Audio) {
        UpdateSpinningDisc(m_DiscState, m_PreviewPlaying ? 1.4f : 0.0f, ImGui::GetIO().DeltaTime);
        float  radius = std::min(w, h) * 0.32f;
        ImVec2 center = { wp.x + w * 0.5f, wp.y + h * 0.5f };
        DrawSpinningDisc(dl, center, radius, m_DiscState, m_CurrentAudioArt);
        DrawPreviewBadge(wp, "PVW", MT::k_PrevAccent);
    } else {
        DrawVideoFrame(player, w, h, "SIN SEÑAL", "PVW", MT::k_PrevAccent, false);
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

} // namespace ProyecThor::UI