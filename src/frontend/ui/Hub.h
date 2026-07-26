#pragma once

#include <chrono>
#include <array>
#include <imgui.h>

struct GLFWmonitor;

namespace ProyecThor::UI {

class Hub {
public:
    Hub();

    bool Render();
    void ForceOpen();

    bool IsOpen()               const { return m_Open; }
    bool SettingsRequested()    const { return m_OpenSettingsRequested; }
    void ClearSettingsRequest()       { m_OpenSettingsRequested = false; }
    int  GetActiveTab()         const { return m_ActiveTab; }

private:
    void RenderSidebar(float w, float h);
    void RenderMainContent(float w, float h);
    void RenderWhatsNewIfNeeded();

    void UpdateAnimations(float dt);

    bool  m_Open                  = true;
    bool  m_Appearing             = true;
    float m_AppearProgress        = 0.0f;
    float m_Time                  = 0.0f;

    bool  m_LaunchRequested       = false;
    bool  m_OpenSettingsRequested = false;

    int   m_ActiveTab             = 0;
    int   m_SelectedMonitor       = -1;

    // --- Canvas de particulas (fondo animado) ---
    struct BgParticle {
        float x, y;
        float vx, vy;
        float r;
        float phase;
        bool  isCyan;
    };

    static constexpr int   BG_PARTICLE_COUNT = 40;
    static constexpr float BG_CONNECT_DIST   = 130.0f;
    static constexpr float BG_GRID_SIZE      = 80.0f;

    std::array<BgParticle, BG_PARTICLE_COUNT> m_BgParticles;
    bool m_BgParticlesInit = false;

    void InitBgParticles(float w, float h);
    void UpdateBgParticles(float dt, float w, float h);
    void RenderBgCanvas(ImDrawList* dl, ImVec2 origin, float w, float h);

    std::chrono::steady_clock::time_point m_LastFrameTime;
};

} // namespace ProyecThor::UI