#include <GL/glew.h>
#include "UIManager.h"
#include "backend/core/PresentationCore.h"
#include "frontend/ui/TextEffectsRenderer.h"
#include "backend/core/PerformanceGovernor.h"
#include "../toolbar/ConfigPanel.h"
#include "panels/HomePanel.h"
#include "panels/StylesHubPanel.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <imgui_internal.h>
#include "../external/tools/OpenURL.h"
#include "UIStrings.h"
#include "frontend/views/Announcements.h"
#include "frontend/views/OClock.h"
#include "frontend/views/Audio.h"
#include "Hub.h"
#include "frontend/panels/StreamingPanel.h"
#include "qrcodegen.hpp"
#include "backend/settings/SettingsManager.h"
#include "backend/settings/ProjectionQualityPresets.h"
#include "AppIcons.h"
#include "DesignSystem.h"
#include "IconRail.h"
#include "MonitorTheme.h"
#include "MonitorUIHelpers.h"
#include "frontend/ui/bin/StyleGeneralApp.h"
#include "frontend/panels/home/HomeIcons.h"
#include "biblio/LibraryIcons.h"
#include "LiveContentRenderer.h"
#include "LibrarySongs.h"
#include <ctime>

namespace ProyecThor::UI {

namespace MT = MonitorTheme;

static bool g_ShowAbout = false;

static bool StatusDotToggle(ImDrawList* dl, const char* id, const char* label, bool on, ImVec4 onColor, float rowH)
{
    const float dotR = 5.0f;
    ImVec2 textSz = ImGui::CalcTextSize(label);
    float itemW = dotR * 2.0f + 6.0f + textSz.x + 14.0f;

    ImVec2 p0 = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(id, ImVec2(itemW, rowH));
    bool hovered = ImGui::IsItemHovered();

    ImVec2 center = { p0.x + dotR + 4.0f, p0.y + rowH * 0.5f };
    ImVec4 offColor = { 0.42f, 0.44f, 0.50f, 1.0f };
    Components::DrawStatusDot(dl, center, dotR, on ? onColor : offColor, on);

    ImVec4 textCol = on ? onColor : ImVec4(0.75f, 0.76f, 0.80f, hovered ? 1.0f : 0.85f);
    dl->AddText({ center.x + dotR + 6.0f, p0.y + (rowH - textSz.y) * 0.5f },
               ImGui::ColorConvertFloat4ToU32(textCol), label);

    return clicked;
}

UIManager::UIManager() : m_Window(nullptr), m_ShowConfig(false) {}
UIManager::~UIManager() { Shutdown(); }

bool UIManager::Initialize(GLFWwindow* window)
{
    m_Window = window;
    if (!m_Window) return false;

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    m_TransitionPanelOwned = std::make_shared<TransitionPanel>();
    m_TransitionPanel      = m_TransitionPanelOwned.get();

    m_YggdrasilPanel.SetRedPanel(&m_Red);
    m_YggdrasilPanel.SetChatPanel(&m_Chat);
    m_YggdrasilPanel.SetBroadcastPanel(&m_Broadcast);

    ApplyProfessionalTheme();
    m_SettingsPanel.InitializeTheme();
int fbWidth, fbHeight;
glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
m_GlassRenderer.Initialize(fbWidth, fbHeight);
    return true;
}

void UIManager::ApplyProfessionalTheme()
{
    ImGuiStyle& s = ImGui::GetStyle();

    s.WindowPadding          = ImVec2(22.0f, 18.0f);
    s.FramePadding           = ImVec2(14.0f,  9.0f);
    s.ItemSpacing            = ImVec2(12.0f,  8.0f);
    s.ItemInnerSpacing       = ImVec2( 8.0f,  6.0f);
    s.CellPadding            = ImVec2(10.0f,  7.0f);
    s.TouchExtraPadding      = ImVec2( 0.0f,  0.0f);
    s.IndentSpacing          = 18.0f;
    s.ScrollbarSize          =  6.0f;
    s.GrabMinSize            =  8.0f;

    s.WindowRounding         =  8.0f;
    s.ChildRounding          =  6.0f;
    s.FrameRounding          =  6.0f;
    s.PopupRounding          =  6.0f;
    s.ScrollbarRounding      = 10.0f;
    s.GrabRounding           =  6.0f;
    s.TabRounding            =  6.0f;
    s.WindowMenuButtonPosition = ImGuiDir_None;

    s.WindowBorderSize       = 1.0f;
    s.ChildBorderSize        = 1.0f;
    s.PopupBorderSize        = 1.0f;
    s.FrameBorderSize        = 0.0f;
    s.TabBorderSize          = 0.0f;
    s.TabBarBorderSize       = 0.0f;

    ImVec4* c = s.Colors;

    c[ImGuiCol_WindowBg]             = ImVec4(0.036f, 0.040f, 0.060f, 1.000f);
    c[ImGuiCol_ChildBg]              = ImVec4(0.030f, 0.034f, 0.052f, 0.650f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.040f, 0.044f, 0.066f, 0.985f);
    c[ImGuiCol_Border]               = ImVec4(1.000f, 1.000f, 1.000f, 0.075f);
    c[ImGuiCol_BorderShadow]         = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);

