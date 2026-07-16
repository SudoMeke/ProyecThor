#include <GL/glew.h>
#include "UIManager.h"
#include "backend/core/PresentationCore.h"
#include "../toolbar/ConfigPanel.h"
#include "panels/PreviewPanel.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <algorithm>
#include <imgui_internal.h>
#include "../external/tools/OpenURL.h"
#include "UIStrings.h"
#include "frontend/views/Announcements.h"
#include "Hub.h"
#include "panels/BackgroundsPanel.h"
#include "panels/CanvasStylesPanel.h"
#include "frontend/panels/StreamingPanel.h"
#include "qrcodegen.hpp"
#include "backend/settings/SettingsManager.h"
#include "backend/settings/ProjectionQualityPresets.h"
#include "backend/settings/StageLayoutTemplates.h"
#include <ctime>

namespace ProyecThor::UI {

static bool g_ShowAbout = false;

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

    // Crear el TransitionPanel aqui para que este disponible antes de AddPanel
    m_TransitionPanelOwned = std::make_shared<TransitionPanel>();
    m_TransitionPanel      = m_TransitionPanelOwned.get();

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
 
    // ── Espaciado — valores aumentados para que nada choque con bordes ────────
    s.WindowPadding          = ImVec2(22.0f, 18.0f);   // era (20, 16)
    s.FramePadding           = ImVec2(14.0f,  9.0f);   // era (12, 7)
    s.ItemSpacing            = ImVec2(12.0f,  8.0f);   // era (10, 7)
    s.ItemInnerSpacing       = ImVec2( 8.0f,  6.0f);   // era (6, 6)
    s.CellPadding            = ImVec2(10.0f,  7.0f);   // era (8, 6)
    s.TouchExtraPadding      = ImVec2( 0.0f,  0.0f);
    s.IndentSpacing          = 18.0f;                   // era 16
    s.ScrollbarSize          =  6.0f;                   // era 8 — mas delgada
    s.GrabMinSize            =  8.0f;
 
    // ── Redondeo — mas moderno ─────────────────────────────────────────────────
    s.WindowRounding         = 16.0f;   // era 14
    s.ChildRounding          = 12.0f;   // era 10
    s.FrameRounding          =  9.0f;   // era 8
    s.PopupRounding          = 14.0f;   // era 12
    s.ScrollbarRounding      = 12.0f;
    s.GrabRounding           =  6.0f;
    s.TabRounding            =  9.0f;   // era 8
    s.WindowMenuButtonPosition = ImGuiDir_None;
 
    // ── Bordes ────────────────────────────────────────────────────────────────
    s.WindowBorderSize       = 1.0f;
    s.ChildBorderSize        = 1.0f;
    s.PopupBorderSize        = 1.0f;
    s.FrameBorderSize        = 0.0f;
    s.TabBorderSize          = 0.0f;
    s.TabBarBorderSize       = 0.0f;
 
    ImVec4* c = s.Colors;
 
    // ── Fondos de ventana ─────────────────────────────────────────────────────
    c[ImGuiCol_WindowBg]             = ImVec4(0.036f, 0.040f, 0.060f, 1.000f);
    c[ImGuiCol_ChildBg]              = ImVec4(0.030f, 0.034f, 0.052f, 0.650f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.040f, 0.044f, 0.066f, 0.985f);
    c[ImGuiCol_Border]               = ImVec4(1.000f, 1.000f, 1.000f, 0.075f);
    c[ImGuiCol_BorderShadow]         = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
 
