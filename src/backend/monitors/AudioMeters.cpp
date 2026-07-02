#include "AudioMeters.h"
#include "MonitorDesign.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>

namespace ProyecThor::UI {

using namespace Design;

void AudioMeters::Update(Core::VLCBasePlayer* player, bool isLive, bool isPlaying, bool isMuted, float currentVolume)
{
    float curL = 0.0f;
    float curR = 0.0f;

    if (isLive && isPlaying && player && !isMuted)
    {
        // 1. Obtener la lectura real del PCM
        player->GetAudioLevels(curL, curR);

        // El currentVolume (m_LiveVolume) ya viene de 0.0f a 1.0f
float volScale = currentVolume; 
curL *= volScale;
curR *= volScale;
    }

    // Tu código de interpolación se encarga de darle la suavidad al Vúmetro
    m_VU_L = m_VU_L * 0.70f + curL * 0.30f;
    m_VU_R = m_VU_R * 0.70f + curR * 0.30f;


    if (m_VU_L > m_VU_PeakL) m_VU_PeakL = m_VU_L; else m_VU_PeakL *= 0.985f;
    if (m_VU_R > m_VU_PeakR) m_VU_PeakR = m_VU_R; else m_VU_PeakR *= 0.985f;
}

void AudioMeters::Render(float w, float h)
{
    ImDrawList* dl   = ImGui::GetWindowDrawList();
    ImVec2      pos0 = ImGui::GetCursorScreenPos();
    ImVec2      pos1 = { pos0.x + w, pos0.y + h };

    dl->AddRectFilled(pos0, pos1, IM_COL32(12, 12, 14, 255), 5.0f);
    dl->AddRect(pos0, pos1, IM_COL32(55, 55, 60, 255), 5.0f, 0, 1.5f);

    const float inset = 3.0f;
    ImVec2 inner0 = { pos0.x + inset, pos0.y + inset };
    ImVec2 inner1 = { pos1.x - inset, pos1.y - inset };
    dl->AddRectFilled(inner0, inner1, IM_COL32(4, 4, 6, 255), 3.0f);
    dl->AddRect(inner0, inner1, IM_COL32(38, 38, 44, 220), 3.0f, 0, 1.0f);

    const float workH = inner1.y - inner0.y;
    const float workW = inner1.x - inner0.x;
    const float barH  = 12.0f;
    const float gap   = 8.0f;
    const float startY = inner0.y + (workH - (barH * 2.0f + gap)) * 0.5f;

    auto DrawBar = [&](float y, const char* label, float level, float peak) {
        dl->AddText({ inner0.x + 8.0f, y - 2.0f }, IM_COL32(180, 180, 190, 255), label);

        float trackX = inner0.x + 24.0f;
        float trackW = workW - 32.0f;
        dl->AddRectFilled({ trackX, y }, { trackX + trackW, y + barH }, IM_COL32(10, 14, 10, 255), 1.0f);

        float lvlW = std::clamp(level, 0.0f, 1.0f) * trackW;
        if (lvlW > 0.0f) {
            dl->PushClipRect({ trackX, y }, { trackX + lvlW, y + barH }, true);
            float w1 = trackW * 0.65f;
            float w2 = trackW * 0.85f;
            ImU32 colGreen  = ImGui::ColorConvertFloat4ToU32(k_EQ_Green);
            ImU32 colYellow = ImGui::ColorConvertFloat4ToU32(k_EQ_Yellow);
            ImU32 colRed    = ImGui::ColorConvertFloat4ToU32(k_EQ_Red);

            dl->AddRectFilled({ trackX, y }, { trackX + w1, y + barH }, colGreen);
            dl->AddRectFilledMultiColor({ trackX + w1, y }, { trackX + w2, y + barH }, colGreen, colYellow, colYellow, colGreen);
            dl->AddRectFilledMultiColor({ trackX + w2, y }, { trackX + trackW, y + barH }, colYellow, colRed, colRed, colYellow);
            dl->PopClipRect();
        }

        for (float m = 0.1f; m < 1.0f; m += 0.05f) {
            float mx = trackX + trackW * m;
            dl->AddLine({ mx, y }, { mx, y + barH }, IM_COL32(0, 0, 0, 120), 1.0f);
        }

        if (peak > 0.01f) {
            float px = trackX + std::clamp(peak, 0.0f, 1.0f) * trackW;
            dl->AddRectFilled({ px - 1.0f, y }, { px + 1.0f, y + barH }, IM_COL32(255, 255, 255, 230));
        }
    };

    DrawBar(startY, "L", m_VU_L, m_VU_PeakL);
    DrawBar(startY + barH + gap, "R", m_VU_R, m_VU_PeakR);

    dl->AddRectFilledMultiColor(
        { inner0.x + 2.0f, inner0.y + 2.0f },
        { inner1.x - 2.0f, inner0.y + workH * 0.35f },
        IM_COL32(255, 255, 255, 8), IM_COL32(255, 255, 255, 8),
        IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));

    ImGui::Dummy({ w, h });
}

} // namespace ProyecThor::UI