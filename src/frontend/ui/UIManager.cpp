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
#include <imgui_internal.h>
#include "../external/tools/OpenURL.h"
#include "UIStrings.h"
#include "frontend/views/Announcements.h"
#include "frontend/views/Audio.h"
#include "Hub.h"
#include "frontend/panels/StreamingPanel.h"
#include "qrcodegen.hpp"
#include "backend/settings/SettingsManager.h"
#include "backend/settings/ProjectionQualityPresets.h"
#include "AppIcons.h"
#include "IconRail.h"
#include "frontend/panels/home/HomeIcons.h"
#include "biblio/LibraryIcons.h"
#include "LiveContentRenderer.h"
#include "LibrarySongs.h"
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
 
    // ── Redondeo — mas cuadrado, esquinas apenas redondeadas (menos "liquid
    //    glass", mas ProPresenter/OBS). El popup del click derecho en
    //    particular quedaba con Rounding=14 sobre un fondo casi negro y se
    //    veia como una burbuja fuera de lugar.
    s.WindowRounding         =  8.0f;   // era 16
    s.ChildRounding          =  6.0f;   // era 12
    s.FrameRounding          =  6.0f;   // era 9
    s.PopupRounding          =  6.0f;   // era 14
    s.ScrollbarRounding      = 10.0f;   // era 12
    s.GrabRounding           =  6.0f;
    s.TabRounding            =  6.0f;   // era 9
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
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.550f, 0.560f, 0.580f, 0.880f);
 
    // ── Controles ─────────────────────────────────────────────────────────────
    c[ImGuiCol_CheckMark]            = ImVec4(0.700f, 0.700f, 0.720f, 1.000f);
    c[ImGuiCol_SliderGrab]           = ImVec4(0.700f, 0.700f, 0.720f, 1.000f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.720f, 0.730f, 0.750f, 1.000f);
 
    // ── Botones ───────────────────────────────────────────────────────────────
    c[ImGuiCol_Button]               = ImVec4(1.000f, 1.000f, 1.000f, 0.048f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(1.000f, 1.000f, 1.000f, 0.088f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.550f, 0.560f, 0.580f, 1.000f);
 
    // ── Headers (selectables, tree nodes) ────────────────────────────────────
    c[ImGuiCol_Header]               = ImVec4(0.550f, 0.560f, 0.580f, 0.148f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.550f, 0.560f, 0.580f, 0.215f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.550f, 0.560f, 0.580f, 0.375f);
 
    // ── Separadores ───────────────────────────────────────────────────────────
    c[ImGuiCol_Separator]            = ImVec4(1.000f, 1.000f, 1.000f, 0.055f);
    c[ImGuiCol_SeparatorHovered]     = ImVec4(0.550f, 0.560f, 0.580f, 0.380f);
    c[ImGuiCol_SeparatorActive]      = ImVec4(0.550f, 0.560f, 0.580f, 0.780f);
 
    // ── Resize grip ───────────────────────────────────────────────────────────
    c[ImGuiCol_ResizeGrip]           = ImVec4(0.550f, 0.560f, 0.580f, 0.095f);
    c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.550f, 0.560f, 0.580f, 0.360f);
    c[ImGuiCol_ResizeGripActive]     = ImVec4(0.550f, 0.560f, 0.580f, 0.780f);
 
    // ── Tabs ─────────────────────────────────────────────────────────────────
    c[ImGuiCol_Tab]                  = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
    c[ImGuiCol_TabHovered]           = ImVec4(1.000f, 1.000f, 1.000f, 0.068f);
    c[ImGuiCol_TabActive]            = ImVec4(1.000f, 1.000f, 1.000f, 0.108f);
    c[ImGuiCol_TabUnfocused]         = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
    c[ImGuiCol_TabUnfocusedActive]   = ImVec4(1.000f, 1.000f, 1.000f, 0.058f);
 
    // ── Docking ───────────────────────────────────────────────────────────────
    c[ImGuiCol_DockingPreview]       = ImVec4(0.550f, 0.560f, 0.580f, 0.268f);
    c[ImGuiCol_DockingEmptyBg]       = ImVec4(0.026f, 0.029f, 0.044f, 1.000f);
 
    // ── Graficos ──────────────────────────────────────────────────────────────
    c[ImGuiCol_PlotLines]            = ImVec4(0.550f, 0.560f, 0.580f, 1.000f);
    c[ImGuiCol_PlotLinesHovered]     = ImVec4(0.720f, 0.730f, 0.750f, 1.000f);
    c[ImGuiCol_PlotHistogram]        = ImVec4(0.550f, 0.560f, 0.580f, 1.000f);
    c[ImGuiCol_PlotHistogramHovered] = ImVec4(0.720f, 0.730f, 0.750f, 1.000f);
 
    // ── Tablas ────────────────────────────────────────────────────────────────
    c[ImGuiCol_TableHeaderBg]        = ImVec4(1.000f, 1.000f, 1.000f, 0.038f);
    c[ImGuiCol_TableBorderStrong]    = ImVec4(1.000f, 1.000f, 1.000f, 0.095f);
    c[ImGuiCol_TableBorderLight]     = ImVec4(1.000f, 1.000f, 1.000f, 0.038f);
    c[ImGuiCol_TableRowBg]           = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
    c[ImGuiCol_TableRowBgAlt]        = ImVec4(1.000f, 1.000f, 1.000f, 0.022f);
 
    // ── Seleccion y navegacion ────────────────────────────────────────────────
    c[ImGuiCol_TextSelectedBg]       = ImVec4(0.550f, 0.560f, 0.580f, 0.215f);
    c[ImGuiCol_DragDropTarget]       = ImVec4(0.550f, 0.560f, 0.580f, 0.780f);
    c[ImGuiCol_NavHighlight]         = ImVec4(0.550f, 0.560f, 0.580f, 1.000f);
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
    m_Mode = WorkspaceMode::Hub;
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

        // F11 — pantalla completa (menu Ventana > Pantalla completa)
        if (ImGui::IsKeyPressed(ImGuiKey_F11, false))
            ToggleFullscreen();
    }

    // Red/Chat/Streaming corren SIEMPRE, sin importar el WorkspaceMode
    // activo (ver comentario de los getters en UIManager.h) -- si esto
    // dependiera de estar en modo Yggdrasil, una transmision o el chat se
    // pausarian solos apenas el operador volviera a Proyector.
    m_Red.Update();
    m_Chat.Update();
    m_Broadcast.Update();

    // Toolbar de segundo nivel (Hub/Proyector/Streaming/Yggdrasil) — se
    // dibuja siempre, sea cual sea el modo activo, y reduce el area de
    // trabajo del viewport (ver RenderModeToolbar) para que lo que se
    // dibuje despues (Hub, dockspace o Yggdrasil) no quede tapado debajo.
    RenderModeToolbar();

    // ── Yggdrasil (OSC, Red, Chat y Streaming) ──────────────────────────────
    if (m_Mode == WorkspaceMode::Yggdrasil)
    {
        m_YggdrasilPanel.Render();
        RenderMainMenuBar();
        return;
    }

    // ── Biblioteca (ver/gestionar assets, sin proyectar) ────────────────────
    if (m_Mode == WorkspaceMode::Biblioteca)
    {
        m_LibraryManagerPanel.Render();
        RenderMainMenuBar();
        return;
    }

    // ── Biblia (el mismo BibleView de Home, a pantalla completa) ────────────
    if (m_Mode == WorkspaceMode::Biblia)
    {
        m_BiblePanel.Render();
        RenderMainMenuBar();
        return;
    }

    // ── Hub de inicio ────────────────────────────────────────────────────────
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
if (m_FocusViewNextFrame) {
        ImGui::SetWindowFocus("Vista en Vivo");
        m_FocusViewNextFrame = false;
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

// FIX: disparaba con transitionTrigger, que tambien incrementaba con
// cualquier cambio de FONDO/VIDEO — asi que cambiar el fondo animaba el
// texto (aunque no hubiera cambiado) y cambiar el texto ensuciaba el
// color de fondo recordado (m_LastBgColor), que despues aparecia como un
// "flash" de un color sin relacion la proxima vez que el fondo cambiaba
// de verdad. Ahora usa textTransitionTrigger, que SOLO cambia cuando el
// texto (letras/Layer2 o nota rapida) realmente cambia — el fondo/video ya
// tiene su propio crossfade automatico en BackgroundLayer, totalmente
// independiente de esto.
//
// Disparo por CONTADOR, no por diff de contenido: asi tambien anima
// cuando el slide "nuevo" es identico al anterior (mismo verso repetido).
if (m_TransitionPanel && state.textTransitionTrigger != m_LastTransitionTrigger)
{
    m_LastTransitionTrigger = state.textTransitionTrigger;

    std::string outgoing = m_LastProjectedText;
    m_LastProjectedText   = state.currentText;

    // Ni al inicio ni al final de una cancion (texto vacio de un lado o
    // del otro) hay animacion: se corta instantaneo. La transicion solo
    // tiene sentido ENTRE dos lineas reales.
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
                // Le informa a PresentationCore cual ImGuiID es esta viewport
                // en este frame, para que el override de Renderer_RenderWindow
                // en main.cpp sepa cuando desviar el render hacia
                // CompositePostChain (CRT/Grano/FXAA) y cuando no (StageLive,
                // paneles flotantes) — ver PresentationCore::
                // RenderProjectorViewportPostFX.
                Core::PresentationCore::Get().SetProjectorPostFXViewportID(
                    ImGui::GetWindowViewport()->ID);
                ImDrawList* drawList = ImGui::GetWindowDrawList();

// Pantalla de carga (ver Ajustes > Proyeccion > Logo): mientras un fondo o
// la cola de Monitor esta cargando (PresentationCore::
// ShouldShowLoadingScreen), el publico ve el logo configurado en vez de un
// frame entrecortado/desactualizado o texto encima de eso. Reemplaza TODO
// el bloque de fondo+texto de mas abajo, no se dibuja nada mas encima.
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
// Fondo de color solido — corte directo, sin animacion (la transicion
// del operador es solo para el texto, ver el FIX mas arriba).
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
                    auto qualityMode = static_cast<ProyecThor::Settings::OutputQualityMode>(projSettings.outputQualityMode);
                    int qualityW = 0, qualityH = 0;
                    ProyecThor::Settings::ResolveQualityTarget(
                        qualityMode,
                        projSettings.outputPresetIndex, projSettings.outputWidth, projSettings.outputHeight,
                        mode->width, mode->height, qualityW, qualityH);

                    // El PerformanceGovernor solo recorta mas el target cuando el
                    // usuario dejo la calidad en Auto — jamas pisa un preset o
                    // tamano custom elegido a mano (ver PerformanceGovernor.h).
                    if (qualityMode == ProyecThor::Settings::OutputQualityMode::Auto)
                        Core::PerformanceGovernor::Get().ApplyCap(qualityW, qualityH);

                    void* texID = Core::PresentationCore::Get().GetProcessedBackgroundTexture(
                        qualityW, qualityH);

                    if (texID) {
                        // "Rellenado": si el contenido no llena la pantalla
                        // (quedarian barras negras arriba/abajo o a los
                        // costados), se dibuja primero una copia del MISMO
                        // fondo, muy desenfocada, estirada a pantalla
                        // completa -- después el contenido nítido encima,
                        // en su rect real. El rect negro de más arriba
                        // (linea ~418) sigue ahi como base/fallback, asi que
                        // si el blur no esta listo todavia el primer frame
                        // simplemente se ve negro como antes, sin parpadeo.
                        bool hasBars = (destW < (float)mode->width - 0.5f) ||
                                       (destH < (float)mode->height - 0.5f);
                        if (hasBars && Core::PresentationCore::Get().GetFillBlurEnabled()) {
                            void* fillTex = Core::PresentationCore::Get().GetBackgroundFillTexture(
                                qualityW, qualityH);
                            if (fillTex) {
                                // Brillo del relleno (0=negro, 1=el brillo real
                                // del blur) -- ver Ajustes > Shaders >
                                // Rellenado. Tint multiplicativo, no toca el
                                // shader de blur en si.
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

                    // FIX: este es el UNICO rendering real del proyector
                    // publico ("ProjectorLive" es la unica ventana — ver mas
                    // abajo, migrado igual que ya se hizo con Stage). Antes
                    // solo dibujaba GetProcessedBackgroundTexture() (Active()
                    // nada mas, sin nocion de Standby ni de swap), mientras
                    // el crossfade de verdad vivia en BackgroundLayer::Render(),
                    // que corria en una SEGUNDA ventana nativa (SecondaryOutputWindow)
                    // compitiendo por "siempre encima" con esta — el publico
                    // podia ver cualquiera de las dos, sin blend, con el
                    // fondo de la OTRA ventana quedando "sin relacion"
                    // durante el swap. Blendear el standby directo aca es lo
                    // que hace que el cambio de fondo se vea fluido de
                    // verdad en la salida que el publico realmente ve.
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

                // Fondo "now playing" (audio en vivo desde el panel de audio
                // de la biblioteca) — ver PresentationCore::SetBackgroundAudio
                // y AudioPanel::RenderLiveBackground. Se dibuja en ESTE
                // drawlist (ventana "ProjectorLive"), por eso
                // RenderLiveBackground puede usar ImGui::GetWindowDrawList()
                // internamente sin que el caller le pase el drawlist.
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
} // else (!showingLoadingScreen)

                // Anuncios (tampoco se dibujan sobre la pantalla de carga —
                // mismo criterio que el texto en vivo, mas arriba)
                if (!showingLoadingScreen)
                {
                    static auto s_AnnLastTime = std::chrono::steady_clock::now();
                    auto        annNow        = std::chrono::steady_clock::now();
                    float       annDt = std::chrono::duration<float>(annNow - s_AnnLastTime).count();
                    s_AnnLastTime = annNow;
                    annDt = std::min(annDt, 0.1f);

                    for (auto& p : m_Panels) {
                        if (p->GetName() == "Diseño") {
                            // Anuncios/Captura se movieron de Home a este hub
                            // (StylesHubPanel, ver Diseño > Anuncios/Captura) —
                            // ninguno de los dos es un IPanel propio, se llega
                            // a ellos igual que antes se llegaba via HomePanel.
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

    // ── Stage Display (Monitor de Control) ────────────────────────────────────
    // Nota: el Stage fisico ya NO usa la ventana secundaria cruda
    // (CreateStageWindow/RenderStageContent, que solo pintaba gris) — este
    // viewport ImGui es la unica ventana real para el Stage, gateado por
    // state.isStaging/state.stageMonitorIndex en vez de IsStageWindowActive(),
    // para no competir por z-order con ninguna otra ventana sobre el mismo monitor.
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

    // Configuracion
    if (m_ShowConfig)
        m_SettingsPanel.Render(&m_ShowConfig);

    // Rendimiento (menu Vista)
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
// ToggleFullscreen — menu Ventana > Pantalla completa. glfwSetWindowMonitor
// no recuerda la geometria "windowed" previa, asi que se guarda a mano en
// m_WindowedX/Y/W/H antes de pasar a pantalla completa, para poder
// restaurarla al volver a modo ventana.
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// RenderModeToolbar
// ---------------------------------------------------------------------------
// Segunda toolbar, debajo del menu principal: 4 iconos que cambian el modo
// completo del workspace (ver WorkspaceMode en UIManager.h) — no son
// paneles dockeados, cada uno reemplaza TODO lo que se dibuja despues.
// Reduce manualmente el area de trabajo del viewport (mismo mecanismo que
// usa ImGui::BeginMainMenuBar internamente) para que el Hub/dockspace/
// Yggdrasil que se dibuje a continuacion no quede tapado por esta barra.
void UIManager::RenderModeToolbar()
{
    // Opcional (menu Vista > "Barra de modos...") y apagada por default:
    // si esta apagada no se dibuja nada ni se reserva espacio -- el
    // comportamiento queda identico al de antes de que esta barra existiera.
    auto& general = ProyecThor::Settings::SettingsManager::Get().GetSettings().general;
    if (!general.showModeToolbar) return;

    static const IconRailItem kItems[] = {
        { (int)WorkspaceMode::Hub,        HomeIcons::DrawIcon_Home,      "Hub"        },
        { (int)WorkspaceMode::Projector,  AppIcons::DrawIcon_Monitor,    "Proyector"  },
        { (int)WorkspaceMode::Yggdrasil,  AppIcons::DrawIcon_Yggdrasil,  "Yggdrasil"  },
        { (int)WorkspaceMode::Biblioteca, AppIcons::DrawIcon_Layers,     "Biblioteca" },
        { (int)WorkspaceMode::Biblia,     Library::DrawIcon_Cross,       "Biblia"     },
    };
    static const float kColors[5][4] = {
        { 0.55f, 0.60f, 0.68f, 1.0f }, // Hub
        { 0.31f, 0.55f, 1.00f, 1.0f }, // Proyector
        { 0.65f, 0.31f, 0.94f, 1.0f }, // Yggdrasil
        { 0.35f, 0.80f, 0.55f, 1.0f }, // Biblioteca
        { 0.86f, 0.67f, 0.16f, 1.0f }, // Biblia
    };

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
    ImGui::Begin("##ModeToolbar", nullptr, flags);
    ImGui::PopStyleVar();

    int currentIndex = (int)m_Mode;
    RenderIconRail(kItems, 5, currentIndex, IconRailOrientation::Horizontal, kColors);
    WorkspaceMode newMode = (WorkspaceMode)currentIndex;
    if (newMode != m_Mode)
    {
        m_Mode = newMode;
        if (m_Mode == WorkspaceMode::Hub)       m_Hub.ForceOpen();
        if (m_Mode == WorkspaceMode::Projector) m_ResetLayout = true;
    }

    ImGui::End();

    // Reserva el alto de esta barra para lo que se dibuje despues en el
    // mismo frame (Hub/dockspace/Yggdrasil ya leen vp->WorkPos/WorkSize).
    vp->WorkPos.y  += railH;
    vp->WorkSize.y -= railH;
}

// ---------------------------------------------------------------------------
// RenderMainMenuBar
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// RenderQuickSwitcher
// ---------------------------------------------------------------------------
// Alt+Espacio: paleta flotante para saltar entre las 5 secciones del
// workspace (Hub/Proyector/Streaming/Yggdrasil/Biblioteca) con las flechas
// + Enter, sin depender de que la toolbar de modos este visible (ver
// GeneralSettings::showModeToolbar) — funciona igual este prendida o no.
void UIManager::RenderQuickSwitcher()
{
    struct QSItem { WorkspaceMode mode; DrawIconFn icon; const char* label; };
    static const QSItem kItems[] = {
        { WorkspaceMode::Hub,        HomeIcons::DrawIcon_Home,      "Hub"        },
        { WorkspaceMode::Projector,  AppIcons::DrawIcon_Monitor,    "Proyector"  },
        { WorkspaceMode::Yggdrasil,  AppIcons::DrawIcon_Yggdrasil,  "Yggdrasil"  },
        { WorkspaceMode::Biblioteca, AppIcons::DrawIcon_Layers,     "Biblioteca" },
        { WorkspaceMode::Biblia,     Library::DrawIcon_Cross,       "Biblia"     },
    };
    constexpr int kCount = 5;

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
    if (!m_QuickSwitchOpen) return; // Enter/Escape ya lo cerraron este mismo frame

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

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(12.0f, 10.0f));
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg,        ImVec4(0.052f, 0.056f, 0.078f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(14.0f, 10.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,             ImVec4(0.860f, 0.880f, 0.940f, 1.0f));

    if (ImGui::BeginMainMenuBar())
    {
        // ── Menu "ProyecThor" (nombre de la app, siempre primero) ───────────────
        // Preferencias (movida desde Editar, que se elimino por quedar con
        // un solo item) y Salir (movida desde Archivo).
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

            // "Base de datos"/"Wiki" se mudaron a Ayuda y "Salir" al menu
            // "ProyecThor" de arriba — Archivo ahora es solo la categoria
            // Importar.
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

            // Afecta a los 4 rails de iconos (Biblioteca/Home/Control/Diseño):
            // con el titulo apagado quedan solo-icono y ocupan menos espacio.
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

            // Riel de "Limpiar <tipo>" a la derecha del video en Vista en
            // Vivo — opcional para operadores que prefieren mas ancho para
            // el video en vez de los botones especificos.
            if (ImGui::MenuItem("Botones de limpieza (Vista en Vivo)", nullptr, general.showViewQuickActions))
            {
                general.showViewQuickActions = !general.showViewQuickActions;
                ProyecThor::Settings::SettingsManager::Get().Save();
            }

            // Toolbar de modos (Hub/Proyector/Streaming/Yggdrasil/Biblioteca)
            // — opcional y apagada por default, ver GeneralSettings::showModeToolbar.
            if (ImGui::MenuItem("Barra de modos (Streaming/Yggdrasil/Biblioteca)", nullptr, general.showModeToolbar))
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

            // Movidos aca desde Archivo (reorganizacion del menu).
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

            // --- Canal de YouTube ---
            // TODO: falta el link real del canal de YouTube — queda
            // deshabilitado (visible pero sin accion) hasta tenerlo, para no
            // inventar una URL. Una vez que se pase el link, cambiar por el
            // mismo patron BeginMenu+RenderSocialQrMenu que Discord/WhatsApp
            // arriba (o un MenuItem+OpenURL simple si no hace falta QR).
            ImGui::BeginDisabled(true);
            ImGui::MenuItem("Canal de YouTube");
            ImGui::EndDisabled();
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

        // "Control" se elimino (ver ControlPanel, ahora en Ajustes >
        // Proyeccion/Stage). dock_right ahora SI se divide de nuevo: arriba
        // "Vista en Vivo" (video + transporte), abajo "Herramientas" (Control
        // Overlays/Red/Notas/Reloj, ver ViewToolsPanel.cpp) — antes Control
        // Overlays vivia dentro de Vista en Vivo y Red/Notas/Reloj eran
        // secciones de Home; el operador las queria "al lado del video", asi
        // que ahora tienen su propio hub debajo, en vez de mezcladas con la
        // biblioteca de contenido (Home) o apretadas dentro del video.
        ImGuiID dock_right;
        ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.37f, &dock_right, &dock_main);
        ImGuiID dock_right_top, dock_right_bottom;
        // 0.40 (antes 0.30): Herramientas (Overlays/Red/Notas/Reloj/Chat) le
        // pedia mas alto — con el chat el contenido de esa pestaña dejo de
        // ser un par de checkboxes y paso a necesitar espacio de verdad
        // (QR + log de mensajes + composer).
        ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Down, 0.40f, &dock_right_bottom, &dock_right_top);

        ImGuiID dock_center_right;
        ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.45f, &dock_center_right, &dock_main);
        ImGuiID dock_center_right_top, dock_center_right_bottom;
        ImGui::DockBuilderSplitNode(dock_center_right, ImGuiDir_Down, 0.70f, &dock_center_right_bottom, &dock_center_right_top);

        ImGuiID dock_main_top, dock_main_bottom;
        ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.30f, &dock_main_bottom, &dock_main_top);
ImGui::DockBuilderDockWindow(str.library,          dock_left_top);
// Home reemplaza a los 6 paneles sueltos que antes vivian aca como pestañas
// nativas de ImGui (Preview/OClock/Anuncios/Notas Rapidas/Captura/
// Transmision en Red) — ahora son secciones de un sidebar de iconos dentro
// de un unico panel "Home" (ver HomePanel.cpp).
ImGui::DockBuilderDockWindow("Home",               dock_main_top);
ImGui::DockBuilderDockWindow("Vista en Vivo",      dock_right_top);
ImGui::DockBuilderDockWindow("Herramientas",       dock_right_bottom);
// "Diseño" es el hub de Fondos + Estilos + Overlays + Transiciones +
// Anuncios + Captura (rail a la izquierda, ver StylesHubPanel.cpp) — antes
// pestañas nativas de ImGui separadas, ahora un único panel.
        ImGui::DockBuilderDockWindow("Diseño",              dock_main_bottom);

{
    ImGuiID leafNodes[] = {
        dock_left_top, dock_left_bottom,
        dock_main_top, dock_main_bottom,
        dock_right_top, dock_right_bottom
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

} // namespace ProyecThor::UI