    // ── Frames (inputs, combos, etc.) ─────────────────────────────────────────
    c[ImGuiCol_FrameBg]              = ImVec4(1.000f, 1.000f, 1.000f, 0.042f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(1.000f, 1.000f, 1.000f, 0.075f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(1.000f, 1.000f, 1.000f, 0.108f);
 
    // ── Barras de titulo ──────────────────────────────────────────────────────
    c[ImGuiCol_TitleBg]              = ImVec4(0.025f, 0.028f, 0.044f, 1.000f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.032f, 0.036f, 0.056f, 1.000f);
    c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.025f, 0.028f, 0.044f, 0.800f);
    c[ImGuiCol_MenuBarBg]            = ImVec4(0.022f, 0.025f, 0.040f, 1.000f);
 
    // ── Scrollbar ─────────────────────────────────────────────────────────────
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(1.000f, 1.000f, 1.000f, 0.110f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1.000f, 1.000f, 1.000f, 0.185f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.369f, 0.420f, 1.000f, 0.880f);
 
    // ── Controles ─────────────────────────────────────────────────────────────
    c[ImGuiCol_CheckMark]            = ImVec4(0.400f, 0.455f, 1.000f, 1.000f);
    c[ImGuiCol_SliderGrab]           = ImVec4(0.400f, 0.455f, 1.000f, 1.000f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.520f, 0.575f, 1.000f, 1.000f);
 
    // ── Botones ───────────────────────────────────────────────────────────────
    c[ImGuiCol_Button]               = ImVec4(1.000f, 1.000f, 1.000f, 0.048f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(1.000f, 1.000f, 1.000f, 0.088f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.369f, 0.420f, 1.000f, 1.000f);
 
    // ── Headers (selectables, tree nodes) ────────────────────────────────────
    c[ImGuiCol_Header]               = ImVec4(0.369f, 0.420f, 1.000f, 0.148f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.369f, 0.420f, 1.000f, 0.215f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.369f, 0.420f, 1.000f, 0.375f);
 
    // ── Separadores ───────────────────────────────────────────────────────────
    c[ImGuiCol_Separator]            = ImVec4(1.000f, 1.000f, 1.000f, 0.055f);
    c[ImGuiCol_SeparatorHovered]     = ImVec4(0.369f, 0.420f, 1.000f, 0.380f);
    c[ImGuiCol_SeparatorActive]      = ImVec4(0.369f, 0.420f, 1.000f, 0.780f);
 
    // ── Resize grip ───────────────────────────────────────────────────────────
    c[ImGuiCol_ResizeGrip]           = ImVec4(0.369f, 0.420f, 1.000f, 0.095f);
    c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.369f, 0.420f, 1.000f, 0.360f);
    c[ImGuiCol_ResizeGripActive]     = ImVec4(0.369f, 0.420f, 1.000f, 0.780f);
 
    // ── Tabs ─────────────────────────────────────────────────────────────────
    c[ImGuiCol_Tab]                  = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
    c[ImGuiCol_TabHovered]           = ImVec4(1.000f, 1.000f, 1.000f, 0.068f);
    c[ImGuiCol_TabActive]            = ImVec4(1.000f, 1.000f, 1.000f, 0.108f);
    c[ImGuiCol_TabUnfocused]         = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
    c[ImGuiCol_TabUnfocusedActive]   = ImVec4(1.000f, 1.000f, 1.000f, 0.058f);
 
    // ── Docking ───────────────────────────────────────────────────────────────
    c[ImGuiCol_DockingPreview]       = ImVec4(0.369f, 0.420f, 1.000f, 0.268f);
    c[ImGuiCol_DockingEmptyBg]       = ImVec4(0.026f, 0.029f, 0.044f, 1.000f);
 
    // ── Graficos ──────────────────────────────────────────────────────────────
    c[ImGuiCol_PlotLines]            = ImVec4(0.369f, 0.420f, 1.000f, 1.000f);
    c[ImGuiCol_PlotLinesHovered]     = ImVec4(0.520f, 0.575f, 1.000f, 1.000f);
    c[ImGuiCol_PlotHistogram]        = ImVec4(0.369f, 0.420f, 1.000f, 1.000f);
    c[ImGuiCol_PlotHistogramHovered] = ImVec4(0.520f, 0.575f, 1.000f, 1.000f);
 
    // ── Tablas ────────────────────────────────────────────────────────────────
    c[ImGuiCol_TableHeaderBg]        = ImVec4(1.000f, 1.000f, 1.000f, 0.038f);
    c[ImGuiCol_TableBorderStrong]    = ImVec4(1.000f, 1.000f, 1.000f, 0.095f);
    c[ImGuiCol_TableBorderLight]     = ImVec4(1.000f, 1.000f, 1.000f, 0.038f);
    c[ImGuiCol_TableRowBg]           = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
    c[ImGuiCol_TableRowBgAlt]        = ImVec4(1.000f, 1.000f, 1.000f, 0.022f);
 
    // ── Seleccion y navegacion ────────────────────────────────────────────────
    c[ImGuiCol_TextSelectedBg]       = ImVec4(0.369f, 0.420f, 1.000f, 0.215f);
    c[ImGuiCol_DragDropTarget]       = ImVec4(0.369f, 0.420f, 1.000f, 0.780f);
    c[ImGuiCol_NavHighlight]         = ImVec4(0.369f, 0.420f, 1.000f, 1.000f);
    c[ImGuiCol_NavWindowingHighlight]= ImVec4(1.000f, 1.000f, 1.000f, 0.580f);
    c[ImGuiCol_NavWindowingDimBg]    = ImVec4(0.000f, 0.000f, 0.000f, 0.440f);
    c[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.000f, 0.000f, 0.000f, 0.540f);
 
    // ── Texto ─────────────────────────────────────────────────────────────────
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
    m_HubMode = true;
}

void UIManager::RequestSettings()
{
    m_ShowConfig = true;
}

// ---------------------------------------------------------------------------
// RenderAll
// ---------------------------------------------------------------------------
void UIManager::RenderAll()
{

     {
        ImGuiIO& io = ImGui::GetIO();

        // F1 — abrir documentacion (misma URL que el menu Ayuda > Documentacion)
        if (ImGui::IsKeyPressed(ImGuiKey_F1, false))
            ProyecThor::External::OpenURL("https://proyecthor.web.app/");

        // Ctrl + P — abrir Preferencias
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_P, false))
            m_ShowConfig = true;

