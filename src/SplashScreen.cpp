#include "SplashScreen.h"

#include <GL/glew.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include "Version.h"

std::string GetAppDataFilePath(const std::string& filename);

namespace ProyecThor::Splash {

namespace {

const std::vector<Art> kRegistry = {
    {"splash_bg1.png", "Fabiola Fernandez"},
    {"splash_bg5.png", "TheVixcho"},
    {"splash_bg2.png", "TheVixcho"},
};

float EaseOutQuad(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return 1.0f - (1.0f - t) * (1.0f - t);
}

ImU32 ThemeColorU32(const float c[4], float alphaOverride = -1.0f) {
    auto toByte = [](float v) -> int {
        v = std::clamp(v, 0.0f, 1.0f);
        return (int)(v * 255.0f + 0.5f);
    };
    const float a = (alphaOverride >= 0.0f) ? alphaOverride : c[3];
    return IM_COL32(toByte(c[0]), toByte(c[1]), toByte(c[2]), toByte(a));
}

ImVec4 ThemeColorVec4(const float c[4], float alphaOverride = -1.0f) {
    const float a = (alphaOverride >= 0.0f) ? alphaOverride : c[3];
    return ImVec4(c[0], c[1], c[2], a);
}

} // namespace

Art PickArt() {
    const std::string stateFile = GetAppDataFilePath("splash_state.txt");
    int index = 0;

    std::ifstream inFile(stateFile);
    if (inFile.is_open()) {
        if (inFile >> index)
            index = (index + 1) % (int)kRegistry.size();
        inFile.close();
    }

    std::ofstream outFile(stateFile);
    if (outFile.is_open()) {
        outFile << index;
        outFile.close();
    }

    return kRegistry[index];
}

void Render(GLFWwindow* window, ImVec2 size, const std::string& status, float progress,
            GLuint logoTexture, GLuint bgTexture, const Fonts& fonts,
            const std::string& creditText, const ProyecThor::Settings::ThemeSettings& theme)
{
    static const double s_StartTime = glfwGetTime();
    const float appear = EaseOutQuad((float)(glfwGetTime() - s_StartTime) / 0.30f);

    glfwMakeContextCurrent(window);
    glClearColor(theme.base[0], theme.base[1], theme.base[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,            appear);

    ImGui::Begin("##Splash", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float scaleY = size.y / 380.0f;

    const float margin   = 4.0f;
    const float rounding = 18.0f;
    const ImVec2 cardMin(margin, margin);
    const ImVec2 cardMax(size.x - margin, size.y - margin);

    for (int i = 5; i >= 1; i--) {
        const float t   = (float)i / 5.0f;
        const float pad = t * 10.0f;
        const float a   = 0.05f * (1.0f - t) * appear;
        dl->AddRectFilled(ImVec2(cardMin.x - pad, cardMin.y - pad), ImVec2(cardMax.x + pad, cardMax.y + pad),
            ThemeColorU32(theme.accent, a), rounding + pad * 0.5f);
    }

    dl->AddRectFilled(cardMin, cardMax, ThemeColorU32(theme.surface0), rounding);
    if (bgTexture != 0)
        dl->AddImageRounded((void*)(intptr_t)bgTexture, cardMin, cardMax,
            ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), IM_COL32_WHITE, rounding);

    dl->AddRectFilled(cardMin, cardMax, ThemeColorU32(theme.base, 90.0f / 255.0f), rounding);
    dl->AddRect(cardMin, cardMax, ThemeColorU32(theme.accent, (60.0f / 255.0f) * appear), rounding, 0, 1.5f);

    const float padX     = 50.0f;
    const float logoSize = 88.0f * scaleY;
    const float logoY    = 58.0f * scaleY;

    if (logoTexture != 0)
        dl->AddImage((void*)(intptr_t)logoTexture, ImVec2(padX, logoY), ImVec2(padX + logoSize, logoY + logoSize));

    dl->AddLine(ImVec2(padX + logoSize + 18.0f, logoY + 6.0f), ImVec2(padX + logoSize + 18.0f, logoY + logoSize - 6.0f),
        ThemeColorU32(theme.accent, 170.0f / 255.0f), 1.8f);

    ImGui::SetCursorPos(ImVec2(padX + logoSize + 32.0f, logoY + 10.0f));
    if (fonts.title) ImGui::PushFont(fonts.title);
    ImGui::TextColored(ThemeColorVec4(theme.textPrimary), "ProyecThor");
    if (fonts.title) ImGui::PopFont();

    ImGui::SetCursorPos(ImVec2(padX + logoSize + 34.0f, logoY + 60.0f));
    if (fonts.regular) ImGui::PushFont(fonts.regular);
    ImGui::TextColored(ThemeColorVec4(theme.accentLight), "Professional Presentation Engine");
    if (fonts.regular) ImGui::PopFont();

    const float footerH = 82.0f * scaleY;
    const float footerY = size.y - footerH;

    if (fonts.small) ImGui::PushFont(fonts.small);
    const float creditW = ImGui::CalcTextSize(creditText.c_str()).x;
    const float creditH = ImGui::CalcTextSize(creditText.c_str()).y;
    const float badgeY  = footerY - creditH - 16.0f;

    dl->AddRectFilled(ImVec2(padX - 10.0f, badgeY), ImVec2(padX + creditW + 10.0f, footerY - 6.0f),
        ThemeColorU32(theme.base, 200.0f / 255.0f), 6.0f);

    ImGui::SetCursorPos(ImVec2(padX, badgeY + 5.0f));
    ImGui::TextColored(ThemeColorVec4(theme.textDim), "%s", creditText.c_str());
    if (fonts.small) ImGui::PopFont();

    dl->AddRectFilled(ImVec2(margin, footerY), cardMax, ThemeColorU32(theme.base, 218.0f / 255.0f),
        rounding, ImDrawFlags_RoundCornersBottom);
    dl->AddLine(ImVec2(margin, footerY), ImVec2(cardMax.x, footerY), ThemeColorU32(theme.borderFaint, 18.0f / 255.0f), 1.0f);

    ImGui::SetCursorPos(ImVec2(padX, footerY + 28.0f * scaleY));
    if (fonts.small) ImGui::PushFont(fonts.small);
    ImGui::TextColored(ThemeColorVec4(theme.textDim), "%s", status.c_str());

    static const std::string versionLine =
        "Version " PROYECTHOR_VERSION_STRING "  |  Build " + std::to_string(PROYECTHOR_BUILD_NUMBER);
    const std::string copyLine = "\xC2\xA9 2026 ProyecThor Team";
    const float vW = ImGui::CalcTextSize(versionLine.c_str()).x;
    const float cW = ImGui::CalcTextSize(copyLine.c_str()).x;

    ImGui::SetCursorPos(ImVec2(size.x - vW - padX, footerY + 18.0f * scaleY));
    ImGui::TextColored(ThemeColorVec4(theme.textFaint), "%s", versionLine.c_str());

    ImGui::SetCursorPos(ImVec2(size.x - cW - padX, footerY + 42.0f * scaleY));
    ImGui::TextColored(ThemeColorVec4(theme.textFaint, theme.textFaint[3] * 0.75f), "%s", copyLine.c_str());
    if (fonts.small) ImGui::PopFont();

    const float barH   = 4.0f;
    const float barEnd = margin + (size.x - margin * 2.0f) * progress;

    dl->AddRectFilled(ImVec2(margin, cardMax.y - barH), cardMax, ThemeColorU32(theme.surface0),
        rounding, ImDrawFlags_RoundCornersBottom);

    if (barEnd > margin + 2.0f) {
        dl->AddRectFilled(ImVec2(margin, cardMax.y - barH - 6.0f), ImVec2(barEnd, cardMax.y),
            ThemeColorU32(theme.accent, 45.0f / 255.0f));
        dl->AddRectFilled(ImVec2(margin, cardMax.y - barH), ImVec2(barEnd, cardMax.y),
            ThemeColorU32(theme.accent), rounding, ImDrawFlags_RoundCornersBottomLeft);
    }

    ImGui::End();
    ImGui::PopStyleVar(3);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
    glfwPollEvents();
}

void RunStep(const Step& step, int idx, int total, GLFWwindow* window, ImVec2 size,
             GLuint logoTex, GLuint bgTex, const Fonts& fonts,
             const std::string& creditText, const ProyecThor::Settings::ThemeSettings& theme)
{
    step.task();

    const float progress = (float)(idx + 1) / (float)total;
    glfwMakeContextCurrent(window);
    Render(window, size, step.msg, progress, logoTex, bgTex, fonts, creditText, theme);
}

} // namespace ProyecThor::Splash