    c[ImGuiCol_FrameBg]              = ImVec4(1.000f, 1.000f, 1.000f, 0.042f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(1.000f, 1.000f, 1.000f, 0.075f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(1.000f, 1.000f, 1.000f, 0.108f);

    c[ImGuiCol_TitleBg]              = ImVec4(0.025f, 0.028f, 0.044f, 1.000f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.032f, 0.036f, 0.056f, 1.000f);
    c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.025f, 0.028f, 0.044f, 0.800f);
    c[ImGuiCol_MenuBarBg]            = ImVec4(0.022f, 0.025f, 0.040f, 1.000f);

    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(1.000f, 1.000f, 1.000f, 0.110f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1.000f, 1.000f, 1.000f, 0.185f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.550f, 0.560f, 0.580f, 0.880f);

    c[ImGuiCol_CheckMark]            = ImVec4(0.700f, 0.700f, 0.720f, 1.000f);
    c[ImGuiCol_SliderGrab]           = ImVec4(0.700f, 0.700f, 0.720f, 1.000f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.720f, 0.730f, 0.750f, 1.000f);

    c[ImGuiCol_Button]               = ImVec4(1.000f, 1.000f, 1.000f, 0.048f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(1.000f, 1.000f, 1.000f, 0.088f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.550f, 0.560f, 0.580f, 1.000f);

    c[ImGuiCol_Header]               = ImVec4(0.550f, 0.560f, 0.580f, 0.148f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.550f, 0.560f, 0.580f, 0.215f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.550f, 0.560f, 0.580f, 0.375f);

    c[ImGuiCol_Separator]            = ImVec4(1.000f, 1.000f, 1.000f, 0.055f);
    c[ImGuiCol_SeparatorHovered]     = ImVec4(0.550f, 0.560f, 0.580f, 0.380f);
    c[ImGuiCol_SeparatorActive]      = ImVec4(0.550f, 0.560f, 0.580f, 0.780f);

    c[ImGuiCol_ResizeGrip]           = ImVec4(0.550f, 0.560f, 0.580f, 0.095f);
    c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.550f, 0.560f, 0.580f, 0.360f);
    c[ImGuiCol_ResizeGripActive]     = ImVec4(0.550f, 0.560f, 0.580f, 0.780f);

    c[ImGuiCol_Tab]                  = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
    c[ImGuiCol_TabHovered]           = ImVec4(1.000f, 1.000f, 1.000f, 0.068f);
    c[ImGuiCol_TabActive]            = ImVec4(1.000f, 1.000f, 1.000f, 0.108f);
    c[ImGuiCol_TabUnfocused]         = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
    c[ImGuiCol_TabUnfocusedActive]   = ImVec4(1.000f, 1.000f, 1.000f, 0.058f);

    c[ImGuiCol_DockingPreview]       = ImVec4(0.550f, 0.560f, 0.580f, 0.268f);
    c[ImGuiCol_DockingEmptyBg]       = ImVec4(0.026f, 0.029f, 0.044f, 1.000f);

    c[ImGuiCol_PlotLines]            = ImVec4(0.550f, 0.560f, 0.580f, 1.000f);
    c[ImGuiCol_PlotLinesHovered]     = ImVec4(0.720f, 0.730f, 0.750f, 1.000f);
    c[ImGuiCol_PlotHistogram]        = ImVec4(0.550f, 0.560f, 0.580f, 1.000f);
    c[ImGuiCol_PlotHistogramHovered] = ImVec4(0.720f, 0.730f, 0.750f, 1.000f);

    c[ImGuiCol_TableHeaderBg]        = ImVec4(1.000f, 1.000f, 1.000f, 0.038f);
    c[ImGuiCol_TableBorderStrong]    = ImVec4(1.000f, 1.000f, 1.000f, 0.095f);
    c[ImGuiCol_TableBorderLight]     = ImVec4(1.000f, 1.000f, 1.000f, 0.038f);
    c[ImGuiCol_TableRowBg]           = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
    c[ImGuiCol_TableRowBgAlt]        = ImVec4(1.000f, 1.000f, 1.000f, 0.022f);

    c[ImGuiCol_TextSelectedBg]       = ImVec4(0.550f, 0.560f, 0.580f, 0.215f);
    c[ImGuiCol_DragDropTarget]       = ImVec4(0.550f, 0.560f, 0.580f, 0.780f);
    c[ImGuiCol_NavHighlight]         = ImVec4(0.550f, 0.560f, 0.580f, 1.000f);
    c[ImGuiCol_NavWindowingHighlight]= ImVec4(1.000f, 1.000f, 1.000f, 0.580f);
    c[ImGuiCol_NavWindowingDimBg]    = ImVec4(0.000f, 0.000f, 0.000f, 0.440f);
    c[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.000f, 0.000f, 0.000f, 0.540f);

    c[ImGuiCol_Text]                 = ImVec4(0.921f, 0.929f, 0.960f, 1.000f);
    c[ImGuiCol_TextDisabled]         = ImVec4(1.000f, 1.000f, 1.000f, 0.265f);

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        s.WindowRounding       = 0.0f;
        c[ImGuiCol_WindowBg].w = 1.0f;
    }
}

void UIManager::AddPanel(std::shared_ptr<IPanel> panel)
{
    if (!panel) return;
    m_Panels.push_back(std::move(panel));
}

void UIManager::OpenHub()
{
    m_Hub.ForceOpen();
    m_Mode = WorkspaceMode::Hub;
}

void UIManager::RequestSettings()
{
    m_ShowConfig = true;
}

void UIManager::RenderAll()
{

     {
        ImGuiIO& io = ImGui::GetIO();

        if (ImGui::IsKeyPressed(ImGuiKey_F1, false))
            ProyecThor::External::OpenURL("https://proyecthor.web.app/");

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_P, false))
            m_ShowConfig = true;

        if (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_F4, false))
            glfwSetWindowShouldClose(m_Window, true);

        if (ImGui::IsKeyPressed(ImGuiKey_F11, false))
            ToggleFullscreen();
    }

    m_Red.Update();
    m_Chat.Update();
    m_Broadcast.Update();

    RenderModeToolbar();

    if (m_Mode == WorkspaceMode::Yggdrasil)
    {
        m_YggdrasilPanel.Render();
        RenderMainMenuBar();
        return;
    }

    if (m_Mode == WorkspaceMode::Biblia)
    {
        m_BiblePanel.Render();
        RenderMainMenuBar();
        return;
    }

if (m_Mode == WorkspaceMode::Hub)
    {
        if (m_Hub.Render())
        {
            if (!m_Hub.SettingsRequested())
            {
                m_Mode        = WorkspaceMode::Projector;
                m_ResetLayout = true;
            }
        }

        if (m_Hub.SettingsRequested())
        {
            m_ShowConfig = true;
            m_SettingsPanel.SetInitialCategory(m_Hub.GetActiveTab());
            m_Hub.ClearSettingsRequest();
        }

        if (m_ShowConfig)
            m_SettingsPanel.Render(&m_ShowConfig);

        {
            auto& general = ProyecThor::Settings::SettingsManager::Get().GetSettings().general;
            if (general.showPerfPanel)
            {
                bool wasOpen = general.showPerfPanel;
                m_PerformancePanel.Render(&general.showPerfPanel);
                if (wasOpen && !general.showPerfPanel)
                    ProyecThor::Settings::SettingsManager::Get().Save();
            }
        }

        m_DatabasePanel.Render();
        m_WikiPanel.Render();

        RenderMainMenuBar();
        return;
    }

    BeginDockspace();

    const auto& str = ProyecThor::UI::GetUIStrings();

    float transNow = (float)glfwGetTime();
    float transDt  = transNow - m_TransitionLastTime;
    m_TransitionLastTime = transNow;
    transDt = std::min(transDt, 0.1f);

    if (m_TransitionPanel)
        m_TransitionPanel->Update(transDt);

    for (auto& panel : m_Panels)
        panel->Render();