        // Alt + F4 — cerrar ProyecThor
        if (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_F4, false))
            glfwSetWindowShouldClose(m_Window, true);
    }
    // ── Hub de inicio ────────────────────────────────────────────────────────
if (m_HubMode)
    {
        if (m_Hub.Render())
        {
            if (!m_Hub.SettingsRequested())
            {
                m_HubMode     = false;
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

        m_DatabasePanel.Render();   // <-- AGREGAR
        m_WikiPanel.Render();       // <-- AGREGAR
 
        RenderMainMenuBar();
        return;
    }
 

    // ── Workspace normal ─────────────────────────────────────────────────────
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
if (m_FocusControlNextFrame) {
        ImGui::SetWindowFocus(str.control);
        m_FocusControlNextFrame = false;
    }

    // ── Proyector ────────────────────────────────────────────────────────────
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
// Empujar SIEMPRE la config elegida por el usuario hacia el estado
// compartido, para que tambien viaje a los clientes LAN.
if (m_TransitionPanel) {
    Core::PresentationCore::Get().SetTransitionConfig(
        static_cast<int>(m_TransitionPanel->GetCurrentType()),
        m_TransitionPanel->GetDuration());
}

// Disparo por CONTADOR, no por diff de contenido: asi tambien anima
// cuando el slide "nuevo" es identico al anterior (mismo verso repetido).
if (m_TransitionPanel && state.transitionTrigger != m_LastTransitionTrigger)
{
    m_LastTransitionTrigger = state.transitionTrigger;

    // Guardamos TODO el contenido saliente (texto + fondo), no solo el
    // texto, para poder dibujarlo blendeado durante la transicion.
    m_OutgoingText        = m_LastProjectedText;
    m_OutgoingBgColor[0]  = m_LastBgColor[0];
    m_OutgoingBgColor[1]  = m_LastBgColor[1];
    m_OutgoingBgColor[2]  = m_LastBgColor[2];
    m_OutgoingBgWasVideo  = m_LastBgWasVideo;

    m_LastProjectedText = state.currentText;
    m_LastBgColor[0] = state.bgColor[0];
    m_LastBgColor[1] = state.bgColor[1];
    m_LastBgColor[2] = state.bgColor[2];
    m_LastBgWasVideo = (state.bgType == Core::PresentationState::BackgroundType::Video);

m_TransitionPanel->Trigger();
}

// Cada frame, mientras la transicion este activa, empujamos el progreso
// actual hacia BackgroundLayer para que pueda blendear Active/Standby.
if (m_TransitionPanel)
    Core::PresentationCore::Get().SetBackgroundTransitionProgress(
        m_TransitionPanel->IsActive() ? m_TransitionPanel->GetProgress() : 1.0f);

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
                ImDrawList* drawList = ImGui::GetWindowDrawList();
// Fondo de color solido, con crossfade si venimos de otro color solido
if (state.bgType == Core::PresentationState::BackgroundType::SolidColor)
{
    ImU32 colFrom = IM_COL32(
        (int)(m_OutgoingBgColor[0]*255), (int)(m_OutgoingBgColor[1]*255),
        (int)(m_OutgoingBgColor[2]*255), 255);
    ImU32 colTo = IM_COL32(
        (int)(state.bgColor[0]*255), (int)(state.bgColor[1]*255),
        (int)(state.bgColor[2]*255), 255);

    bool transActive = m_TransitionPanel && m_TransitionPanel->IsActive();
    ImU32 finalCol = transActive
        ? ImGui::ColorConvertFloat4ToU32(ImLerp(
              ImGui::ColorConvertU32ToFloat4(colFrom),
              ImGui::ColorConvertU32ToFloat4(colTo),
              m_TransitionPanel->GetProgress()))
        : colTo;

    drawList->AddRectFilled(
        ImVec2((float)mx, (float)my),
        ImVec2((float)(mx + mode->width), (float)(my + mode->height)),
        finalCol);
}
                // Fondo de video
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
                    int qualityW = 0, qualityH = 0;
                    ProyecThor::Settings::ResolveQualityTarget(
                        static_cast<ProyecThor::Settings::OutputQualityMode>(projSettings.outputQualityMode),
                        projSettings.outputPresetIndex, projSettings.outputWidth, projSettings.outputHeight,
                        mode->width, mode->height, qualityW, qualityH);

                    void* texID = Core::PresentationCore::Get().GetProcessedBackgroundTexture(
                        qualityW, qualityH);

                    if (texID) {
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
                }

                // Texto con transicion
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

                        ImU32 shadowCol = IM_COL32(0, 0, 0, static_cast<int>(220.0f * alphaMult));

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

                                    drawList->AddText(activeFont, targetFontSize,
                                        ImVec2(lineX + 3, currentY + 3), shadowCol, line.c_str());
                                    drawList->AddText(activeFont, targetFontSize,
                                        ImVec2(lineX, currentY), col, line.c_str());
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

                            drawList->AddText(activeFont, targetFontSize,
                                ImVec2(textX + 3, textY + 3), shadowCol,
                                text.c_str(), nullptr, boxW);
                            drawList->AddText(activeFont, targetFontSize,
                                ImVec2(textX, textY), col,
                                text.c_str(), nullptr, boxW);
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

                // Anuncios
                {
                    static auto s_AnnLastTime = std::chrono::steady_clock::now();
                    auto        annNow        = std::chrono::steady_clock::now();
                    float       annDt = std::chrono::duration<float>(annNow - s_AnnLastTime).count();
                    s_AnnLastTime = annNow;
                    annDt = std::min(annDt, 0.1f);

                    for (auto& p : m_Panels) {
                        if (p->GetName() == "PreviewPanel") {
                            auto* previewPanel =
                                static_cast<ProyecThor::UI::PreviewPanel*>(p.get());

                            if (previewPanel->m_Announcements.IsLive()) {
                                previewPanel->m_Announcements.RenderOnProjector(
                                    drawList,
                                    (float)mx, (float)my,
                                    (float)mode->width, (float)mode->height,
                                    annDt);
                            }
                            break;
                        }
                    }
                }

                // CapturePanel
                for (auto& p : m_Panels) {
                    if (p->GetName() == "CapturePanel") {
                        auto* capturePanel =
                            static_cast<ProyecThor::UI::CapturePanel*>(p.get());
                        capturePanel->RenderOnProjector(
                            drawList,
                            (float)mx, (float)my,
                            (float)mode->width, (float)mode->height);
                        break;
                    }
                }

                ImGui::End();
            }
        }
    }

    // ── Stage Display (Monitor de Control) ────────────────────────────────────
    if (Core::PresentationCore::Get().IsStageWindowActive())
    {
        int stageMonitorIdx = Core::PresentationCore::Get().GetSecondaryWindowMonitor("stage");
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

                stageDrawList->AddRectFilled(
                    ImVec2((float)smx, (float)smy),
                    ImVec2((float)(smx + stageMode->width), (float)(smy + stageMode->height)),
                    IM_COL32(8, 8, 10, 255));

                const auto& stageSettings =
                    ProyecThor::Settings::SettingsManager::Get().GetSettings().stageDisplay;
                int tmplIdx = std::clamp(stageSettings.layoutTemplateIndex, 0,
                                          ProyecThor::Settings::kStageLayoutTemplateCount - 1);
                const auto& stageTmpl = ProyecThor::Settings::kStageLayoutTemplates[tmplIdx];

                for (int i = 0; i < stageTmpl.cellCount; i++)
                {
                    const float* r = stageTmpl.rect[i];
                    float cx0 = smx + r[0] * stageMode->width;
                    float cy0 = smy + r[1] * stageMode->height;
                    float cw  = r[2] * stageMode->width;
                    float ch  = r[3] * stageMode->height;
                    const float pad = 12.0f;

                    stageDrawList->AddRect(
                        ImVec2(cx0 + pad, cy0 + pad), ImVec2(cx0 + cw - pad, cy0 + ch - pad),
                        IM_COL32(255, 255, 255, 25), 8.0f);

                    auto widget = static_cast<ProyecThor::Settings::StageWidgetType>(
                        std::clamp(stageSettings.cellWidget[i], 0, 3));

                    std::string cellText;
                    ImU32 cellColor = IM_COL32(235, 235, 240, 255);
                    float fontFrac  = 0.16f;
                    ImFont* cellFont = ImGui::GetFont();

                    switch (widget) {
                        case ProyecThor::Settings::StageWidgetType::Clock: {
                            std::time_t now = std::time(nullptr);
                            std::tm lt{};
#ifdef _WIN32
                            localtime_s(&lt, &now);
#else
                            localtime_r(&now, &lt);
#endif
                            char buf[16];
                            std::strftime(buf, sizeof(buf), "%H:%M:%S", &lt);
                            cellText = buf;
                            fontFrac = 0.24f;
                            break;
                        }
                        case ProyecThor::Settings::StageWidgetType::LiveText: {
                            cellText = state.currentText;
                            ImFont* activeFont = Core::PresentationCore::Get().GetImGuiFont(
                                Core::PresentationCore::Get().GetActiveFontName(), ch * fontFrac);
                            if (activeFont) cellFont = activeFont;
                            break;
                        }
                        case ProyecThor::Settings::StageWidgetType::NextLine: {
                            cellText = state.nextText;
                            cellColor = IM_COL32(170, 175, 190, 255);
                            fontFrac  = 0.12f;
                            ImFont* activeFont = Core::PresentationCore::Get().GetImGuiFont(
                                Core::PresentationCore::Get().GetActiveFontName(), ch * fontFrac);
                            if (activeFont) cellFont = activeFont;
                            break;
                        }
                        default:
                            break;
                    }

                    if (!cellText.empty()) {
                        float wrapW    = std::max(10.0f, cw - pad * 4.0f);
                        float fontSize = std::clamp(ch * fontFrac, 14.0f, 140.0f);

                        ImVec2 ts = cellFont->CalcTextSizeA(fontSize, FLT_MAX, wrapW, cellText.c_str());
                        ImVec2 pos = ImVec2(cx0 + (cw - ts.x) * 0.5f, cy0 + (ch - ts.y) * 0.5f);

                        stageDrawList->PushClipRect(ImVec2(cx0, cy0), ImVec2(cx0 + cw, cy0 + ch), true);
                        stageDrawList->AddText(cellFont, fontSize, pos, cellColor,
                                               cellText.c_str(), nullptr, wrapW);
                        stageDrawList->PopClipRect();
                    }
                }

                ImGui::End();
            }
        }
    }

    // Configuracion
    if (m_ShowConfig)
        m_SettingsPanel.Render(&m_ShowConfig);

    // Modal Acerca de
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

    m_DatabasePanel.Render();   // <-- AGREGAR
    m_WikiPanel.Render();       // <-- AGREGAR

    RenderMainMenuBar();
}
// ---------------------------------------------------------------------------
// RenderSocialQrMenu
// Dibuja "Escanea con tu celular" + el QR del link dado + botón para abrirlo
// en el navegador. Pensado para usarse dentro de un ImGui::BeginMenu(...).
// ---------------------------------------------------------------------------
static void RenderSocialQrMenu(const char* url)
{
    ImGui::Text("Escanea con tu celular:");
    ImGui::Spacing();

    // Generar el QR una sola vez por link (cache simple para no recalcular cada frame)
    static const char* s_CachedUrl = nullptr;
    static qrcodegen::QrCode s_CachedQr = qrcodegen::QrCode::encodeText(" ", qrcodegen::QrCode::Ecc::MEDIUM);

    if (s_CachedUrl != url)
    {
        s_CachedQr = qrcodegen::QrCode::encodeText(url, qrcodegen::QrCode::Ecc::MEDIUM);
        s_CachedUrl = url;
    }

    int qrSize = s_CachedQr.getSize();
    float cellSize = 5.0f;           // Tamaño en píxeles de cada cuadradito del QR
    float margin = cellSize * 2.0f;  // Zona de silencio/borde blanco para facilitar lectura
    float totalSize = (qrSize * cellSize) + (margin * 2.0f);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // 1. Fondo blanco (alto contraste para el escáner)
    drawList->AddRectFilled(pos, ImVec2(pos.x + totalSize, pos.y + totalSize), IM_COL32(255, 255, 255, 255));

    // 2. Módulos negros del QR
    for (int y = 0; y < qrSize; y++) {
        for (int x = 0; x < qrSize; x++) {
            if (s_CachedQr.getModule(x, y)) {
                ImVec2 minP(pos.x + margin + (x * cellSize), pos.y + margin + (y * cellSize));
                ImVec2 maxP(pos.x + margin + ((x + 1) * cellSize), pos.y + margin + ((y + 1) * cellSize));
                drawList->AddRectFilled(minP, maxP, IM_COL32(0, 0, 0, 255));
            }
        }
    }

    // 3. Reservar el espacio en el layout de ImGui para evitar encimamientos
    ImGui::Dummy(ImVec2(totalSize, totalSize));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Abrir en el navegador", ImVec2(totalSize, 0)))
        ProyecThor::External::OpenURL(url);
}

