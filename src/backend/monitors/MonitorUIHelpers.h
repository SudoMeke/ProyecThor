#pragma once
#include <imgui.h>
#include <string>
#include <cstdint>
#include "backend/media/VLCBasePlayer.h"

namespace ProyecThor::UI::Components {

    struct TransportConfig {
        float  availW, transportH, gap;
        ImVec4 btnBase, btnHov, btnAct;
        ImVec4 playBase, playHov, playAct, playText, accentText;
        bool   showHome, isPlaying;
        const char *playId, *pauseId, *homeId, *skipBkId, *skipFwId, *stopId;
    };

    struct VolumeConfig {
        float  availW, volumeH, pad;
        const char* muteLabel;
        float* volume;
        bool*  muted;
        ImVec4 sliderBg, grab, grabAct;
        ImVec4 btnBase, btnHov, btnAct;
        const char* sliderId;
    };

    std::string FormatTime(int64_t ms);
    void DrawStatusDot(ImDrawList* dl, ImVec2 center, float r, ImVec4 col, bool active);
    void DrawAccentLine(float width, ImVec4 color, float thickness = 1.5f);
    bool BMButton(const char* label, ImVec2 size, ImVec4 base, ImVec4 hov, ImVec4 act, ImVec4 textCol = { -1, -1, -1, -1 }, float rounding = 6.0f);
    bool BMSlider(const char* id, float* val, float lo, float hi, const char* fmt, ImVec4 frameBg, ImVec4 grab, ImVec4 grabAct, float width = -1.0f);
    void DrawTimeRow(float innerW, float padLeft, int64_t curMs, int64_t lenMs);
    void DrawVideoFrame(Core::VLCBasePlayer* player, float w, float h, const char* placeholder, const char* badgeLabel, ImVec4 badgeAccent, bool pulseBorder = false);
    int  RenderTransportRow(const TransportConfig& cfg);
    bool RenderVolumeRow(const VolumeConfig& cfg);

} // namespace ProyecThor::UI::Components