if (m_FocusViewNextFrame) {
        ImGui::SetWindowFocus("Vista en Vivo");
        m_FocusViewNextFrame = false;
    }

    auto state = Core::PresentationCore::Get().GetState();
    if (state.isProjecting)
    {
        int monitorCount = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

        if (monitors && monitorCount > 0 &&
            state.targetMonitorIndex >= 0 &&
            state.targetMonitorIndex < monitorCount)
        {
            const GLFWvidmode* mode = glfwGetVideoMode(monitors[state.targetMonitorIndex]);

            if (mode && mode->width > 0 && mode->height > 0)
            {
                int mx, my;
                glfwGetMonitorPos(monitors[state.targetMonitorIndex], &mx, &my);

if (m_TransitionPanel) {
    Core::PresentationCore::Get().SetTransitionConfig(
        static_cast<int>(m_TransitionPanel->GetCurrentType()),
        m_TransitionPanel->GetDuration());
}

if (m_TransitionPanel && state.textTransitionTrigger != m_LastTransitionTrigger)
{
    m_LastTransitionTrigger = state.textTransitionTrigger;

    std::string outgoing = m_LastProjectedText;
    m_LastProjectedText   = state.currentText;

    if (!outgoing.empty() && !state.currentText.empty())
    {
        m_OutgoingText = outgoing;
        m_TransitionPanel->Trigger();
    }
}

                ImGui::SetNextWindowPos(ImVec2((float)mx, (float)my));
                ImGui::SetNextWindowSize(ImVec2((float)mode->width, (float)mode->height));

                ImGuiWindowFlags flags =
                    ImGuiWindowFlags_NoDecoration          |
                    ImGuiWindowFlags_NoBackground          |
                    ImGuiWindowFlags_NoSavedSettings       |
                    ImGuiWindowFlags_NoFocusOnAppearing    |
                    ImGuiWindowFlags_NoNav                 |
                    ImGuiWindowFlags_NoBringToFrontOnFocus;

                ImGuiWindowClass projectorClass;
projectorClass.ViewportFlagsOverrideSet =
    ImGuiViewportFlags_NoAutoMerge | ImGuiViewportFlags_TopMost;
ImGui::SetNextWindowClass(&projectorClass);

ImGui::Begin("ProjectorLive", nullptr, flags);

                Core::PresentationCore::Get().SetProjectorPostFXViewportID(
                    ImGui::GetWindowViewport()->ID);
                ImDrawList* drawList = ImGui::GetWindowDrawList();

bool showingLoadingScreen = Core::PresentationCore::Get().ShouldShowLoadingScreen();
if (showingLoadingScreen)
{
    drawList->AddRectFilled(
        ImVec2((float)mx, (float)my),
        ImVec2((float)(mx + mode->width), (float)(my + mode->height)),
        IM_COL32(0, 0, 0, 255));

    void* logoTex = Core::PresentationCore::Get().GetLoadingLogoTexture();
    int   logoW   = Core::PresentationCore::Get().GetLoadingLogoWidth();
    int   logoH   = Core::PresentationCore::Get().GetLoadingLogoHeight();
    if (logoTex && logoW > 0 && logoH > 0)
    {
        float destX = (float)mx, destY = (float)my;
        float destW = (float)mode->width, destH = (float)mode->height;
        float logoRatio   = (float)logoW / (float)logoH;
        float screenRatio = destW / destH;

        if (logoRatio > screenRatio + 0.001f) {
            destH = destW / logoRatio;
            destY = (float)my + ((float)mode->height - destH) * 0.5f;
        } else if (logoRatio < screenRatio - 0.001f) {
            destW = destH * logoRatio;
            destX = (float)mx + ((float)mode->width - destW) * 0.5f;
        }

        drawList->AddImage(logoTex, ImVec2(destX, destY), ImVec2(destX + destW, destY + destH),
                           ImVec2(0, 0), ImVec2(1, 1));
    }
}
else
{

if (state.bgType == Core::PresentationState::BackgroundType::SolidColor)
{
    ImU32 finalCol = IM_COL32(
        (int)(state.bgColor[0]*255), (int)(state.bgColor[1]*255),
        (int)(state.bgColor[2]*255), 255);

    drawList->AddRectFilled(
        ImVec2((float)mx, (float)my),
        ImVec2((float)(mx + mode->width), (float)(my + mode->height)),
        finalCol);
}

                if (state.bgType == Core::PresentationState::BackgroundType::Video)
                {
                    drawList->AddRectFilled(
                        ImVec2((float)mx, (float)my),
                        ImVec2((float)(mx + mode->width), (float)(my + mode->height)),
                        IM_COL32(0, 0, 0, 255));

                    int srcW = 0, srcH = 0;
                    auto* player = Core::PresentationCore::Get().GetBackgroundPlayer();
                    if (player) player->GetVideoSize(srcW, srcH);

                    bool stretch = Core::PresentationCore::Get().GetStretchToFill();

                    float destX = (float)mx;
                    float destY = (float)my;
                    float destW = (float)mode->width;
                    float destH = (float)mode->height;

                    if (!stretch && srcW > 0 && srcH > 0)
                    {
                        float videoRatio  = (float)srcW / (float)srcH;
                        float screenRatio = destW / destH;

                        if (videoRatio > screenRatio + 0.001f) {
                            destH = destW / videoRatio;
                            destY = (float)my + ((float)mode->width / screenRatio - destH) * 0.5f;
                        } else if (videoRatio < screenRatio - 0.001f) {
                            destW = destH * videoRatio;
                            destX = (float)mx + ((float)mode->height * screenRatio - destW) * 0.5f;
                        }
                    }

                    const auto& projSettings = ProyecThor::Settings::SettingsManager::Get().GetSettings().projection;
                    auto qualityMode = static_cast<ProyecThor::Settings::OutputQualityMode>(projSettings.outputQualityMode);
                    int qualityW = 0, qualityH = 0;
                    ProyecThor::Settings::ResolveQualityTarget(
                        qualityMode,
                        projSettings.outputPresetIndex, projSettings.outputWidth, projSettings.outputHeight,
                        mode->width, mode->height, qualityW, qualityH);

                    if (qualityMode == ProyecThor::Settings::OutputQualityMode::Auto)
                        Core::PerformanceGovernor::Get().ApplyCap(qualityW, qualityH);

                    void* texID = Core::PresentationCore::Get().GetProcessedBackgroundTexture(
                        qualityW, qualityH);

                    if (texID) {

                        bool hasBars = (destW < (float)mode->width - 0.5f) ||
                                       (destH < (float)mode->height - 0.5f);
                        if (hasBars && Core::PresentationCore::Get().GetFillBlurEnabled()) {
                            void* fillTex = Core::PresentationCore::Get().GetBackgroundFillTexture(
                                qualityW, qualityH);
                            if (fillTex) {

                                float b = std::clamp(
                                    Core::PresentationCore::Get().GetFillBlurBrightness(), 0.0f, 1.0f);
                                ImU32 fillTint = IM_COL32((int)(b * 255.0f), (int)(b * 255.0f), (int)(b * 255.0f), 255);
                                drawList->AddImage(fillTex,
                                    ImVec2((float)mx, (float)my),
                                    ImVec2((float)(mx + mode->width), (float)(my + mode->height)),
                                    ImVec2(0, 0), ImVec2(1, 1), fillTint);
                            }
                        }

                        drawList->AddImage(texID,
                            ImVec2(destX, destY),
                            ImVec2(destX + destW, destY + destH),
                            ImVec2(0, 0), ImVec2(1, 1));
                    } else {
                        drawList->AddRectFilled(
                            ImVec2((float)mx, (float)my),
                            ImVec2((float)(mx + mode->width), (float)(my + mode->height)),
                            IM_COL32(0, 0, 0, 255));
                    }

                    auto& core = Core::PresentationCore::Get();
                    if (core.IsBackgroundSwapPending() && core.IsBackgroundStandbyReady())
                    {
                        void* standbyTex = core.GetStandbyBackgroundTexture();
                        if (standbyTex)
                        {
                            float progress = std::clamp(core.GetBackgroundBlendProgress(), 0.0f, 1.0f);
                            ImU32 tint = IM_COL32(255, 255, 255, (int)(progress * 255.0f));
                            drawList->AddImage(standbyTex,
                                ImVec2(destX, destY),
                                ImVec2(destX + destW, destY + destH),
                                ImVec2(0, 0), ImVec2(1, 1), tint);
                        }
                    }
                }

                if (state.bgType == Core::PresentationState::BackgroundType::Audio)
                {
                    drawList->AddRectFilled(
                        ImVec2((float)mx, (float)my),
                        ImVec2((float)(mx + mode->width), (float)(my + mode->height)),
                        IM_COL32(0, 0, 0, 255));

                    if (auto* audioPanel = Core::PresentationCore::Get().GetAudioPanelRef())
                        audioPanel->RenderLiveBackground((float)mx, (float)my,
                                                          (float)mode->width, (float)mode->height);
                }

                if (state.showText)
                {
                    auto DrawTextBlock = [&](const std::string& text,
                                            float offsetX, float offsetY,
                                            float alphaMult = 1.0f, float scaleMult = 1.0f)
                    {
                        if (text.empty() || alphaMult <= 0.001f) return;

                        float screenScale = (float)mode->width / 1920.0f;
                        float marginL = state.margins[0] * screenScale;
                        float marginT = state.margins[1] * screenScale;
                        float marginR = state.margins[2] * screenScale;
                        float marginB = state.margins[3] * screenScale;

                        float boxW = std::max(10.0f, (float)mode->width  - marginL - marginR);
                        float boxH = std::max(10.0f, (float)mode->height - marginT - marginB);

                        float shiftX = offsetX * (float)mode->width;
                        float shiftY = offsetY * (float)mode->height;
                        float boxX   = (float)mx + marginL + shiftX;
                        float boxY   = (float)my + marginT + shiftY;

                        float targetFontSize = state.textSize * screenScale;

                        std::string activeFontName =
                            Core::PresentationCore::Get().GetActiveFontName();
                        ImFont* activeFont =
                            Core::PresentationCore::Get().GetImGuiFont(activeFontName, targetFontSize);
                        if (!activeFont) activeFont = ImGui::GetFont();

                        if (state.autoScale) {
                            while (targetFontSize > 10.0f) {
                                ImVec2 tSize = activeFont->CalcTextSizeA(
                                    targetFontSize, FLT_MAX, boxW, text.c_str());
                                if (tSize.y <= boxH) break;
                                targetFontSize -= 1.0f;
                            }
                        }

                        targetFontSize *= std::max(0.01f, scaleMult);

                        ImVec2 finalBlockSize = activeFont->CalcTextSizeA(
                            targetFontSize, FLT_MAX, boxW, text.c_str());

                        ImU32 col = ImGui::ColorConvertFloat4ToU32(
                            ImVec4(state.textColor[0], state.textColor[1],
                                   state.textColor[2], state.textColor[3] * alphaMult));

                        bool isSong = (Core::PresentationCore::Get().PeekSelection().type
                                       == Core::ItemType::Song);

                        drawList->PushClipRect(
                            ImVec2((float)mx, (float)my),
                            ImVec2((float)(mx + mode->width), (float)(my + mode->height)),
                            true);

                        if (isSong && state.textAlignment == 1)
                        {
                            float startY = boxY;
                            if (state.vAlignment == 1)
                                startY += (boxH - finalBlockSize.y) * 0.5f;
                            else if (state.vAlignment == 2)
                                startY += (boxH - finalBlockSize.y);

                            float  currentY   = startY;
                            size_t startPos   = 0;
                            size_t endPos     = text.find('\n');
                            float  lineHeight = activeFont->CalcTextSizeA(
                                targetFontSize, FLT_MAX, boxW, "A").y;

                            while (startPos != std::string::npos)
                            {
                                std::string line = text.substr(startPos, endPos - startPos);
                                if (!line.empty() && line.back() == '\r') line.pop_back();

                                if (!line.empty()) {
                                    ImVec2 lineSize = activeFont->CalcTextSizeA(
                                        targetFontSize, FLT_MAX, boxW, line.c_str());
                                    float lineX = boxX + (boxW - lineSize.x) * 0.5f;

                                    DrawStyledText(drawList, activeFont, targetFontSize,
                                        ImVec2(lineX, currentY), col, line.c_str(),
                                        0.0f, screenScale, state.effects, alphaMult);
                                }

                                currentY += lineHeight;
                                if (endPos == std::string::npos) break;
                                startPos = endPos + 1;
                                endPos   = text.find('\n', startPos);
                            }
                        }
                        else
                        {
                            float textX = boxX;
                            if (state.textAlignment == 1)
                                textX += (boxW - finalBlockSize.x) * 0.5f;
                            else if (state.textAlignment == 2)
                                textX += (boxW - finalBlockSize.x);

                            float textY = boxY;
                            if (state.vAlignment == 1)
                                textY += (boxH - finalBlockSize.y) * 0.5f;
                            else if (state.vAlignment == 2)
                                textY += (boxH - finalBlockSize.y);

                            DrawStyledText(drawList, activeFont, targetFontSize,
                                ImVec2(textX, textY), col, text.c_str(),
                                boxW, screenScale, state.effects, alphaMult);
                        }

                        drawList->PopClipRect();
                    };

                    bool transActive = m_TransitionPanel && m_TransitionPanel->IsActive();

                    if (transActive) {
                        DrawTextBlock(m_OutgoingText,
                            m_TransitionPanel->GetOutgoingOffsetX(),
                            m_TransitionPanel->GetOutgoingOffsetY(),
                            m_TransitionPanel->GetOutgoingAlpha(),
                            m_TransitionPanel->GetOutgoingScale());

                        DrawTextBlock(state.currentText,
                            m_TransitionPanel->GetIncomingOffsetX(),
                            m_TransitionPanel->GetIncomingOffsetY(),
                            m_TransitionPanel->GetIncomingAlpha(),
                            m_TransitionPanel->GetIncomingScale());
                    } else if (!state.currentText.empty()) {
                        DrawTextBlock(state.currentText, 0.0f, 0.0f, 1.0f, 1.0f);
                    }
                }
}

                if (!showingLoadingScreen)
                {
                    static auto s_AnnLastTime = std::chrono::steady_clock::now();
                    auto        annNow        = std::chrono::steady_clock::now();
                    float       annDt = std::chrono::duration<float>(annNow - s_AnnLastTime).count();
                    s_AnnLastTime = annNow;
                    annDt = std::min(annDt, 0.1f);

                    for (auto& p : m_Panels) {
                        if (p->GetName() == "Diseño") {

                            auto* stylesHub =
                                static_cast<ProyecThor::UI::StylesHubPanel*>(p.get());

                            if (stylesHub->GetAnnouncements().IsLive()) {
                                stylesHub->GetAnnouncements().RenderOnProjector(
                                    drawList,
                                    (float)mx, (float)my,
                                    (float)mode->width, (float)mode->height,
                                    annDt);
                            }

                            stylesHub->GetCapturePanel().RenderOnProjector(
                                drawList,
                                (float)mx, (float)my,
                                (float)mode->width, (float)mode->height);
                            break;
                        }
                    }
                }

                ImGui::End();
            }
        }
    }

    if (state.isStaging)
    {
        int stageMonitorIdx = state.stageMonitorIndex;
        int stageMonitorCount = 0;
        GLFWmonitor** stageMonitors = glfwGetMonitors(&stageMonitorCount);

        if (stageMonitors && stageMonitorIdx >= 0 && stageMonitorIdx < stageMonitorCount)
        {
            const GLFWvidmode* stageMode = glfwGetVideoMode(stageMonitors[stageMonitorIdx]);

            if (stageMode && stageMode->width > 0 && stageMode->height > 0)
            {
                int smx = 0, smy = 0;
                glfwGetMonitorPos(stageMonitors[stageMonitorIdx], &smx, &smy);

                ImGui::SetNextWindowPos(ImVec2((float)smx, (float)smy));
                ImGui::SetNextWindowSize(ImVec2((float)stageMode->width, (float)stageMode->height));

                ImGuiWindowFlags stageFlags =
                    ImGuiWindowFlags_NoDecoration          |
                    ImGuiWindowFlags_NoBackground          |
                    ImGuiWindowFlags_NoSavedSettings       |
                    ImGuiWindowFlags_NoFocusOnAppearing    |
                    ImGuiWindowFlags_NoNav                 |
                    ImGuiWindowFlags_NoBringToFrontOnFocus;

                ImGuiWindowClass stageClass;
                stageClass.ViewportFlagsOverrideSet =
                    ImGuiViewportFlags_NoAutoMerge | ImGuiViewportFlags_TopMost;
                ImGui::SetNextWindowClass(&stageClass);

                ImGui::Begin("StageLive", nullptr, stageFlags);
                ImDrawList* stageDrawList = ImGui::GetWindowDrawList();

                DrawStageContent(
                    stageDrawList,
                    ImVec2((float)smx, (float)smy),
                    ImVec2((float)(smx + stageMode->width), (float)(smy + stageMode->height)));

                ImGui::End();
            }
        }
    }

    if (m_ShowConfig)
        m_SettingsPanel.Render(&m_ShowConfig);

    if (m_ShowNotes)
        RenderNotesWindow();

    {
        auto& general = ProyecThor::Settings::SettingsManager::Get().GetSettings().general;
        if (general.showPerfPanel)
        {
            bool wasOpen = general.showPerfPanel;
            m_PerformancePanel.Render(&general.showPerfPanel);
            if (wasOpen && !general.showPerfPanel)
                ProyecThor::Settings::SettingsManager::Get().Save();
        }
    }

    if (g_ShowAbout)
        ImGui::OpenPopup(str.menuAbout);

    ImGuiViewport* viewport = ImGui::GetWindowViewport();
    ImVec2 work_pos  = viewport->WorkPos;
    ImVec2 work_size = viewport->WorkSize;
    ImVec2 center    = ImVec2(work_pos.x + work_size.x * 0.5f,
                              work_pos.y + work_size.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.060f, 0.065f, 0.088f, 0.99f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.250f, 0.210f, 0.090f, 0.80f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(24.0f, 20.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);

    if (ImGui::BeginPopupModal(str.menuAbout, &g_ShowAbout,
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.886f, 0.753f, 0.408f, 1.0f));
        ImGui::SetWindowFontScale(1.20f);
        ImGui::Text("%s", str.appTitle);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.420f, 0.420f, 0.420f, 1.0f));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.0f);
        ImGui::Text("v0.1.6");
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.250f, 0.210f, 0.090f, 0.60f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.780f, 0.770f, 0.740f, 1.0f));
        ImGui::Text("%s", str.aboutDesc);
        ImGui::Spacing();
        ImGui::TextWrapped("%s", str.aboutNonProfit);
        ImGui::Spacing();
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.500f, 0.500f, 0.490f, 1.0f));
        ImGui::Text("Creado por TheVixcho y la comunidad de ProyecThor");
        ImGui::Spacing();
        ImGui::TextDisabled("2026");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.180f, 0.185f, 0.230f, 1.0f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.240f, 0.200f, 0.085f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.500f, 0.415f, 0.180f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.650f, 0.530f, 0.220f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.920f, 0.820f, 0.560f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(16.0f, 6.0f));

        if (ImGui::Button(str.close)) {
            ImGui::CloseCurrentPopup();
            g_ShowAbout = false;
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);
        ImGui::SetItemDefaultFocus();
        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

   EndDockspace();

    m_DatabasePanel.Render();
    m_WikiPanel.Render();

    RenderMainMenuBar();
}