// ---------------------------------------------------------------------------
// RenderMainMenuBar
// ---------------------------------------------------------------------------
void UIManager::RenderMainMenuBar()
{
    const auto& str = ProyecThor::UI::GetUIStrings();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(12.0f, 10.0f));
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg,        ImVec4(0.052f, 0.056f, 0.078f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(14.0f, 10.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,             ImVec4(0.860f, 0.880f, 0.940f, 1.0f));

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu(str.menuFile))
        {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.45f, 0.45f, 1.0f));
            if (ImGui::MenuItem("Base de datos"))
    m_DatabasePanel.Open();

if (ImGui::MenuItem("Wiki"))
    ProyecThor::External::OpenURL("https://github.com/TheVixcho/ProyecThor/wiki");
    
ImGui::Spacing();
ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.200f, 0.210f, 0.300f, 0.600f));
ImGui::Separator();
ImGui::PopStyleColor();
ImGui::Spacing();
            if (ImGui::MenuItem(str.menuExit, "Alt+F4"))
                glfwSetWindowShouldClose(m_Window, true);
            ImGui::PopStyleColor();
            ImGui::Spacing();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(str.menuEdit))
        {
            ImGui::Spacing();
            if (ImGui::MenuItem(str.menuPrefs, "Ctrl+P"))
                m_ShowConfig = true;
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

            for (auto& panel : m_Panels) {
                if (panel->GetName() == "PreviewPanel") {
                    auto* previewPanel = static_cast<ProyecThor::UI::PreviewPanel*>(panel.get());
                    ImGui::MenuItem(str.oclockTitle,     NULL, &previewPanel->m_ShowOClock);
                    ImGui::MenuItem(str.quickNotesTitle, NULL, &previewPanel->m_ShowQuickNotes);
                    ImGui::MenuItem("Anuncios",           NULL, &previewPanel->m_ShowAnnouncements);
                    break;
                }
            }
            ImGui::Spacing();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(str.menuHelp))
        {
            ImGui::Spacing();

            if (ImGui::MenuItem(str.menuDocs, "F1"))
                ProyecThor::External::OpenURL("https://proyecthor.web.app/");

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.345f, 0.403f, 0.941f, 1.0f));
            if (ImGui::MenuItem("Reporte de bugs"))
                ProyecThor::External::OpenURL("https://github.com/TheVixcho/ProyecThor/issues");
            ImGui::PopStyleColor();

            // ==========================================
            // NUEVO: MENÚS DE DISCORD Y WHATSAPP CON QR
            // ==========================================

            // --- Canal de Discord ---
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.345f, 0.396f, 0.949f, 1.0f)); // Azul Discord
            bool discordOpen = ImGui::BeginMenu("Canal de Discord");
            ImGui::PopStyleColor();
            if (discordOpen)
            {
                const char* discordUrl = "https://discord.gg/RMk8AGC5pn"; // <-- pon aquí tu invitación de Discord
                RenderSocialQrMenu(discordUrl);
                ImGui::EndMenu();
            }

            // --- Canal de WhatsApp ---
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.145f, 0.827f, 0.400f, 1.0f)); // Verde WhatsApp
            bool whatsappOpen = ImGui::BeginMenu("Canal de WhatsApp");
            ImGui::PopStyleColor();
            if (whatsappOpen)
            {
                const char* whatsappUrl = "https://whatsapp.com/channel/0029Vb7e9tj3WHTdNivIxR19"; // <-- pon aquí tu link de WhatsApp (chat.whatsapp.com/... o wa.me/...)
                RenderSocialQrMenu(whatsappUrl);
                ImGui::EndMenu();
            }
            // ==========================================

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

        ImGui::EndMainMenuBar();
    }

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}
// ---------------------------------------------------------------------------
// BeginDockspace
// ---------------------------------------------------------------------------
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
        ImGuiID dock_right_top, dock_right_bottom;
        ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Down, 0.75f, &dock_right_bottom, &dock_right_top);

        ImGuiID dock_center_right;
        ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.45f, &dock_center_right, &dock_main);
        ImGuiID dock_center_right_top, dock_center_right_bottom;
        ImGui::DockBuilderSplitNode(dock_center_right, ImGuiDir_Down, 0.70f, &dock_center_right_bottom, &dock_center_right_top);

        ImGuiID dock_main_top, dock_main_bottom;
        ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.30f, &dock_main_bottom, &dock_main_top);
