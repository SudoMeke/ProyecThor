#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#define STB_IMAGE_IMPLEMENTATION
#include "frontend/panels/stb_image.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#ifdef _WIN32
#include <dwmapi.h>
#endif
#include <memory>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <iostream>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <filesystem>
#include <cstdlib>
#include <fstream>
#include "frontend/ui/bin/StyleGeneralApp.h"
#include "Version.h"
#include "SettingsManager.h"
#include "PresentationCore.h"
#include "PerformanceGovernor.h"
#include "SystemStats.h"
#include "ui/UIManager.h"
#include "frontend/panels/LibraryPanel.h"
#include "frontend/panels/overlay/OverlayExportService.h"
#include "frontend/panels/HomePanel.h"
#include "frontend/ui/Hub.h"
#include "frontend/panels/ViewPanel.h"
#include "frontend/panels/StylesHubPanel.h"
#include "SplashScreen.h"

#ifdef _WIN32
    #pragma comment(lib, "dwmapi.lib")
#endif

namespace {

constexpr int kSplashWBase = 600;
constexpr int kSplashHBase = 380;
constexpr int kMainWBase   = 1280;
constexpr int kMainHBase   = 720;

}

std::string GetAppDataFilePath(const std::string& filename)
{
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    if (!appData) return filename;
    std::filesystem::path dirPath = std::filesystem::path(appData) / "ProyecThor";
#else
    const char* home = std::getenv("HOME");
    if (!home) return filename;
    std::filesystem::path dirPath = std::filesystem::path(home) / ".config" / "ProyecThor";
#endif

    if (!std::filesystem::exists(dirPath))
        std::filesystem::create_directories(dirPath);

    return (dirPath / filename).string();
}

GLuint LoadTextureFromFile(const char* filename)
{
    int w = 0, h = 0, ch = 0;
    unsigned char* data = stbi_load(filename, &w, &h, &ch, 4);
    if (!data)
    {
        std::cerr << "[DIAG] LoadTextureFromFile: no se pudo cargar '" << filename << "'\n";
        return 0;
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);
    return tex;
}

#ifdef _WIN32
static void EnableDpiAwareness()
{
    HMODULE user32 = LoadLibraryA("user32.dll");
    if (user32)
    {
        using SetCtxFn = BOOL(WINAPI*)(HANDLE);
        auto setCtx = (SetCtxFn)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        if (setCtx && setCtx((HANDLE)(-4)))
        {
            FreeLibrary(user32);
            return;
        }
        FreeLibrary(user32);
    }

    HMODULE shcore = LoadLibraryA("shcore.dll");
    if (shcore)
    {
        using SetAwarenessFn = HRESULT(WINAPI*)(int);
        auto setAwareness = (SetAwarenessFn)GetProcAddress(shcore, "SetProcessDpiAwareness");
        if (setAwareness && SUCCEEDED(setAwareness(2)))
        {
            FreeLibrary(shcore);
            return;
        }
        FreeLibrary(shcore);
    }

    HMODULE user32b = LoadLibraryA("user32.dll");
    if (user32b)
    {
        using SetDpiAwareFn = BOOL(WINAPI*)();
        auto setDpiAware = (SetDpiAwareFn)GetProcAddress(user32b, "SetProcessDPIAware");
        if (setDpiAware) setDpiAware();
        FreeLibrary(user32b);
    }
}
#endif

namespace FrameProfiler
{
    static constexpr int kSampleFrames = 60;

    struct Accum
    {
        double totalMs = 0.0;
        int    count   = 0;
    };

    static Accum s_PollEvents;
    static Accum s_CoreUpdate;
    static Accum s_ImGuiBuild;
    static Accum s_ImGuiRender;
    static Accum s_PlatformWindows;
    static Accum s_SwapBuffers;
    static Accum s_FrameTotal;

    using Clock = std::chrono::steady_clock;

    static double ElapsedMs(Clock::time_point start)
    {
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    }

    static void Add(Accum& a, double ms)
    {
        a.totalMs += ms;
        a.count++;
    }