static void RenderSocialQrMenu(const char* url)
{
    ImGui::Text("Escanea con tu celular:");
    ImGui::Spacing();

    static const char* s_CachedUrl = nullptr;
    static qrcodegen::QrCode s_CachedQr = qrcodegen::QrCode::encodeText(" ", qrcodegen::QrCode::Ecc::MEDIUM);

    if (s_CachedUrl != url)
    {
        s_CachedQr = qrcodegen::QrCode::encodeText(url, qrcodegen::QrCode::Ecc::MEDIUM);
        s_CachedUrl = url;
    }

    int qrSize = s_CachedQr.getSize();
    float cellSize = 5.0f;
    float margin = cellSize * 2.0f;
    float totalSize = (qrSize * cellSize) + (margin * 2.0f);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRectFilled(pos, ImVec2(pos.x + totalSize, pos.y + totalSize), IM_COL32(255, 255, 255, 255));

    for (int y = 0; y < qrSize; y++) {
        for (int x = 0; x < qrSize; x++) {
            if (s_CachedQr.getModule(x, y)) {
                ImVec2 minP(pos.x + margin + (x * cellSize), pos.y + margin + (y * cellSize));
                ImVec2 maxP(pos.x + margin + ((x + 1) * cellSize), pos.y + margin + ((y + 1) * cellSize));
                drawList->AddRectFilled(minP, maxP, IM_COL32(0, 0, 0, 255));
            }
        }
    }

    ImGui::Dummy(ImVec2(totalSize, totalSize));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Abrir en el navegador", ImVec2(totalSize, 0)))
        ProyecThor::External::OpenURL(url);
}