ImGui::DockBuilderDockWindow(str.library,          dock_left_top);
ImGui::DockBuilderDockWindow(str.quickNotesTitle,  dock_main_top);
ImGui::DockBuilderDockWindow(str.preview,          dock_main_top);
ImGui::DockBuilderDockWindow(str.oclockTitle,      dock_main_top);
ImGui::DockBuilderDockWindow("Anuncios",           dock_main_top);
ImGui::DockBuilderDockWindow("Vista en Vivo",      dock_right_top);
ImGui::DockBuilderDockWindow("Captura",            dock_main_top);
// Control se registra PRIMERO en dock_right_bottom, para que sea la
// pestaña que abre por defecto sin depender solo del override manual
// de abajo (rightBottomNode->SelectedTabId). Estilos/Fondos/Transiciones
// van despues, sin ninguna prioridad implicita entre ellas.
ImGui::DockBuilderDockWindow(str.control,          dock_right_bottom);
ImGui::DockBuilderDockWindow("Estilos",            dock_right_bottom);
ImGui::DockBuilderDockWindow("Fondos",             dock_right_bottom);
ImGui::DockBuilderDockWindow("Transiciones",       dock_right_bottom);
        ImGui::DockBuilderDockWindow("Transmisión en Red", dock_main_top);

        ImGui::DockBuilderFinish(dockspace_id);

        m_FocusControlNextFrame = true;
        ImGuiDockNode* rightBottomNode = ImGui::DockBuilderGetNode(dock_right_bottom);
        if (rightBottomNode)
            rightBottomNode->SelectedTabId = ImHashStr(str.control, 0, 0);
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

} // namespace ProyecThor::UI