    static void ReportIfReady()
    {
        if (s_FrameTotal.count < kSampleFrames)
            return;

        auto avg = [](const Accum& a) { return a.totalMs / a.count; };
        double fps = 1000.0 / avg(s_FrameTotal);

        std::cout << "\n[PROFILE] Promedio ultimos " << kSampleFrames << " frames"
                  << " (fps estimado: " << fps << ")\n"
                  << "  PollEvents        : " << avg(s_PollEvents)       << " ms\n"
                  << "  core.Update()     : " << avg(s_CoreUpdate)       << " ms\n"
                  << "  ImGui build       : " << avg(s_ImGuiBuild)       << " ms\n"
                  << "  ImGui render(GL)  : " << avg(s_ImGuiRender)      << " ms\n"
                  << "  PlatformWindows   : " << avg(s_PlatformWindows)  << " ms\n"
                  << "  SwapBuffers       : " << avg(s_SwapBuffers)      << " ms\n"
                  << "  TOTAL frame       : " << avg(s_FrameTotal)       << " ms\n";

        s_PollEvents      = {};
        s_CoreUpdate      = {};
        s_ImGuiBuild      = {};
        s_ImGuiRender     = {};
        s_PlatformWindows = {};
        s_SwapBuffers     = {};
        s_FrameTotal      = {};
    }
}

namespace {

float DetectDpiScale()
{
    float scale = 1.0f;
    GLFWmonitor* primary = glfwGetPrimaryMonitor();
    if (primary)
    {
        float sx = 1.0f, sy = 1.0f;
        glfwGetMonitorContentScale(primary, &sx, &sy);
        if (sx > 0.0f) scale = sx;
    }
    std::cerr << "[DIAG] Escala de DPI detectada: " << (scale * 100.0f) << "%\n";
    return scale;
}

GLFWwindow* CreateSplashWindow(int splashW, int splashH)
{
    glfwWindowHint(GLFW_DECORATED,             GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING,              GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE,             GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_VISIBLE,               GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(splashW, splashH, "ProyecThor", nullptr, nullptr);
    if (!window)
    {
        const char* desc = nullptr;
        int code = glfwGetError(&desc);
        std::cerr << "[DIAG] FALLO: glfwCreateWindow(splash) devolvio nullptr. "
                  << "Codigo GLFW: " << code << " Descripcion: " << (desc ? desc : "N/A") << "\n";
        return nullptr;
    }
    std::cerr << "[DIAG] splashWindow creado OK\n";

    GLFWmonitor* primary = glfwGetPrimaryMonitor();
    const GLFWvidmode* vm = glfwGetVideoMode(primary);
    if (vm)
    {
        int mx = 0, my = 0;
        glfwGetMonitorPos(primary, &mx, &my);
        glfwSetWindowPos(window, mx + (vm->width - splashW) / 2, my + (vm->height - splashH) / 2);
    }
    return window;
}

ProyecThor::Splash::Fonts LoadSplashFonts(ImGuiIO& io, float dpiScale)
{
    using namespace ProyecThor::Settings;

    std::string fontPath;
    const std::string& customFontPath = SettingsManager::Get().GetSettings().theme.customFontPath;
    const char* defaultFontPath = "bin/assets/fonts/OpenSans-Regular.ttf";

    if (!customFontPath.empty() && IsValidFontFile(customFontPath))
        fontPath = customFontPath;
    else if (IsValidFontFile(defaultFontPath))
        fontPath = defaultFontPath;

    ProyecThor::Splash::Fonts fonts;
    if (!fontPath.empty())
    {
        fonts.title   = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 46.0f * dpiScale);
        fonts.regular = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 20.0f * dpiScale);
        fonts.small   = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f * dpiScale);
    }

    if (!fonts.title || !fonts.regular || !fonts.small)
        std::cerr << "[DIAG] ADVERTENCIA: no se pudo cargar la fuente en '" << fontPath
                  << "'. Verifica que el binario se ejecute desde el directorio correcto "
                  << "(donde existe bin/assets/...).\n";

    return fonts;
}

