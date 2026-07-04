#include "StreamingPanel.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#define STB_IMAGE_IMPLEMENTATION
#include "frontend/panels/stb_image.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <dwmapi.h>
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
#include "ui/UIManager.h"
#include "frontend/panels/LibraryPanel.h"
#include "frontend/panels/PreviewPanel.h"
#include "frontend/panels/ControlPanel.h"
#include "frontend/panels/capture/CapturePanel.h"
#include "frontend/ui/Hub.h"
#include "frontend/panels/ViewPanel.h"
#include "frontend/panels/BackgroundsPanel.h"
#include "frontend/panels/CanvasStylesPanel.h"

#ifdef _WIN32
    #pragma comment(lib, "dwmapi.lib")
#endif

static constexpr int   SPLASH_W       = 600;
static constexpr int   SPLASH_H       = 380;
static constexpr int   MAIN_W         = 1280;
static constexpr int   MAIN_H         = 720;
static constexpr int   LOAD_STEPS     = 5;

struct SplashArt {
    std::string filename;
    std::string author;
};

static const std::vector<SplashArt> splashRegistry = {
    {"splash_bg1.png", "Fabiola Fernandez"},
    {"splash_bg2.png", "TheVixcho"},
};

static float EaseOutCubic(float t)
{
    const float f = 1.0f - t;
    return 1.0f - f * f * f;
}

static ImU32 ThemeColorU32(const float c[4], float alphaOverride = -1.0f)
{
    auto toByte = [](float v) -> int {
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        return (int)(v * 255.0f + 0.5f);
    };
    const float a = (alphaOverride >= 0.0f) ? alphaOverride : c[3];
    return IM_COL32(toByte(c[0]), toByte(c[1]), toByte(c[2]), toByte(a));
}

static ImVec4 ThemeColorVec4(const float c[4], float alphaOverride = -1.0f)
{
    const float a = (alphaOverride >= 0.0f) ? alphaOverride : c[3];
    return ImVec4(c[0], c[1], c[2], a);
}

std::string GetAppDataFilePath(const std::string& filename) {
    const char* appData = std::getenv("APPDATA");
    if (!appData) return filename; // Fallback si falla la variable de entorno

    std::filesystem::path dirPath = std::filesystem::path(appData) / "ProyecThor";

    // Crea la carpeta ProyecThor en AppData si no existe
    if (!std::filesystem::exists(dirPath)) {
        std::filesystem::create_directories(dirPath);
    }

    return (dirPath / filename).string();
}

GLuint LoadTextureFromFile(const char* filename)
{
    int w = 0, h = 0, ch = 0;
    unsigned char* data = stbi_load(filename, &w, &h, &ch, 4);
    if (!data)
        return 0;

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);
    return tex;
}

