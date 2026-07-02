#pragma once
#include "backend/media/VLCBasePlayer.h"

namespace ProyecThor::UI {

class AudioMeters {
public:
    AudioMeters() = default;

    void Update(Core::VLCBasePlayer* player, bool isLive, bool isPlaying, bool isMuted, float currentVolume);
    void Render(float w, float h);

private:
    float m_VU_L     = 0.0f;
    float m_VU_R     = 0.0f;
    float m_VU_PeakL = 0.0f;
    float m_VU_PeakR = 0.0f;
};

}