void LoadUIIcons()
{
    StyleGeneralApp::LoadAppIcon("search",            "bin/assets/icons/ui/searchico.png");
    StyleGeneralApp::LoadAppIcon("izquierda",         "bin/assets/icons/ui/izquierda.png");
    StyleGeneralApp::LoadAppIcon("editar",            "bin/assets/icons/ui/editar.png");
    StyleGeneralApp::LoadAppIcon("play",              "bin/assets/icons/ui/play.png");
    StyleGeneralApp::LoadAppIcon("pause",             "bin/assets/icons/ui/pause.png");
    StyleGeneralApp::LoadAppIcon("add",               "bin/assets/icons/ui/add.png");
    StyleGeneralApp::LoadAppIcon("delete",            "bin/assets/icons/ui/delete.png");
    StyleGeneralApp::LoadAppIcon("skip_next",         "bin/assets/icons/ui/skip_next.png");
    StyleGeneralApp::LoadAppIcon("skip_prev",         "bin/assets/icons/ui/skip_previous.png");
    StyleGeneralApp::LoadAppIcon("forward_10",        "bin/assets/icons/ui/forward_10.png");
    StyleGeneralApp::LoadAppIcon("replay_10",         "bin/assets/icons/ui/replay_10.png");
    StyleGeneralApp::LoadAppIcon("volume_up",         "bin/assets/icons/ui/volume_up.png");
    StyleGeneralApp::LoadAppIcon("no_sound",          "bin/assets/icons/ui/no_sound.png");
    StyleGeneralApp::LoadAppIcon("favorite",          "bin/assets/icons/ui/favorite.png");
    StyleGeneralApp::LoadAppIcon("fit_screen",        "bin/assets/icons/ui/fit_screen.png");
    StyleGeneralApp::LoadAppIcon("arrow_forward",     "bin/assets/icons/ui/arrow_forward.png");
    StyleGeneralApp::LoadAppIcon("arrow_back",        "bin/assets/icons/ui/arrow_back.png");
    StyleGeneralApp::LoadAppIcon("repeat",            "bin/assets/icons/ui/repeat.png");
    StyleGeneralApp::LoadAppIcon("repeat_one",        "bin/assets/icons/ui/repeat_one.png");
    StyleGeneralApp::LoadAppIcon("stop",              "bin/assets/icons/ui/stop.png");
    StyleGeneralApp::LoadAppIcon("motion_play",       "bin/assets/icons/ui/motion_play.png");
    StyleGeneralApp::LoadAppIcon("cleaning_services", "bin/assets/icons/ui/cleaning_services.png");
    StyleGeneralApp::LoadAppIcon("add_to_queue",      "bin/assets/icons/ui/add_to_queue.png");
    StyleGeneralApp::LoadAppIcon("original_screen",   "bin/assets/icons/ui/original_screen.png");
    StyleGeneralApp::LoadAppIcon("add_photo",         "bin/assets/icons/ui/add_photo.png");
    StyleGeneralApp::LoadAppIcon("upload_file",       "bin/assets/icons/ui/upload_file.png");
    StyleGeneralApp::LoadAppIcon("history",           "bin/assets/icons/ui/history.png");
    StyleGeneralApp::LoadAppIcon("cards_star",        "bin/assets/icons/ui/cards_star.png");
}

GLFWwindow* CreateMainWindow(GLFWwindow* splashWindow, int mainW, int mainH)
{
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_RESIZABLE,             GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE,               GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(mainW, mainH, "ProyecThor", nullptr, splashWindow);
    if (!window)
    {
        const char* desc = nullptr;
        int code = glfwGetError(&desc);
        std::cerr << "[DIAG] glfwCreateWindow(main, GL 3.3) fallo. Codigo GLFW: " << code
                  << " Descripcion: " << (desc ? desc : "N/A") << ". Reintentando con GL 3.0...\n";

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        window = glfwCreateWindow(mainW, mainH, "ProyecThor", nullptr, splashWindow);
    }
    if (!window)
    {
        const char* desc = nullptr;
        int code = glfwGetError(&desc);
        std::cerr << "[DIAG] FALLO DEFINITIVO: glfwCreateWindow(main) devolvio nullptr. "
                  << "Codigo GLFW: " << code << " Descripcion: " << (desc ? desc : "N/A") << "\n";
        return nullptr;
    }
    std::cerr << "[DIAG] mainWindow creado OK\n";

#ifdef _WIN32
    {
        HWND hwnd = glfwGetWin32Window(window);
        BOOL useDarkMode = TRUE;
        DwmSetWindowAttribute(hwnd, 20, &useDarkMode, sizeof(useDarkMode));
    }
#endif
    {
        GLFWimage images[1];
        images[0].pixels = stbi_load("proyecthor.png", &images[0].width, &images[0].height, 0, 4);
        if (images[0].pixels)
        {
            glfwSetWindowIcon(window, 1, images);
            stbi_image_free(images[0].pixels);
        }
        else
        {
            std::cerr << "[DIAG] ADVERTENCIA: no se pudo cargar 'proyecthor.png' para el icono de ventana.\n";
        }
    }

    glfwMakeContextCurrent(window);
    ProyecThor::Core::PresentationCore::Get().SetMainWindow(window);
    glfwSwapInterval(1);
    glewExperimental = GL_TRUE;
    GLenum status = glewInit();
    if (status != GLEW_OK)
        std::cerr << "[DIAG] ADVERTENCIA: glewInit() para mainWindow devolvio error: "
                  << glewGetErrorString(status) << "\n";

    return window;
}