void UIManager::ToggleFullscreen()
{
    if (!m_Window) return;

    if (glfwGetWindowMonitor(m_Window) != nullptr) {
        glfwSetWindowMonitor(m_Window, nullptr, m_WindowedX, m_WindowedY, m_WindowedW, m_WindowedH, 0);
    } else {
        glfwGetWindowPos(m_Window, &m_WindowedX, &m_WindowedY);
        glfwGetWindowSize(m_Window, &m_WindowedW, &m_WindowedH);

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        if (!monitor) return;
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if (!mode) return;
        glfwSetWindowMonitor(m_Window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    }
}

void UIManager::RenderModeToolbar()
{

    auto& general = ProyecThor::Settings::SettingsManager::Get().GetSettings().general;
    if (!general.showModeToolbar) return;

    // Grupo izquierdo (Hub/Proyector) separado del resto por una linea
    // vertical -- pedido explicito para que Conexiones/Notas/Biblia queden
    // claramente aparte de los dos modos "de trabajo" principales.
    static const IconRailItem kItemsLeft[] = {
        { (int)WorkspaceMode::Hub,        HomeIcons::DrawIcon_Home,      "Hub"        },
        { (int)WorkspaceMode::Projector,  AppIcons::DrawIcon_Monitor,    "Proyector"  },
    };
    static const IconRailItem kItemsRight[] = {
        { (int)WorkspaceMode::Yggdrasil,  AppIcons::DrawIcon_Antenna,    "Conexiones" },
    };
    static const IconRailItem kItemsTail[] = {
        { (int)WorkspaceMode::Biblia,     Library::DrawIcon_Cross,       "Biblia"     },
    };

    ImVec4 accent = ImGui::ColorConvertU32ToFloat4(DS::AccentColor);

    ImGuiViewport* vp     = ImGui::GetMainViewport();
    float          railH  = IconRailThickness(false);

    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, railH));
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove       |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::ColorConvertU32ToFloat4(DS::GlassFillTop));
    ImGui::Begin("##ModeToolbar", nullptr, flags);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    {
        ImDrawList*   dl      = ImGui::GetWindowDrawList();
        bool          showLbl = general.showRailLabels;
        const float   padY    = 4.0f;
        const float   padX    = 10.0f;
        const float   gap     = 6.0f;
        const float   iconGap = 3.0f;
        const float   btnH    = railH - padY * 2.0f;
        const float   rounding= 10.0f;

        ImFont* font          = ImGui::GetFont();
        const float labelSz   = std::max(9.0f, std::floor(ImGui::GetFontSize() * 0.72f));
        const float iconSz    = std::max(12.0f, btnH - (showLbl ? (labelSz + iconGap) : 0.0f) - 2.0f);

        ImGuiStorage* storage = ImGui::GetStateStorage();

        ImGui::SetCursorPos(ImVec2(10.0f, padY));

        // Pastillas de icono+etiqueta APILADOS (icono arriba, texto abajo) --
        // pedido explicito en vez del layout lado a lado de antes, mismo
        // idioma visual que una bottom-tab-bar. drawLabel/measureLabel usan
        // labelSz (mas chico que el font por defecto) para que el texto entre
        // completo debajo del icono sin agrandar la barra.
        auto measureLabelW = [&](const char* text) {
            return showLbl ? font->CalcTextSizeA(labelSz, FLT_MAX, 0.0f, text).x : 0.0f;
        };
        auto drawLabelCentered = [&](const char* text, float btnW, ImVec2 bMin, float labelY, ImU32 col) {
            if (!showLbl) return;
            float w = font->CalcTextSizeA(labelSz, FLT_MAX, 0.0f, text).x;
            float x = bMin.x + (btnW - w) * 0.5f;
            dl->AddText(font, labelSz, { x, labelY }, col, text);
        };

        // Una sola pastilla icono+etiqueta apilados -- factorizado para que
        // los grupos Hub/Proyector, Conexiones, Notas y Biblia (cada uno con
        // su propia fuente de "activo") compartan el mismo dibujo en vez de
        // triplicar/cuadruplicar el mismo bloque de ~30 lineas.
        auto RenderPill = [&](const char* label, DrawIconFn drawIcon, bool active, bool sameLine, float sameLineSpacing) -> bool {
            float lblW     = measureLabelW(label);
            float contentW = std::max(iconSz, lblW);
            float btnW     = contentW + padX * 2.0f;

            if (sameLine) ImGui::SameLine(0.0f, sameLineSpacing);

            ImVec2 cursor = ImGui::GetCursorScreenPos();
            ImVec2 bMin   = cursor;
            ImVec2 bMax   = { cursor.x + btnW, cursor.y + btnH };

            ImGuiID hovId = ImGui::GetID(label);
            float*  pT    = storage->GetFloatRef(hovId ^ 0x51A17E5u, 0.0f);
            bool hovered  = ImGui::IsMouseHoveringRect(bMin, bMax, false);
            *pT += ((hovered ? 1.0f : 0.0f) - *pT) * std::min(1.0f, ImGui::GetIO().DeltaTime * 14.0f);
            float t = *pT;

            if (active) {
                dl->AddRectFilled(bMin, bMax, ImGui::ColorConvertFloat4ToU32(accent), rounding);
            } else if (t > 0.01f) {
                dl->AddRectFilled(bMin, bMax, IM_COL32(255, 255, 255, (int)(t * 18.0f)), rounding);
            }

            ImGui::SetCursorScreenPos(bMin);
            const std::string btnId = std::string("##modeTb_") + label;
            bool clicked = ImGui::InvisibleButton(btnId.c_str(), { btnW, btnH });

            ImU32 icCol;
            if (active) {
                icCol = IM_COL32(18, 18, 20, 255);
            } else {
                ImVec4 base  = ImGui::ColorConvertU32ToFloat4(DS::TextSecondary);
                ImVec4 hover = ImGui::ColorConvertU32ToFloat4(DS::TextPrimary);
                base.x += (hover.x - base.x) * t;
                base.y += (hover.y - base.y) * t;
                base.z += (hover.z - base.z) * t;
                icCol = ImGui::ColorConvertFloat4ToU32(base);
            }

            float iconX = bMin.x + (btnW - iconSz) * 0.5f;
            float iconY = bMin.y + 1.0f;
            drawIcon(dl, { iconX, iconY }, iconSz, icCol);
            drawLabelCentered(label, btnW, bMin, iconY + iconSz + iconGap, icCol);

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                ImGui::SetTooltip("%s", label);

            return clicked;
        };

        auto RenderModeItem = [&](const IconRailItem& item, bool sameLine, float sameLineSpacing) {
            bool active  = ((int)m_Mode == item.index);
            bool clicked = RenderPill(item.label, item.drawIcon, active, sameLine, sameLineSpacing);
            if (clicked && !active)
            {
                m_Mode = (WorkspaceMode)item.index;
                if (m_Mode == WorkspaceMode::Hub)       m_Hub.ForceOpen();
                if (m_Mode == WorkspaceMode::Projector) m_ResetLayout = true;
            }
        };

        // ── Grupo izquierdo: Hub / Proyector ─────────────────────────────
        for (int i = 0; i < (int)(sizeof(kItemsLeft) / sizeof(kItemsLeft[0])); i++)
            RenderModeItem(kItemsLeft[i], i > 0, gap);

        // ── Linea separadora ──────────────────────────────────────────────
        {
            ImGui::SameLine(0.0f, gap * 2.0f);
            ImVec2 p = ImGui::GetCursorScreenPos();
            float  sepH = btnH * 0.7f;
            dl->AddRectFilled({ p.x, p.y + (btnH - sepH) * 0.5f },
                              { p.x + 1.0f, p.y + (btnH - sepH) * 0.5f + sepH },
                              IM_COL32(255, 255, 255, 30));
            ImGui::Dummy(ImVec2(1.0f, btnH));
        }

        // ── Grupo derecho: Conexiones, luego (con su propio espacio) Notas
        //    y Biblia ──────────────────────────────────────────────────────
        for (int i = 0; i < (int)(sizeof(kItemsRight) / sizeof(kItemsRight[0])); i++)
            RenderModeItem(kItemsRight[i], true, gap * 2.0f);

        {
            bool clicked = RenderPill("Notas", HomeIcons::DrawIcon_Notepad, m_ShowNotes, true, gap * 2.0f);
            if (clicked) m_ShowNotes = !m_ShowNotes;
        }

        for (int i = 0; i < (int)(sizeof(kItemsTail) / sizeof(kItemsTail[0])); i++)
            RenderModeItem(kItemsTail[i], true, gap);
    }

    RenderModeToolbarStatusActions(ImGui::GetWindowWidth(), railH);

    ImGui::End();

    vp->WorkPos.y  += railH;
    vp->WorkSize.y -= railH;
}