static void RenderSplashScreen(GLFWwindow* splashWindow,
                               const std::string& status,
                               float progress,
                               GLuint logoTexture,
                               GLuint bgTexture,
                               ImFont* titleFont,
                               ImFont* regularFont,
                               ImFont* smallFont,
                               const std::string& creditText,
                               const ProyecThor::Settings::ThemeSettings& theme)
{
    glfwMakeContextCurrent(splashWindow);
    glClearColor(theme.base[0], theme.base[1], theme.base[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2((float)SPLASH_W, (float)SPLASH_H));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::Begin("##Splash", nullptr,
        ImGuiWindowFlags_NoDecoration          |
        ImGuiWindowFlags_NoMove                |
        ImGuiWindowFlags_NoBackground          |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (bgTexture != 0)
    {
        dl->AddImage(
            (void*)(intptr_t)bgTexture,
            ImVec2(0.0f, 0.0f),
            ImVec2((float)SPLASH_W, (float)SPLASH_H));
    }
    else
    {
        dl->AddRectFilled(
            ImVec2(0.0f, 0.0f),
            ImVec2((float)SPLASH_W, (float)SPLASH_H),
            ThemeColorU32(theme.surface0));
    }

    dl->AddRectFilledMultiColor(
        ImVec2(0.0f, 0.0f),
        ImVec2((float)SPLASH_W, (float)SPLASH_H),
        ThemeColorU32(theme.base, 245.0f / 255.0f),
        ThemeColorU32(theme.base, 120.0f / 255.0f),
        ThemeColorU32(theme.base, 120.0f / 255.0f),
        ThemeColorU32(theme.base, 245.0f / 255.0f));

    const float padX     = 50.0f;
    const float logoSize = 88.0f;
    const float logoY    = 58.0f;

    // Logo
    if (logoTexture != 0)
    {
        dl->AddImage(
            (void*)(intptr_t)logoTexture,
            ImVec2(padX, logoY),
            ImVec2(padX + logoSize, logoY + logoSize));
    }

    // Linea de acento vertical entre logo y texto
    dl->AddLine(
        ImVec2(padX + logoSize + 18.0f, logoY + 6.0f),
        ImVec2(padX + logoSize + 18.0f, logoY + logoSize - 6.0f),
        ThemeColorU32(theme.accent, 170.0f / 255.0f), 1.8f);

    // Titulo
    ImGui::SetCursorPos(ImVec2(padX + logoSize + 32.0f, logoY + 10.0f));
    if (titleFont) ImGui::PushFont(titleFont);
    ImGui::TextColored(ThemeColorVec4(theme.textPrimary), "ProyecThor");
    if (titleFont) ImGui::PopFont();

    // Subtitulo
    ImGui::SetCursorPos(ImVec2(padX + logoSize + 34.0f, logoY + 60.0f));
    if (regularFont) ImGui::PushFont(regularFont);
    ImGui::TextColored(ThemeColorVec4(theme.accentLight), "Professional Presentation Engine");
    if (regularFont) ImGui::PopFont();

    // Footer: fondo semi-opaco encima de la imagen de fondo
    const float footerH = 82.0f;
    const float footerY = (float)SPLASH_H - footerH;

    // ── Etiqueta del autor de la ilustración (Badge sobre la imagen) ──────
    if (smallFont) ImGui::PushFont(smallFont);
    const float creditW = ImGui::CalcTextSize(creditText.c_str()).x;
    const float creditH = ImGui::CalcTextSize(creditText.c_str()).y;
    const float badgeY  = footerY - creditH - 16.0f; // Posicionado justo arriba del footer

    // Fondo del badge (caja semitransparente oscura para que el texto siempre sea legible)
    dl->AddRectFilled(
        ImVec2(padX - 10.0f, badgeY),
        ImVec2(padX + creditW + 10.0f, footerY - 6.0f),
        ThemeColorU32(theme.base, 200.0f / 255.0f), 4.0f); // 4.0f da esquinas redondeadas suaves

    ImGui::SetCursorPos(ImVec2(padX, badgeY + 5.0f));
    ImGui::TextColored(ThemeColorVec4(theme.textDim), "%s", creditText.c_str());
    if (smallFont) ImGui::PopFont();
    // ────────────────────────────────────────────────────────────────────────

    // Dibujar el fondo del pie de página (footer)
    dl->AddRectFilled(
        ImVec2(0.0f, footerY),
        ImVec2((float)SPLASH_W, (float)SPLASH_H),
        ThemeColorU32(theme.base, 218.0f / 255.0f));

    // Linea separadora superior del footer
    dl->AddLine(
        ImVec2(0.0f,          footerY),
        ImVec2((float)SPLASH_W, footerY),
        ThemeColorU32(theme.borderFaint, 18.0f / 255.0f), 1.0f);

    // Texto de estado (Carga de componentes)
    ImGui::SetCursorPos(ImVec2(padX, footerY + 28.0f));
    if (smallFont) ImGui::PushFont(smallFont);
    ImGui::TextColored(ThemeColorVec4(theme.textDim), "%s", status.c_str());

    // Version y copyright alineados a la derecha
    const std::string versionLine = "Version " PROYECTHOR_VERSION_STRING "  |  Build 2026";
    const std::string copyLine    = "\xC2\xA9 2026 ProyecThor Team";
    const float vW = ImGui::CalcTextSize(versionLine.c_str()).x;
    const float cW = ImGui::CalcTextSize(copyLine.c_str()).x;

    ImGui::SetCursorPos(ImVec2((float)SPLASH_W - vW - padX, footerY + 18.0f));
    ImGui::TextColored(ThemeColorVec4(theme.textFaint), "%s", versionLine.c_str());

    ImGui::SetCursorPos(ImVec2((float)SPLASH_W - cW - padX, footerY + 42.0f));
    ImGui::TextColored(ThemeColorVec4(theme.textFaint, theme.textFaint[3] * 0.75f), "%s", copyLine.c_str());
    if (smallFont) ImGui::PopFont();

    // ── Barra de progreso animada (con efecto de brillo) ──────────────────
    const float barH   = 3.0f;
    const float barEnd = (float)SPLASH_W * progress;

    // Pista de la barra (fondo oscuro)
    dl->AddRectFilled(
        ImVec2(0.0f, (float)SPLASH_H - barH),
        ImVec2((float)SPLASH_W, (float)SPLASH_H),
        ThemeColorU32(theme.surface0));

    if (barEnd > 2.0f)
    {
        // Resplandor suave justo encima de la barra
        dl->AddRectFilled(
            ImVec2(0.0f, (float)SPLASH_H - barH - 2.0f),
            ImVec2(barEnd, (float)SPLASH_H - barH),
            ThemeColorU32(theme.accent, 60.0f / 255.0f));

        // Barra de progreso activa
        dl->AddRectFilled(
            ImVec2(0.0f, (float)SPLASH_H - barH),
            ImVec2(barEnd, (float)SPLASH_H),
            ThemeColorU32(theme.accent));

        // Punta brillante al final de la barra
        if (barEnd > 8.0f)
        {
            dl->AddRectFilled(
                ImVec2(barEnd - 8.0f, (float)SPLASH_H - barH),
                ImVec2(barEnd, (float)SPLASH_H),
                ThemeColorU32(theme.accentLight));
        }
    }
    // ────────────────────────────────────────────────────────────────────────

    ImGui::End();
    ImGui::PopStyleVar(2);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(splashWindow);
    glfwPollEvents();
}

// ── Paso de carga animado: interpola la barra de p0 a p1 mientras ejecuta
//    su tarea una sola vez. No toca contextos de ImGui ni backends; solo
//    repite RenderSplashScreen durante "dur" segundos.
// ────────────────────────────────────────────────────────────────────────
struct LoadStep
{
    std::string msg;
    float dur;
    std::function<void()> task;
};

static void RunStep(const LoadStep& step, int idx, int total, GLFWwindow* splashWindow,
                     GLuint logoTex, GLuint bgTex,
                     ImFont* titleFont, ImFont* regularFont, ImFont* smallFont,
                     const std::string& creditText,
                     const ProyecThor::Settings::ThemeSettings& theme)
{
    const float p0 = (float)idx / (float)total;
    const float p1 = (float)(idx + 1) / (float)total;
    const float t0 = (float)glfwGetTime();
    bool taskDone = false;

    while (true)
    {
        const float now      = (float)glfwGetTime();
        const float elapsed   = now - t0;
        const float t         = std::min(elapsed / step.dur, 1.0f);
        const float eased     = EaseOutCubic(t);
        const float progress  = p0 + (p1 - p0) * eased;

        // Siempre nos asegura el contexto del splash antes de dibujar,
        // sin importar lo que haya tocado la tarea (ver step.task()).
        glfwMakeContextCurrent(splashWindow);
        RenderSplashScreen(splashWindow, step.msg, progress,
            logoTex, bgTex, titleFont, regularFont, smallFont, creditText, theme);

        if (!taskDone)
        {
            step.task();
            taskDone = true;
        }

        if (elapsed >= step.dur)
            break;
    }
}
// ────────────────────────────────────────────────────────────────────────

// ── Profiling temporal por secciones del frame ──────────────────────────
// Acumula el tiempo de cada seccion durante N frames y despues imprime el
// promedio en milisegundos a consola. Solo diagnostico, no toca ninguna
// logica real del programa. Se puede sacar por completo una vez encontrado
// el cuello de botella.
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
// ────────────────────────────────────────────────────────────────────────

int main()
{
    if (!glfwInit())
        return -1;

    // Cargamos los settings (incluyendo el tema) antes de crear cualquier
    // ventana, para que el splash screen ya pinte con los colores correctos
    // desde el primer frame.
    ProyecThor::Settings::SettingsManager::Get().LoadSettings();
    auto& theme = ProyecThor::Settings::SettingsManager::Get().GetSettings().theme;

    glfwWindowHint(GLFW_DECORATED,             GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING,              GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE,             GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_VISIBLE,               GLFW_TRUE);

    GLFWwindow* splashWindow = glfwCreateWindow(SPLASH_W, SPLASH_H, "ProyecThor", nullptr, nullptr);
    if (!splashWindow)
    {
        glfwTerminate();
        return -1;
    }

    {
        const GLFWvidmode* vm = glfwGetVideoMode(glfwGetPrimaryMonitor());
        if (vm)
            glfwSetWindowPos(splashWindow,
                (vm->width  - SPLASH_W) / 2,
                (vm->height - SPLASH_H) / 2);
    }

    glfwMakeContextCurrent(splashWindow);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        glfwTerminate();
        return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& splashIO   = ImGui::GetIO();
    splashIO.IniFilename = nullptr;

    const char* fontPath  = "bin/assets/fonts/OpenSans-Regular.ttf";
    ImFont* titleFont   = splashIO.Fonts->AddFontFromFileTTF(fontPath, 46.0f);
    ImFont* regularFont = splashIO.Fonts->AddFontFromFileTTF(fontPath, 20.0f);
    ImFont* smallFont   = splashIO.Fonts->AddFontFromFileTTF(fontPath, 16.0f);

    ImGui_ImplGlfw_InitForOpenGL(splashWindow, true);
    ImGui_ImplOpenGL3_Init("#version 130");
    ImGui::StyleColorsDark();

    // ── Lógica de fondo y créditos secuenciales ─────────────────────────────
    std::string stateFile = GetAppDataFilePath("splash_state.txt");
    int bgIndex = 0;

    std::ifstream inFile(stateFile);
    if (inFile.is_open()) {
        if (inFile >> bgIndex) {
            bgIndex = (bgIndex + 1) % splashRegistry.size();
        }
        inFile.close();
    }

    std::ofstream outFile(stateFile);
    if (outFile.is_open()) {
        outFile << bgIndex;
        outFile.close();
    }

    std::string bgFilename = splashRegistry[bgIndex].filename;
    std::string creditText = "Illustration by: " + splashRegistry[bgIndex].author;
    // ────────────────────────────────────────────────────────────────────────

    GLuint logoTex = LoadTextureFromFile("proyecthor.png");
    GLuint bgTex   = LoadTextureFromFile(bgFilename.c_str());

    // Ventana principal: se crea dentro del paso 2, pero la declaramos aquí
    // porque la usamos en los pasos siguientes y en el resto de main().
    GLFWwindow* mainWindow = nullptr;

    const std::vector<LoadStep> steps = {
        // Paso 0: los settings ya se cargaron antes de crear el splash,
        // este paso solo se mantiene para la animación de carga.
        { "Leyendo preferencias del sistema...", 0.55f, [](){}},

        // Paso 1: crear la ventana principal y su contexto OpenGL
        { "Inicializando motor grafico OpenGL...", 0.50f, [&](){
            glfwDefaultWindowHints();
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
            glfwWindowHint(GLFW_RESIZABLE,             GLFW_TRUE);
            glfwWindowHint(GLFW_VISIBLE,               GLFW_FALSE);

            mainWindow = glfwCreateWindow(MAIN_W, MAIN_H, "ProyecThor", nullptr, splashWindow);
            if (!mainWindow)
            {
                glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
                glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
                mainWindow = glfwCreateWindow(MAIN_W, MAIN_H, "ProyecThor", nullptr, splashWindow);
            }
            if (!mainWindow)
                return;

#ifdef _WIN32
            {
                HWND hwnd = glfwGetWin32Window(mainWindow);
                BOOL useDarkMode = TRUE;
                DwmSetWindowAttribute(hwnd, 20, &useDarkMode, sizeof(useDarkMode));
            }
#endif
            {
                GLFWimage images[1];
                images[0].pixels = stbi_load("proyecthor.png",
                    &images[0].width, &images[0].height, 0, 4);
                if (images[0].pixels)
                {
                    glfwSetWindowIcon(mainWindow, 1, images);
                    stbi_image_free(images[0].pixels);
                }
            }

            glfwMakeContextCurrent(mainWindow);
            glfwSwapInterval(1);
            glewExperimental = GL_TRUE;
            glewInit();
        }},

        // Paso 2: cargar iconos y recursos graficos (contexto compartido)
        { "Cargando iconos y recursos graficos...", 0.50f, [&](){
    if (!mainWindow) return;
    glfwMakeContextCurrent(mainWindow);

    StyleGeneralApp::LoadAppIcon("search",      "bin/assets/icons/ui/searchico.png");
    StyleGeneralApp::LoadAppIcon("izquierda",   "bin/assets/icons/ui/izquierda.png");
    StyleGeneralApp::LoadAppIcon("editar",      "bin/assets/icons/ui/editar.png");
    StyleGeneralApp::LoadAppIcon("play",        "bin/assets/icons/ui/play.png");
    StyleGeneralApp::LoadAppIcon("pause",       "bin/assets/icons/ui/pause.png");
    StyleGeneralApp::LoadAppIcon("add",         "bin/assets/icons/ui/add.png");
    StyleGeneralApp::LoadAppIcon("delete",      "bin/assets/icons/ui/delete.png");
    StyleGeneralApp::LoadAppIcon("skip_next",   "bin/assets/icons/ui/skip_next.png");
    StyleGeneralApp::LoadAppIcon("skip_prev",   "bin/assets/icons/ui/skip_previous.png");
    StyleGeneralApp::LoadAppIcon("forward_10",  "bin/assets/icons/ui/forward_10.png");
    StyleGeneralApp::LoadAppIcon("replay_10",   "bin/assets/icons/ui/replay_10.png");
    StyleGeneralApp::LoadAppIcon("volume_up",   "bin/assets/icons/ui/volume_up.png");
    StyleGeneralApp::LoadAppIcon("no_sound",    "bin/assets/icons/ui/no_sound.png");
    StyleGeneralApp::LoadAppIcon("favorite",    "bin/assets/icons/ui/favorite.png");
    StyleGeneralApp::LoadAppIcon("fit_screen",  "bin/assets/icons/ui/fit_screen.png");
    StyleGeneralApp::LoadAppIcon("arrow_forward",     "bin/assets/icons/ui/arrow_forward.png");
    StyleGeneralApp::LoadAppIcon("arrow_back",        "bin/assets/icons/ui/arrow_back.png");
    StyleGeneralApp::LoadAppIcon("repeat",            "bin/assets/icons/ui/repeat.png");
    StyleGeneralApp::LoadAppIcon("repeat_one",        "bin/assets/icons/ui/repeat_one.png");
    StyleGeneralApp::LoadAppIcon("stop",              "bin/assets/icons/ui/stop.png");
    StyleGeneralApp::LoadAppIcon("motion_play",       "bin/assets/icons/ui/motion_play.png");
    StyleGeneralApp::LoadAppIcon("cleaning_services", "bin/assets/icons/ui/cleaning_services.png");
    StyleGeneralApp::LoadAppIcon("add_to_queue",      "bin/assets/icons/ui/add_to_queue.png");
    StyleGeneralApp::LoadAppIcon("original_screen",   "bin/assets/icons/ui/original_screen.png");
    StyleGeneralApp::LoadAppIcon("add_photo",   "bin/assets/icons/ui/add_photo.png");
StyleGeneralApp::LoadAppIcon("upload_file", "bin/assets/icons/ui/upload_file.png");
StyleGeneralApp::LoadAppIcon("cards_star",  "bin/assets/icons/ui/cards_star.png");
        }},

        // Paso 3: tipografias y modulos de interfaz (se preparan despues del splash)
        { "Cargando tipografias y modulos de interfaz...", 0.45f, [](){}},

        // Paso 4: listo, la barra llega al 100%
        { "Listo", 0.30f, [](){}},
    };

    for (int i = 0; i < (int)steps.size(); ++i)
        RunStep(steps[i], i, LOAD_STEPS, splashWindow,
                logoTex, bgTex, titleFont, regularFont, smallFont, creditText, theme);

    if (!mainWindow)
    {
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

    io.Fonts->AddFontFromFileTTF("bin/assets/fonts/OpenSans-Regular.ttf", 16.0f);
    ProyecThor::Core::PresentationCore::Get().LoadFontsIntoImGui();

    ImGui_ImplGlfw_InitForOpenGL(mainWindow, true);
ImGui_ImplOpenGL3_Init("#version 130");
ImGui::StyleColorsDark();

// ── Fix crítico: las ventanas de viewports secundarios (incluida la del
//    proyector en el segundo monitor) NO deben esperar su propio vsync.
//    RenderPlatformWindowsDefault() las swapea sincrónicamente en este
//    mismo hilo cada frame; si alguna espera vsync de un monitor a
//    distinto refresh (ej. 60Hz) que la ventana principal (144Hz), todo
//    el hilo queda atrapado esperando el más lento.
{
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    static void (*s_OrigCreateWindow)(ImGuiViewport*) = platform_io.Platform_CreateWindow;

    platform_io.Platform_CreateWindow = [](ImGuiViewport* viewport)
    {
        s_OrigCreateWindow(viewport); // crea la ventana GLFW real del viewport

        GLFWwindow* w = static_cast<GLFWwindow*>(viewport->PlatformHandle);
        if (w)
        {
            GLFWwindow* backup = glfwGetCurrentContext();
            glfwMakeContextCurrent(w);
            glfwSwapInterval(0);   // <- clave
            glfwMakeContextCurrent(backup);
        }
    };
}

    // Aplica el tema (preset o personalizado) guardado en settings sobre
    // el estilo de ImGui recien creado para la ventana principal.
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

    ProyecThor::UI::UIManager uiManager;
    if (!uiManager.Initialize(mainWindow))
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(mainWindow);
        glfwDestroyWindow(splashWindow);
        glfwTerminate();
        return -1;
    }

    auto previewPanel = std::make_shared<ProyecThor::UI::PreviewPanel>();
    auto libraryPanel = std::make_shared<ProyecThor::UI::LibraryPanel>();
previewPanel->SetAudioPanel(libraryPanel->GetAudioPanel());
    previewPanel->m_UIManagerRef = &uiManager;
    libraryPanel->SetUIManager(&uiManager);

    uiManager.AddPanel(libraryPanel);
    uiManager.AddPanel(previewPanel);
    uiManager.AddPanel(std::make_shared<ProyecThor::UI::CapturePanel>());
    uiManager.AddPanel(std::make_shared<ProyecThor::UI::ControlPanel>(&uiManager));
    uiManager.AddPanel(std::make_shared<ProyecThor::UI::ViewPanel>());
    uiManager.AddPanel(std::make_shared<ProyecThor::UI::BackgroundsPanel>());
    uiManager.AddPanel(std::make_shared<ProyecThor::UI::CanvasStylesPanel>());
    uiManager.AddPanel(std::make_shared<ProyecThor::UI::StreamingPanel>());
    uiManager.AddPanel(uiManager.GetTransitionPanelOwned());

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

    while (!glfwWindowShouldClose(mainWindow))
    {
        using Clock = FrameProfiler::Clock;
        auto frameStart = Clock::now();

        auto t0 = Clock::now();
        glfwPollEvents();
        FrameProfiler::Add(FrameProfiler::s_PollEvents, FrameProfiler::ElapsedMs(t0));

        auto& core = ProyecThor::Core::PresentationCore::Get();

        if (glfwGetKey(mainWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            core.SetProjecting(false);
            core.ClearLayer2();
        }

        auto t1 = Clock::now();
        core.Update();
        FrameProfiler::Add(FrameProfiler::s_CoreUpdate, FrameProfiler::ElapsedMs(t1));

        int fw, fh;
        glfwGetFramebufferSize(mainWindow, &fw, &fh);

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

        auto t3 = Clock::now();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        FrameProfiler::Add(FrameProfiler::s_ImGuiRender, FrameProfiler::ElapsedMs(t3));

        auto t4 = Clock::now();
        {
            GLFWwindow* ctxBackup = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(ctxBackup);
        }
        FrameProfiler::Add(FrameProfiler::s_PlatformWindows, FrameProfiler::ElapsedMs(t4));

        auto t5 = Clock::now();
        glfwSwapBuffers(mainWindow);
        FrameProfiler::Add(FrameProfiler::s_SwapBuffers, FrameProfiler::ElapsedMs(t5));

        FrameProfiler::Add(FrameProfiler::s_FrameTotal, FrameProfiler::ElapsedMs(frameStart));
        FrameProfiler::ReportIfReady();
    }

    uiManager.Shutdown();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(mainWindow);
    glfwTerminate();

    return 0;
}