void InstallViewportRenderHook()
{
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    static void (*s_OrigCreateWindow)(ImGuiViewport*) = platform_io.Platform_CreateWindow;

    platform_io.Platform_CreateWindow = [](ImGuiViewport* viewport)
    {
        s_OrigCreateWindow(viewport);
        GLFWwindow* w = static_cast<GLFWwindow*>(viewport->PlatformHandle);
        if (w)
        {
            GLFWwindow* backup = glfwGetCurrentContext();
            glfwMakeContextCurrent(w);
            glfwSwapInterval(0);
            glfwMakeContextCurrent(backup);
        }
    };

    static void (*s_OrigRenderWindow)(ImGuiViewport*, void*) = platform_io.Renderer_RenderWindow;

    platform_io.Renderer_RenderWindow = [](ImGuiViewport* viewport, void* renderArg)
    {
        auto& core = ProyecThor::Core::PresentationCore::Get();
        if (core.IsProjectorPostFXViewport(viewport->ID))
            core.RenderProjectorViewportPostFX(viewport, s_OrigRenderWindow);
        else if (s_OrigRenderWindow)
            s_OrigRenderWindow(viewport, renderArg);
    };
}

void RunMainLoop(GLFWwindow* window, ProyecThor::UI::UIManager& uiManager,
                  const ProyecThor::Settings::ThemeSettings& theme)
{
    while (!glfwWindowShouldClose(window))
    {
        using Clock = FrameProfiler::Clock;
        auto frameStart = Clock::now();

        auto t0 = Clock::now();
        glfwPollEvents();
        FrameProfiler::Add(FrameProfiler::s_PollEvents, FrameProfiler::ElapsedMs(t0));

        auto& core = ProyecThor::Core::PresentationCore::Get();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            core.SetProjecting(false);
            core.ClearLayer2();
        }

        if (ProyecThor::Settings::SettingsManager::Get().IsRestartRequested())
            glfwSetWindowShouldClose(window, GLFW_TRUE);

        auto t1 = Clock::now();
        core.Update();
        FrameProfiler::Add(FrameProfiler::s_CoreUpdate, FrameProfiler::ElapsedMs(t1));
        core.RenderAllSecondaryWindows();

        int fw, fh;
        glfwGetFramebufferSize(window, &fw, &fh);
        if (fw == 0 || fh == 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        uiManager.GetGlassRenderer().Resize(fw, fh);
        glViewport(0, 0, fw, fh);
        glClearColor(theme.base[0], theme.base[1], theme.base[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        auto t2 = Clock::now();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        uiManager.RenderAll();

        ImGui::Render();
        FrameProfiler::Add(FrameProfiler::s_ImGuiBuild, FrameProfiler::ElapsedMs(t2));

        ProyecThor::UI::OverlayExportService::Get().ProcessPending();

        auto t3 = Clock::now();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        FrameProfiler::Add(FrameProfiler::s_ImGuiRender, FrameProfiler::ElapsedMs(t3));

        uiManager.GetGlassRenderer().CaptureCurrentFrame();
        uiManager.GetGlassRenderer().Blur(1.0f, 1);

        auto t4 = Clock::now();
        {
            GLFWwindow* ctxBackup = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(ctxBackup);
        }
        FrameProfiler::Add(FrameProfiler::s_PlatformWindows, FrameProfiler::ElapsedMs(t4));

        auto t5 = Clock::now();
        glfwSwapBuffers(window);
        FrameProfiler::Add(FrameProfiler::s_SwapBuffers, FrameProfiler::ElapsedMs(t5));

        double frameTotalMs = FrameProfiler::ElapsedMs(frameStart);
        FrameProfiler::Add(FrameProfiler::s_FrameTotal, frameTotalMs);
        FrameProfiler::ReportIfReady();

        ProyecThor::Core::PerformanceGovernor::Get().ReportFrame(frameTotalMs);
        ProyecThor::Core::SystemStats::Get().Update();
    }
}

}

int main()
{
    std::cerr << "[DIAG] Iniciando main()\n";

#ifdef _WIN32
    EnableDpiAwareness();
#endif

#ifndef _WIN32
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    std::cerr << "[DIAG] Forzando backend GLFW a X11/XWayland (necesario para GLEW)\n";
#endif

    if (!glfwInit())
    {
        std::cerr << "[DIAG] FALLO: glfwInit() devolvio false\n";
        return -1;
    }
    std::cerr << "[DIAG] glfwInit() OK\n";

    const float dpiScale = DetectDpiScale();
    const int splashW = (int)(kSplashWBase * dpiScale);
    const int splashH = (int)(kSplashHBase * dpiScale);
    const int mainW   = (int)(kMainWBase   * dpiScale);
    const int mainH   = (int)(kMainHBase   * dpiScale);

    ProyecThor::Settings::SettingsManager::Get().LoadSettings();
    std::cerr << "[DIAG] SettingsManager::LoadSettings() OK\n";
    auto& theme = ProyecThor::Settings::SettingsManager::Get().GetSettings().theme;

    GLFWwindow* splashWindow = CreateSplashWindow(splashW, splashH);
    if (!splashWindow)
    {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(splashWindow);
    glfwSwapInterval(1);
    glewExperimental = GL_TRUE;
    GLenum glewStatus = glewInit();
    if (glewStatus != GLEW_OK)
    {
        std::cerr << "[DIAG] FALLO: glewInit() devolvio error: " << glewGetErrorString(glewStatus) << "\n";
        glfwTerminate();
        return -1;
    }
    std::cerr << "[DIAG] glewInit() OK. Version OpenGL: " << (const char*)glGetString(GL_VERSION) << "\n";

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& splashIO = ImGui::GetIO();
    splashIO.IniFilename = nullptr;

    ProyecThor::Splash::Fonts splashFonts = LoadSplashFonts(splashIO, dpiScale);

    ImGui_ImplGlfw_InitForOpenGL(splashWindow, true);
    ImGui_ImplOpenGL3_Init("#version 130");
    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(dpiScale);
    std::cerr << "[DIAG] ImGui inicializado para splashWindow OK\n";

    const ProyecThor::Splash::Art splashArt = ProyecThor::Splash::PickArt();
    const std::string creditText = "Illustration by: " + splashArt.author;

    GLuint logoTex = LoadTextureFromFile("proyecthor.png");
    GLuint bgTex   = LoadTextureFromFile(splashArt.filename.c_str());

    GLFWwindow* mainWindow = nullptr;
    const ImVec2 splashSize((float)splashW, (float)splashH);

    const std::vector<ProyecThor::Splash::Step> steps = {
        { "Inicializando motor grafico OpenGL...", [&](){
            mainWindow = CreateMainWindow(splashWindow, mainW, mainH);
        }},

        { "Cargando iconos y recursos graficos...", [&](){
            if (!mainWindow) return;
            glfwMakeContextCurrent(mainWindow);
            LoadUIIcons();
        }},

        { "Listo", [](){} },
    };

    for (int i = 0; i < (int)steps.size(); ++i)
        ProyecThor::Splash::RunStep(steps[i], i, (int)steps.size(), splashWindow, splashSize,
            logoTex, bgTex, splashFonts, creditText, theme);

    if (!mainWindow)
    {
        std::cerr << "[DIAG] FALLO: mainWindow sigue siendo nullptr despues del loop de carga. "
                  << "Revisa los mensajes [DIAG] anteriores para ver donde fallo la creacion "
                  << "de la ventana principal.\n";
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(splashWindow);
        glfwTerminate();
        return -1;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (logoTex != 0) glDeleteTextures(1, &logoTex);
    if (bgTex   != 0) glDeleteTextures(1, &bgTex);

    glfwMakeContextCurrent(mainWindow);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename  = "proyecthor_ui.ini";

    {
        using namespace ProyecThor::Settings;
        const std::string& customFontPath = SettingsManager::Get().GetSettings().theme.customFontPath;
        const char* defaultFontPath = "bin/assets/fonts/OpenSans-Regular.ttf";

        std::string fontToLoad;
        if (!customFontPath.empty() && IsValidFontFile(customFontPath))
            fontToLoad = customFontPath;
        else if (IsValidFontFile(defaultFontPath))
            fontToLoad = defaultFontPath;

        if (!fontToLoad.empty())
            io.Fonts->AddFontFromFileTTF(fontToLoad.c_str(), 16.0f * dpiScale);
    }
    ProyecThor::Core::PresentationCore::Get().LoadFontsIntoImGui();

    ImGui_ImplGlfw_InitForOpenGL(mainWindow, true);
    ImGui_ImplOpenGL3_Init("#version 130");
    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(dpiScale);

    InstallViewportRenderHook();

    ProyecThor::Settings::SettingsManager::Get().ApplyTheme();

    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding              = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    {
        int monitorCount = 0;
        glfwGetMonitors(&monitorCount);
        auto& settings = ProyecThor::Settings::SettingsManager::Get().GetSettings();
        if (settings.projection.targetMonitor == -1)
            settings.projection.targetMonitor = (monitorCount > 1) ? 1 : 0;
    }

    std::cerr << "[DIAG] Antes de uiManager.Initialize()\n";
    ProyecThor::UI::UIManager uiManager;
    if (!uiManager.Initialize(mainWindow))
    {
        std::cerr << "[DIAG] FALLO: uiManager.Initialize(mainWindow) devolvio false\n";
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(mainWindow);
        glfwDestroyWindow(splashWindow);
        glfwTerminate();
        return -1;
    }
    std::cerr << "[DIAG] uiManager.Initialize() OK\n";

    ProyecThor::Settings::SettingsManager::Get().ApplyProjection();

    auto homePanel    = std::make_shared<ProyecThor::UI::HomePanel>();
    auto libraryPanel = std::make_shared<ProyecThor::UI::LibraryPanel>();
    homePanel->SetAudioPanel(libraryPanel->GetAudioPanel());
    ProyecThor::Core::PresentationCore::Get().SetAudioPanelRef(libraryPanel->GetAudioPanel());
    homePanel->m_UIManagerRef = &uiManager;
    libraryPanel->SetUIManager(&uiManager);

    uiManager.AddPanel(libraryPanel);
    uiManager.AddPanel(homePanel);

    auto viewPanel = std::make_shared<ProyecThor::UI::ViewPanel>(&uiManager);
    viewPanel->SetTeamChatPanelRef(&uiManager.GetChatPanel());
    uiManager.AddPanel(viewPanel);

    auto stylesHub = std::make_shared<ProyecThor::UI::StylesHubPanel>(&uiManager);
    stylesHub->SetTransitionPanel(uiManager.GetTransitionPanelOwned().get());
    uiManager.AddPanel(stylesHub);
    std::cerr << "[DIAG] Todos los paneles agregados OK\n";

    {
        int fw, fh;
        glfwGetFramebufferSize(mainWindow, &fw, &fh);
        if (fw > 0 && fh > 0)
        {
            glViewport(0, 0, fw, fh);
            glClearColor(theme.base[0], theme.base[1], theme.base[2], 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            uiManager.RenderAll();

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            uiManager.GetGlassRenderer().CaptureCurrentFrame();
            uiManager.GetGlassRenderer().Blur(1.0f, 1);

            {
                GLFWwindow* ctxBackup = glfwGetCurrentContext();
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
                glfwMakeContextCurrent(ctxBackup);
            }

            glfwSwapBuffers(mainWindow);
        }
    }

    glfwDestroyWindow(splashWindow);
    splashWindow = nullptr;

    glfwShowWindow(mainWindow);
    glfwFocusWindow(mainWindow);

    std::cerr << "[DIAG] Entrando al loop principal\n";
    RunMainLoop(mainWindow, uiManager, theme);

    std::cerr << "[DIAG] Saliendo del loop principal, cerrando limpio\n";

    uiManager.Shutdown();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(mainWindow);
    glfwTerminate();

    if (ProyecThor::Settings::SettingsManager::Get().IsRestartRequested())
        ProyecThor::Settings::RestartApplication();

    return 0;
}