void UIManager::RenderModeToolbarStatusActions(float winW, float railH)
{
    auto& core = Core::PresentationCore::Get();
    auto& sd   = ProyecThor::Settings::SettingsManager::Get().GetSettings().stageDisplay;

    const bool  audienceOn = core.IsProjecting();
    const bool  stageOn    = sd.useLAN ? core.IsStreamingNet() : core.IsStaging();
    const float rowH       = std::min(28.0f, railH - 4.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    const char* clearLabel = "Borrar Todo";
    ImVec2      clearTxtSz = ImGui::CalcTextSize(clearLabel);
    const float clearIconSz  = rowH * 0.55f;
    const float clearIconGap = 8.0f;
    float       clearGroupW  = clearIconSz + clearIconGap + clearTxtSz.x;
    float       clearBtnW    = clearGroupW + 24.0f;

    ImVec2 dotSzAudience = ImVec2(5.0f * 2.0f + 6.0f + ImGui::CalcTextSize("Publico").x + 14.0f, rowH);
    ImVec2 dotSzStage    = ImVec2(5.0f * 2.0f + 6.0f + ImGui::CalcTextSize("Stage").x    + 14.0f, rowH);

    const float gap   = 14.0f;
    float       totalW = dotSzAudience.x + gap + dotSzStage.x + gap + clearBtnW;
    float       startX = std::max(10.0f, winW - totalW - 12.0f);

    ImGui::SetCursorPos(ImVec2(startX, (railH - rowH) * 0.5f));

    if (StatusDotToggle(dl, "##modeTbDotAudience", "Publico", audienceOn, MT::k_LiveAccent, rowH))
        ToggleAudience(!audienceOn);

    ImGui::SameLine(0.0f, gap);

    if (StatusDotToggle(dl, "##modeTbDotStage", "Stage", stageOn, MT::k_PrevAccent, rowH))
        ToggleStageQuick(!stageOn);

    ImGui::SameLine(0.0f, gap);

    {
        auto toVec4 = [](ImU32 c, float alpha) {
            ImVec4 v = ImGui::ColorConvertU32ToFloat4(c);
            v.w = alpha;
            return v;
        };
        ImGui::PushStyleColor(ImGuiCol_Button,        toVec4(DS::DangerColor, 40.0f  / 255.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, toVec4(DS::DangerColor, 90.0f  / 255.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  toVec4(DS::DangerColor, 140.0f / 255.0f));
        ImGui::PushStyleColor(ImGuiCol_Border,        toVec4(DS::DangerColor, 100.0f / 255.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rowH * 0.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        bool clicked = ImGui::Button("##modeTbBorrarTodo", ImVec2(clearBtnW, rowH));

        ImVec2 bMin = ImGui::GetItemRectMin();
        ImVec2 bMax = ImGui::GetItemRectMax();
        float  cStartX = bMin.x + ((bMax.x - bMin.x) - clearGroupW) * 0.5f;
        float  cCenterY = (bMin.y + bMax.y) * 0.5f;
        ImU32  dangerCol = DS::DangerColor;

        auto it = StyleGeneralApp::Icons.find("cleaning_services");
        if (it != StyleGeneralApp::Icons.end() && it->second.textureID)
        {
            dl->AddImage((ImTextureID)(intptr_t)it->second.textureID,
                { cStartX, cCenterY - clearIconSz * 0.5f }, { cStartX + clearIconSz, cCenterY + clearIconSz * 0.5f },
                ImVec2(0, 0), ImVec2(1, 1), dangerCol);
        }
        dl->AddText({ cStartX + clearIconSz + clearIconGap, cCenterY - clearTxtSz.y * 0.5f }, dangerCol, clearLabel);

        if (clicked)
        {
            core.ClearLayer2();
            core.StopBackgroundMedia();
            if (auto* a   = core.GetAnnouncementsRef())  a->SetLive(false);
            if (auto* clk = core.GetOClockRef())          clk->StopTransmitting();
            if (auto* cap = core.GetCapturePanelRef())    cap->Stop();
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);
    }
}

void UIManager::ToggleAudience(bool active)
{
    auto& core = Core::PresentationCore::Get();

    if (active) {
        auto& settings = ProyecThor::Settings::SettingsManager::Get().GetSettings();
        int monitorCount = 0;
        glfwGetMonitors(&monitorCount);
        int monitorIndex = std::clamp(
            settings.projection.targetMonitor < 0 ? 1 : settings.projection.targetMonitor,
            0, std::max(0, monitorCount - 1));

        core.SetTargetMonitor(monitorIndex);
        std::cout << "[UIManager] Proyección iniciada en monitor " << monitorIndex << ".\n";
    } else {
        std::cout << "[UIManager] Proyección detenida.\n";
    }

    core.SetProjecting(active);
}

void UIManager::ToggleStageQuick(bool active)
{
    auto& core = Core::PresentationCore::Get();
    auto& sd   = ProyecThor::Settings::SettingsManager::Get().GetSettings().stageDisplay;

    if (active) {
        if (sd.useLAN) {
            core.ToggleNetworkStream(true, sd.lanPort);
            return;
        }

        int monitorCount = 0;
        glfwGetMonitors(&monitorCount);
        if (monitorCount < 2) {
            std::cerr << "[UIManager] No hay suficientes monitores para activar el stage.\n";
            return;
        }

        int stageMonitorIndex = std::clamp(sd.monitorIndex < 0 ? 1 : sd.monitorIndex, 0, monitorCount - 1);
        sd.monitorIndex = stageMonitorIndex;
        ProyecThor::Settings::SettingsManager::Get().Save();
        core.SetStaging(true, stageMonitorIndex);
    } else {
        if (sd.useLAN) core.ToggleNetworkStream(false);
        else           core.SetStaging(false);
    }
}

void UIManager::RenderNotesWindow()
{
    static bool s_WasOpenLastFrame = false;
    const bool  justOpened = !s_WasOpenLastFrame;
    s_WasOpenLastFrame = true;

    const ImVec2 baseSize(520.0f, 480.0f);

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 workCenter(vp->WorkPos.x + vp->WorkSize.x * 0.5f,
                       vp->WorkPos.y + vp->WorkSize.y * 0.5f);

    if (justOpened) {
        ImGui::SetNextWindowPos(workCenter, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(baseSize, ImGuiCond_Always);
    }
    ImGui::SetNextWindowSizeConstraints(ImVec2(420.0f, 360.0f), ImVec2(10000.0f, 10000.0f));

    ImGuiWindowClass floatingClass;
    floatingClass.DockingAllowUnclassed = false;
    ImGui::SetNextWindowClass(&floatingClass);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
    bool open = ImGui::Begin("Notas", &m_ShowNotes,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking);
    ImGui::PopStyleVar();

    if (open)
        m_NotesPanel.Render();

    ImGui::End();

    if (!m_ShowNotes)
        s_WasOpenLastFrame = false;
}

void UIManager::RenderQuickSwitcher()
{
    struct QSItem { WorkspaceMode mode; DrawIconFn icon; const char* label; };
    static const QSItem kItems[] = {
        { WorkspaceMode::Hub,        HomeIcons::DrawIcon_Home,      "Hub"        },
        { WorkspaceMode::Projector,  AppIcons::DrawIcon_Monitor,    "Proyector"  },
        { WorkspaceMode::Yggdrasil,  AppIcons::DrawIcon_Antenna,    "Conexiones" },
        { WorkspaceMode::Biblia,     Library::DrawIcon_Cross,       "Biblia"     },
    };
    constexpr int kCount = 4;

    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_Space, false))
    {
        m_QuickSwitchOpen  = !m_QuickSwitchOpen;
        for (int i = 0; i < kCount; i++)
            if (kItems[i].mode == m_Mode) m_QuickSwitchIndex = i;
    }
    if (!m_QuickSwitchOpen) return;

    auto Activate = [&](int idx) {
        m_Mode = kItems[idx].mode;
        if (m_Mode == WorkspaceMode::Hub)       m_Hub.ForceOpen();
        if (m_Mode == WorkspaceMode::Projector) m_ResetLayout = true;
        m_QuickSwitchOpen = false;
    };

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) m_QuickSwitchOpen = false;
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
        m_QuickSwitchIndex = (m_QuickSwitchIndex + 1) % kCount;
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
        m_QuickSwitchIndex = (m_QuickSwitchIndex + kCount - 1) % kCount;
    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))
        Activate(m_QuickSwitchIndex);
    if (!m_QuickSwitchOpen) return;

    const ImVec2 winSize(340.0f, 44.0f + kCount * 42.0f);
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + (vp->WorkSize.x - winSize.x) * 0.5f,
                                    vp->WorkPos.y + (vp->WorkSize.y - winSize.y) * 0.5f));
    ImGui::SetNextWindowSize(winSize);
    ImGui::SetNextWindowFocus();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(10.0f, 10.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.070f, 0.075f, 0.100f, 0.97f));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.300f, 0.320f, 0.420f, 0.90f));

    ImGui::Begin("##QuickSwitcher", &m_QuickSwitchOpen,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoDocking    | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoResize     | ImGuiWindowFlags_NoNav);

    ImGui::TextDisabled("Ir a...   (flechas + Enter, Esc para cerrar)");
    ImGui::Spacing();

    for (int i = 0; i < kCount; i++)
    {
        bool sel = (i == m_QuickSwitchIndex);
        ImGui::PushID(i);

        ImVec2 rowPos = ImGui::GetCursorScreenPos();
        const float rowH = 38.0f;

        if (sel) ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.30f, 0.45f, 0.90f, 0.55f));
        if (ImGui::Selectable("##qsRow", sel, 0, ImVec2(0.0f, rowH)))
            Activate(i);
        if (sel) ImGui::PopStyleColor();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 col = sel ? IM_COL32(255, 255, 255, 255) : IM_COL32(180, 182, 198, 255);
        kItems[i].icon(dl, ImVec2(rowPos.x + 8.0f, rowPos.y + (rowH - 22.0f) * 0.5f), 22.0f, col);
        dl->AddText(ImVec2(rowPos.x + 42.0f, rowPos.y + (rowH - ImGui::GetTextLineHeight()) * 0.5f),
                    col, kItems[i].label);

        ImGui::PopID();
    }

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

