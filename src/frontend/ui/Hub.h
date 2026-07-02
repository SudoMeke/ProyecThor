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
    void RenderBackground(float w, float h);
    void RenderLeftPanel (float w, float h);
    void RenderRightPanel(float startX, float w, float h);

    void RenderTabNovedades(float contentW, float a);
    void RenderTabRecursos (float contentW, float a);
    void RenderTabAjustes  (float contentW, float a);

    bool BigButton(const char* label, const char* sublabel,
                   float w, float h, bool primary = false, bool disabled = false);

    void SectionTitle(const char* title);

    void QuickStatCard(const char* label, const char* value,
                       float w, unsigned int accentColorU32);

    void RenderSidebar(float w, float h);
    void RenderMainContent(float w, float h);

    void GlassCard(float x, float y, float w, float h,
                   unsigned int borderColor = 0,
                   float cornerRadius = 12.0f);

    void  UpdateAnimations(float dt);
    float GetAlpha() const;
    float GetSlide() const;

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

    std::array<float, 8> m_NavHover = {};

    std::chrono::steady_clock::time_point m_LastFrameTime;
};

} // namespace ProyecThor::UI