void UIManager::RenderMainMenuBar()
{
    RenderQuickSwitcher();

    const auto& str = ProyecThor::UI::GetUIStrings();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(10.0f, 4.0f));
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg,        ImVec4(0.052f, 0.056f, 0.078f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(14.0f, 10.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,             ImVec4(0.860f, 0.880f, 0.940f, 1.0f));

    if (ImGui::BeginMainMenuBar())
    {

        if (ImGui::BeginMenu("ProyecThor"))
        {
            ImGui::Spacing();
            if (ImGui::MenuItem(str.menuPrefs, "Ctrl+P"))
                m_ShowConfig = true;

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.200f, 0.210f, 0.300f, 0.600f));
            ImGui::Separator();
            ImGui::PopStyleColor();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.45f, 0.45f, 1.0f));
            if (ImGui::MenuItem(str.menuExit, "Alt+F4"))
                glfwSetWindowShouldClose(m_Window, true);
            ImGui::PopStyleColor();
            ImGui::Spacing();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(str.menuFile))
        {
            ImGui::Spacing();

            if (ImGui::BeginMenu(str.importLabel))
            {
                if (ImGui::MenuItem("Importar cancion desde portapapeles"))
                {
                    const char* clip = ImGui::GetClipboardText();
                    if (clip && clip[0] != '\0')
                        ProyecThor::Library::CreateNewSongFromClipboard(clip);
                }
                ImGui::EndMenu();
            }

            ImGui::Spacing();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(str.menuView))
        {
            ImGui::Spacing();
            if (ImGui::MenuItem(str.menuResetLayout))
                m_ResetLayout = true;

            if (ImGui::MenuItem("Hub de inicio"))
                OpenHub();

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.200f, 0.210f, 0.300f, 0.600f));
            ImGui::Separator();
            ImGui::PopStyleColor();
            ImGui::Spacing();

            auto& general = ProyecThor::Settings::SettingsManager::Get().GetSettings().general;
            if (ImGui::MenuItem("Titulos en barras de iconos", nullptr, general.showRailLabels))
            {
                general.showRailLabels = !general.showRailLabels;
                ProyecThor::Settings::SettingsManager::Get().Save();
            }

            if (ImGui::MenuItem("Rendimiento", nullptr, general.showPerfPanel))
            {
                general.showPerfPanel = !general.showPerfPanel;
                ProyecThor::Settings::SettingsManager::Get().Save();
            }

            if (ImGui::MenuItem("Botones de limpieza (Vista en Vivo)", nullptr, general.showViewQuickActions))
            {
                general.showViewQuickActions = !general.showViewQuickActions;
                ProyecThor::Settings::SettingsManager::Get().Save();
            }

            if (ImGui::MenuItem("Barra de modos (Streaming/Conexiones)", nullptr, general.showModeToolbar))
            {
                general.showModeToolbar = !general.showModeToolbar;
                ProyecThor::Settings::SettingsManager::Get().Save();
            }

            ImGui::Spacing();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(str.menuHelp))
        {
            ImGui::Spacing();

            if (ImGui::MenuItem("Base de datos"))
                m_DatabasePanel.Open();

            if (ImGui::MenuItem("Wiki"))
                ProyecThor::External::OpenURL("https://github.com/TheVixcho/ProyecThor/wiki");

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.200f, 0.210f, 0.300f, 0.600f));
            ImGui::Separator();
            ImGui::PopStyleColor();
            ImGui::Spacing();

            if (ImGui::MenuItem(str.menuDocs, "F1"))
                ProyecThor::External::OpenURL("https://proyecthor.web.app/");

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.345f, 0.403f, 0.941f, 1.0f));
            if (ImGui::MenuItem("Reporte de bugs"))
                ProyecThor::External::OpenURL("https://github.com/TheVixcho/ProyecThor/issues");
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.345f, 0.396f, 0.949f, 1.0f));
            bool discordOpen = ImGui::BeginMenu("Canal de Discord");
            ImGui::PopStyleColor();
            if (discordOpen)
            {
                const char* discordUrl = "https://discord.gg/RMk8AGC5pn";
                RenderSocialQrMenu(discordUrl);
                ImGui::EndMenu();
            }

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.145f, 0.827f, 0.400f, 1.0f));
            bool whatsappOpen = ImGui::BeginMenu("Canal de WhatsApp");
            ImGui::PopStyleColor();
            if (whatsappOpen)
            {
                const char* whatsappUrl = "https://whatsapp.com/channel/0029Vb7e9tj3WHTdNivIxR19";
                RenderSocialQrMenu(whatsappUrl);
                ImGui::EndMenu();
            }

            ImGui::BeginDisabled(true);
            ImGui::MenuItem("Canal de YouTube");
            ImGui::EndDisabled();

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.886f, 0.753f, 0.408f, 1.0f));
            if (ImGui::MenuItem(str.menuDonations))
                ProyecThor::External::OpenURL("https://ko-fi.com/vixcho");
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.200f, 0.210f, 0.300f, 0.600f));
            ImGui::Separator();
            ImGui::PopStyleColor();
            ImGui::Spacing();

            if (ImGui::MenuItem(str.menuAbout))
                g_ShowAbout = true;

            ImGui::Spacing();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Ventana"))
        {
            ImGui::Spacing();
            bool isFullscreen = (glfwGetWindowMonitor(m_Window) != nullptr);
            if (ImGui::MenuItem("Pantalla completa", "F11", isFullscreen))
                ToggleFullscreen();
            ImGui::Spacing();
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

void UIManager::BeginDockspace()
{
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    window_flags |=
        ImGuiWindowFlags_NoTitleBar            | ImGuiWindowFlags_NoCollapse          |
        ImGuiWindowFlags_NoResize              | ImGuiWindowFlags_NoMove              |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus          |
        ImGuiWindowFlags_NoBackground;

    ImGui::Begin("ProyecThorWorkspace", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    const auto& str = ProyecThor::UI::GetUIStrings();

    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    if (m_ResetLayout || !ImGui::DockBuilderGetNode(dockspace_id))
    {
        m_ResetLayout = false;

        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

        ImGuiID dock_main = dockspace_id;

        ImGuiID dock_left = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.25f, nullptr, &dock_main);
        ImGuiID dock_left_top, dock_left_bottom;
        ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Down, 0.40f, &dock_left_bottom, &dock_left_top);

        ImGuiID dock_right;
        ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.37f, &dock_right, &dock_main);

        ImGuiID dock_center_right;
        ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.45f, &dock_center_right, &dock_main);
        ImGuiID dock_center_right_top, dock_center_right_bottom;
        ImGui::DockBuilderSplitNode(dock_center_right, ImGuiDir_Down, 0.70f, &dock_center_right_bottom, &dock_center_right_top);

        ImGuiID dock_main_top, dock_main_bottom;
        ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.30f, &dock_main_bottom, &dock_main_top);
ImGui::DockBuilderDockWindow(str.library,          dock_left_top);

ImGui::DockBuilderDockWindow("Home",               dock_main_top);
ImGui::DockBuilderDockWindow("Vista en Vivo",      dock_right);

        ImGui::DockBuilderDockWindow("Diseño",              dock_main_bottom);

{
    ImGuiID leafNodes[] = {
        dock_left_top, dock_left_bottom,
        dock_main_top, dock_main_bottom,
        dock_right
    };
    for (ImGuiID nodeId : leafNodes)
    {
        if (ImGuiDockNode* node = ImGui::DockBuilderGetNode(nodeId))
            node->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
    }
}

ImGui::DockBuilderFinish(dockspace_id);

        m_FocusViewNextFrame = true;
    }
}

void UIManager::EndDockspace()
{
    ImGui::End();
}

void UIManager::Shutdown()
{
    m_Panels.clear();
}

}
