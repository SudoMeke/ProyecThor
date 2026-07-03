#include "SettingsManager.h"
#include "DesignSystem.h"
#include "backend/core/PresentationCore.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include "MonitorTheme.h" 
#include "HubTheme.h"
#include "ControlTheme.h"

using json = nlohmann::json;

namespace ProyecThor::Settings {

static std::string GetSettingsPath() {
    const char* appData = std::getenv("APPDATA");
    if (!appData) return "settings.json";

    std::filesystem::path dir = std::filesystem::path(appData) / "ProyecThor";
    if (!std::filesystem::exists(dir))
        std::filesystem::create_directories(dir);

    return (dir / "settings.json").string();
}

const char* ThemePresetName(ThemePreset preset) {
    switch (preset) {
        case ThemePreset::Dark:        return "Oscuro";
        case ThemePreset::Light:       return "Claro";
        case ThemePreset::OrangeBlack: return "Naranja y Negro";
        case ThemePreset::Jazz:        return "Jazz";
        case ThemePreset::Kofi:        return "Ko-fi";
        case ThemePreset::Deadlock:    return "Deadlock";
        case ThemePreset::Galaxy:      return "Galaxia";
        default:                       return "Personalizado";
    }
}

ThemePreset ThemePresetFromString(const std::string& s) {
    if (s == "dark")        return ThemePreset::Dark;
    if (s == "light")       return ThemePreset::Light;
    if (s == "orangeblack") return ThemePreset::OrangeBlack;
    if (s == "jazz")        return ThemePreset::Jazz;
    if (s == "kofi")        return ThemePreset::Kofi;
    if (s == "deadlock")    return ThemePreset::Deadlock;
    if (s == "galaxy")      return ThemePreset::Galaxy;
    return ThemePreset::Custom;
}

static std::string ThemePresetToKey(ThemePreset preset) {
    switch (preset) {
        case ThemePreset::Dark:        return "dark";
        case ThemePreset::Light:       return "light";
        case ThemePreset::OrangeBlack: return "orangeblack";
        case ThemePreset::Jazz:        return "jazz";
        case ThemePreset::Kofi:        return "kofi";
        case ThemePreset::Deadlock:    return "deadlock";
        case ThemePreset::Galaxy:      return "galaxy";
        default:                       return "custom";
    }
}

// ── Presets ──────────────────────────────────────────────────────────────
ThemeSettings MakeThemePreset(ThemePreset preset) {
    ThemeSettings t;
    t.preset = preset;

    switch (preset) {

    case ThemePreset::Light: {
        t.base[0]=0.94f; t.base[1]=0.95f; t.base[2]=0.97f; t.base[3]=1.0f;
        t.surface0[0]=0.99f; t.surface0[1]=0.99f; t.surface0[2]=1.00f; t.surface0[3]=1.0f;
        t.surface1[0]=1.00f; t.surface1[1]=1.00f; t.surface1[2]=1.00f; t.surface1[3]=1.0f;
        t.surface2[0]=0.90f; t.surface2[1]=0.91f; t.surface2[2]=0.94f; t.surface2[3]=1.0f;
        t.surface3[0]=0.85f; t.surface3[1]=0.86f; t.surface3[2]=0.90f; t.surface3[3]=1.0f;
        t.accent[0]=0.17f; t.accent[1]=0.45f; t.accent[2]=0.95f; t.accent[3]=1.0f;
        t.accentLight[0]=0.35f; t.accentLight[1]=0.60f; t.accentLight[2]=1.00f; t.accentLight[3]=1.0f;
        t.accentDim[0]=0.17f; t.accentDim[1]=0.45f; t.accentDim[2]=0.95f; t.accentDim[3]=0.5f;
        t.accentFaint[0]=0.17f; t.accentFaint[1]=0.45f; t.accentFaint[2]=0.95f; t.accentFaint[3]=0.15f;
        t.border[0]=0; t.border[1]=0; t.border[2]=0; t.border[3]=0.12f;
        t.borderFaint[0]=0; t.borderFaint[1]=0; t.borderFaint[2]=0; t.borderFaint[3]=0.06f;
        t.textPrimary[0]=0.08f; t.textPrimary[1]=0.09f; t.textPrimary[2]=0.12f; t.textPrimary[3]=1.0f;
        t.textDim[0]=0.35f; t.textDim[1]=0.37f; t.textDim[2]=0.42f; t.textDim[3]=1.0f;
        t.textFaint[0]=0; t.textFaint[1]=0; t.textFaint[2]=0; t.textFaint[3]=0.35f;
        t.danger[0]=0.85f; t.danger[1]=0.20f; t.danger[2]=0.25f; t.danger[3]=1.0f;
        t.success[0]=0.15f; t.success[1]=0.60f; t.success[2]=0.35f; t.success[3]=1.0f;
        t.windowRounding=10.0f; t.frameRounding=7.0f; t.scrollbarSize=7.0f;
        break;
    }

    case ThemePreset::OrangeBlack: {
        t.base[0]=0.05f; t.base[1]=0.04f; t.base[2]=0.03f; t.base[3]=1.0f;
        t.surface0[0]=0.09f; t.surface0[1]=0.06f; t.surface0[2]=0.03f; t.surface0[3]=1.0f;
        t.surface1[0]=0.12f; t.surface1[1]=0.08f; t.surface1[2]=0.04f; t.surface1[3]=1.0f;
        t.surface2[0]=0.16f; t.surface2[1]=0.10f; t.surface2[2]=0.05f; t.surface2[3]=1.0f;
        t.surface3[0]=0.20f; t.surface3[1]=0.13f; t.surface3[2]=0.06f; t.surface3[3]=1.0f;
        t.accent[0]=1.00f; t.accent[1]=0.55f; t.accent[2]=0.05f; t.accent[3]=1.0f;
        t.accentLight[0]=1.00f; t.accentLight[1]=0.72f; t.accentLight[2]=0.30f; t.accentLight[3]=1.0f;
        t.accentDim[0]=0.70f; t.accentDim[1]=0.38f; t.accentDim[2]=0.03f; t.accentDim[3]=1.0f;
        t.accentFaint[0]=1.00f; t.accentFaint[1]=0.55f; t.accentFaint[2]=0.05f; t.accentFaint[3]=0.18f;
        t.border[0]=1.00f; t.border[1]=0.55f; t.border[2]=0.05f; t.border[3]=0.20f;
        t.borderFaint[0]=1; t.borderFaint[1]=1; t.borderFaint[2]=1; t.borderFaint[3]=0.05f;
        t.textPrimary[0]=1.00f; t.textPrimary[1]=0.95f; t.textPrimary[2]=0.90f; t.textPrimary[3]=1.0f;
        t.textDim[0]=0.85f; t.textDim[1]=0.65f; t.textDim[2]=0.45f; t.textDim[3]=1.0f;
        t.textFaint[0]=1; t.textFaint[1]=1; t.textFaint[2]=1; t.textFaint[3]=0.30f;
        t.danger[0]=1.00f; t.danger[1]=0.25f; t.danger[2]=0.20f; t.danger[3]=1.0f;
        t.success[0]=0.60f; t.success[1]=0.90f; t.success[2]=0.30f; t.success[3]=1.0f;
        t.windowRounding=6.0f; t.frameRounding=4.0f; t.scrollbarSize=7.0f;
        break;
    }

    case ThemePreset::Jazz: {
        t.base[0]=0.09f; t.base[1]=0.05f; t.base[2]=0.07f; t.base[3]=1.0f;
        t.surface0[0]=0.13f; t.surface0[1]=0.07f; t.surface0[2]=0.10f; t.surface0[3]=1.0f;
        t.surface1[0]=0.17f; t.surface1[1]=0.09f; t.surface1[2]=0.13f; t.surface1[3]=1.0f;
        t.surface2[0]=0.22f; t.surface2[1]=0.12f; t.surface2[2]=0.17f; t.surface2[3]=1.0f;
        t.surface3[0]=0.27f; t.surface3[1]=0.15f; t.surface3[2]=0.20f; t.surface3[3]=1.0f;
        t.accent[0]=0.80f; t.accent[1]=0.62f; t.accent[2]=0.25f; t.accent[3]=1.0f;
        t.accentLight[0]=0.92f; t.accentLight[1]=0.78f; t.accentLight[2]=0.45f; t.accentLight[3]=1.0f;
        t.accentDim[0]=0.55f; t.accentDim[1]=0.40f; t.accentDim[2]=0.15f; t.accentDim[3]=1.0f;
        t.accentFaint[0]=0.80f; t.accentFaint[1]=0.62f; t.accentFaint[2]=0.25f; t.accentFaint[3]=0.16f;
        t.border[0]=0.80f; t.border[1]=0.62f; t.border[2]=0.25f; t.border[3]=0.25f;
        t.borderFaint[0]=1; t.borderFaint[1]=1; t.borderFaint[2]=1; t.borderFaint[3]=0.05f;
        t.textPrimary[0]=0.96f; t.textPrimary[1]=0.92f; t.textPrimary[2]=0.85f; t.textPrimary[3]=1.0f;
        t.textDim[0]=0.75f; t.textDim[1]=0.60f; t.textDim[2]=0.55f; t.textDim[3]=1.0f;
        t.textFaint[0]=1; t.textFaint[1]=1; t.textFaint[2]=1; t.textFaint[3]=0.30f;
        t.danger[0]=0.85f; t.danger[1]=0.25f; t.danger[2]=0.30f; t.danger[3]=1.0f;
        t.success[0]=0.55f; t.success[1]=0.75f; t.success[2]=0.45f; t.success[3]=1.0f;
        t.windowRounding=14.0f; t.frameRounding=9.0f; t.scrollbarSize=8.0f;
        break;
    }

    case ThemePreset::Kofi: {
        t.base[0]=0.98f; t.base[1]=0.94f; t.base[2]=0.88f; t.base[3]=1.0f;
        t.surface0[0]=1.00f; t.surface0[1]=0.97f; t.surface0[2]=0.92f; t.surface0[3]=1.0f;
        t.surface1[0]=1.00f; t.surface1[1]=1.00f; t.surface1[2]=0.97f; t.surface1[3]=1.0f;
        t.surface2[0]=0.95f; t.surface2[1]=0.88f; t.surface2[2]=0.78f; t.surface2[3]=1.0f;
        t.surface3[0]=0.90f; t.surface3[1]=0.80f; t.surface3[2]=0.68f; t.surface3[3]=1.0f;
        t.accent[0]=1.00f; t.accent[1]=0.37f; t.accent[2]=0.36f; t.accent[3]=1.0f; // #FF5E5B
        t.accentLight[0]=1.00f; t.accentLight[1]=0.55f; t.accentLight[2]=0.53f; t.accentLight[3]=1.0f;
        t.accentDim[0]=0.85f; t.accentDim[1]=0.30f; t.accentDim[2]=0.29f; t.accentDim[3]=1.0f;
        t.accentFaint[0]=1.00f; t.accentFaint[1]=0.37f; t.accentFaint[2]=0.36f; t.accentFaint[3]=0.15f;
        t.border[0]=0; t.border[1]=0; t.border[2]=0; t.border[3]=0.10f;
        t.borderFaint[0]=0; t.borderFaint[1]=0; t.borderFaint[2]=0; t.borderFaint[3]=0.05f;
        t.textPrimary[0]=0.20f; t.textPrimary[1]=0.13f; t.textPrimary[2]=0.10f; t.textPrimary[3]=1.0f;
        t.textDim[0]=0.45f; t.textDim[1]=0.35f; t.textDim[2]=0.30f; t.textDim[3]=1.0f;
        t.textFaint[0]=0; t.textFaint[1]=0; t.textFaint[2]=0; t.textFaint[3]=0.35f;
        t.danger[0]=0.80f; t.danger[1]=0.15f; t.danger[2]=0.15f; t.danger[3]=1.0f;
        t.success[0]=0.25f; t.success[1]=0.65f; t.success[2]=0.35f; t.success[3]=1.0f;
        t.windowRounding=16.0f; t.frameRounding=10.0f; t.scrollbarSize=8.0f;
        break;
    }

    case ThemePreset::Deadlock: {
        t.base[0]=0.035f; t.base[1]=0.060f; t.base[2]=0.050f; t.base[3]=1.0f;
        t.surface0[0]=0.050f; t.surface0[1]=0.090f; t.surface0[2]=0.075f; t.surface0[3]=1.0f;
        t.surface1[0]=0.070f; t.surface1[1]=0.120f; t.surface1[2]=0.100f; t.surface1[3]=1.0f;
        t.surface2[0]=0.090f; t.surface2[1]=0.150f; t.surface2[2]=0.125f; t.surface2[3]=1.0f;
        t.surface3[0]=0.120f; t.surface3[1]=0.190f; t.surface3[2]=0.155f; t.surface3[3]=1.0f;
        t.accent[0]=0.55f; t.accent[1]=0.95f; t.accent[2]=0.35f; t.accent[3]=1.0f;
        t.accentLight[0]=0.72f; t.accentLight[1]=1.00f; t.accentLight[2]=0.55f; t.accentLight[3]=1.0f;
        t.accentDim[0]=0.30f; t.accentDim[1]=0.55f; t.accentDim[2]=0.20f; t.accentDim[3]=1.0f;
        t.accentFaint[0]=0.55f; t.accentFaint[1]=0.95f; t.accentFaint[2]=0.35f; t.accentFaint[3]=0.15f;
        t.border[0]=0.55f; t.border[1]=0.95f; t.border[2]=0.35f; t.border[3]=0.18f;
        t.borderFaint[0]=1; t.borderFaint[1]=1; t.borderFaint[2]=1; t.borderFaint[3]=0.05f;
        t.textPrimary[0]=0.90f; t.textPrimary[1]=0.98f; t.textPrimary[2]=0.92f; t.textPrimary[3]=1.0f;
        t.textDim[0]=0.60f; t.textDim[1]=0.75f; t.textDim[2]=0.65f; t.textDim[3]=1.0f;
        t.textFaint[0]=1; t.textFaint[1]=1; t.textFaint[2]=1; t.textFaint[3]=0.28f;
        t.danger[0]=0.95f; t.danger[1]=0.30f; t.danger[2]=0.30f; t.danger[3]=1.0f;
        t.success[0]=0.55f; t.success[1]=0.95f; t.success[2]=0.35f; t.success[3]=1.0f;
        t.windowRounding=10.0f; t.frameRounding=6.0f; t.scrollbarSize=7.0f;
        break;
    }

    case ThemePreset::Galaxy: {
        t.base[0]=0.040f; t.base[1]=0.030f; t.base[2]=0.080f; t.base[3]=1.0f;
        t.surface0[0]=0.070f; t.surface0[1]=0.050f; t.surface0[2]=0.140f; t.surface0[3]=1.0f;
        t.surface1[0]=0.100f; t.surface1[1]=0.070f; t.surface1[2]=0.190f; t.surface1[3]=1.0f;
        t.surface2[0]=0.140f; t.surface2[1]=0.100f; t.surface2[2]=0.250f; t.surface2[3]=1.0f;
        t.surface3[0]=0.180f; t.surface3[1]=0.130f; t.surface3[2]=0.320f; t.surface3[3]=1.0f;
        t.accent[0]=0.65f; t.accent[1]=0.35f; t.accent[2]=1.00f; t.accent[3]=1.0f;
        t.accentLight[0]=0.80f; t.accentLight[1]=0.55f; t.accentLight[2]=1.00f; t.accentLight[3]=1.0f;
        t.accentDim[0]=0.42f; t.accentDim[1]=0.22f; t.accentDim[2]=0.68f; t.accentDim[3]=1.0f;
        t.accentFaint[0]=0.65f; t.accentFaint[1]=0.35f; t.accentFaint[2]=1.00f; t.accentFaint[3]=0.16f;
        t.border[0]=0.65f; t.border[1]=0.35f; t.border[2]=1.00f; t.border[3]=0.22f;
        t.borderFaint[0]=1; t.borderFaint[1]=1; t.borderFaint[2]=1; t.borderFaint[3]=0.05f;
        t.textPrimary[0]=0.92f; t.textPrimary[1]=0.90f; t.textPrimary[2]=0.98f; t.textPrimary[3]=1.0f;
        t.textDim[0]=0.68f; t.textDim[1]=0.62f; t.textDim[2]=0.85f; t.textDim[3]=1.0f;
        t.textFaint[0]=1; t.textFaint[1]=1; t.textFaint[2]=1; t.textFaint[3]=0.30f;
        t.danger[0]=1.00f; t.danger[1]=0.35f; t.danger[2]=0.55f; t.danger[3]=1.0f;
        t.success[0]=0.35f; t.success[1]=0.90f; t.success[2]=0.75f; t.success[3]=1.0f;
        t.windowRounding=16.0f; t.frameRounding=10.0f; t.scrollbarSize=8.0f;
        break;
    }

    case ThemePreset::Dark:
    default:
        // Valores por defecto de ThemeSettings ya son el tema Oscuro.
        break;
    }

    return t;
}

// ── Proyección ───────────────────────────────────────────────────────────
void SettingsManager::ApplyProjection() {
    const auto& p    = m_Settings.projection;
    auto&       core = ::ProyecThor::Core::PresentationCore::Get();

    core.SetTargetMonitor(p.targetMonitor);

    float tc[4]      = { p.textColorR, p.textColorG, p.textColorB, p.textColorA };
    float margins[4] = { p.marginTop,  p.marginBottom, p.marginLeft, p.marginRight };

    core.UpdateTextStyle(p.textSize, tc, p.textAlignment, p.vAlignment,
                          margins, p.autoScale, p.selectedFont);
    core.SetLayer0_Color(p.defaultBgR, p.defaultBgG, p.defaultBgB);
}

// ── Tema ─────────────────────────────────────────────────────────────────
// Expande los tokens de ThemeSettings a todo el estilo de ImGui y
// sincroniza DesignSystem (paneles glass) para que el tema alcance
// también a los widgets custom dibujados con ImDrawList.
void SettingsManager::ApplyTheme() {
    const auto& t = m_Settings.theme;
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4*     c = s.Colors;

    auto V = [](const float* a, float alphaMul = 1.0f) {
        return ImVec4(a[0], a[1], a[2], a[3] * alphaMul);
    };

    c[ImGuiCol_WindowBg]         = V(t.base);
    c[ImGuiCol_ChildBg]          = V(t.surface0, 0.70f);
    c[ImGuiCol_PopupBg]          = V(t.surface1, 0.98f);
    c[ImGuiCol_Border]           = V(t.border);
    c[ImGuiCol_BorderShadow]     = ImVec4(0, 0, 0, 0);

    c[ImGuiCol_FrameBg]          = V(t.surface1);
    c[ImGuiCol_FrameBgHovered]   = V(t.surface2);
    c[ImGuiCol_FrameBgActive]    = V(t.surface3);

    c[ImGuiCol_TitleBg]          = V(t.base);
    c[ImGuiCol_TitleBgActive]    = V(t.surface0);
    c[ImGuiCol_TitleBgCollapsed] = V(t.base, 0.8f);
    c[ImGuiCol_MenuBarBg]        = V(t.base);

    c[ImGuiCol_ScrollbarBg]          = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab]        = V(t.surface3, 0.7f);
    c[ImGuiCol_ScrollbarGrabHovered] = V(t.accentDim);
    c[ImGuiCol_ScrollbarGrabActive]  = V(t.accent);

    c[ImGuiCol_CheckMark]        = V(t.accent);
    c[ImGuiCol_SliderGrab]       = V(t.accent);
    c[ImGuiCol_SliderGrabActive] = V(t.accentLight);

    c[ImGuiCol_Button]        = V(t.surface2);
    c[ImGuiCol_ButtonHovered] = V(t.accentDim);
    c[ImGuiCol_ButtonActive]  = V(t.accent);

    c[ImGuiCol_Header]        = V(t.accentFaint);
    c[ImGuiCol_HeaderHovered] = V(t.accentDim);
    c[ImGuiCol_HeaderActive]  = V(t.accent);

    c[ImGuiCol_Separator]        = V(t.border, 0.6f);
    c[ImGuiCol_SeparatorHovered] = V(t.accentDim);
    c[ImGuiCol_SeparatorActive]  = V(t.accent);

    c[ImGuiCol_ResizeGrip]        = V(t.accentFaint, 0.5f);
    c[ImGuiCol_ResizeGripHovered] = V(t.accentDim);
    c[ImGuiCol_ResizeGripActive]  = V(t.accent);

    c[ImGuiCol_Tab]                = V(t.surface1);
    c[ImGuiCol_TabHovered]         = V(t.accentDim);
    c[ImGuiCol_TabActive]          = V(t.accent);
    c[ImGuiCol_TabUnfocused]       = V(t.surface0);
    c[ImGuiCol_TabUnfocusedActive] = V(t.surface2);

    c[ImGuiCol_DockingPreview] = V(t.accent, 0.35f);
    c[ImGuiCol_DockingEmptyBg] = V(t.base);

    c[ImGuiCol_PlotLines]            = V(t.accent);
    c[ImGuiCol_PlotLinesHovered]     = V(t.accentLight);
    c[ImGuiCol_PlotHistogram]        = V(t.accent);
    c[ImGuiCol_PlotHistogramHovered] = V(t.accentLight);

    c[ImGuiCol_TableHeaderBg]     = V(t.surface1);
    c[ImGuiCol_TableBorderStrong] = V(t.border);
    c[ImGuiCol_TableBorderLight]  = V(t.borderFaint);
    c[ImGuiCol_TableRowBg]        = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt]     = V(t.surface0, 0.4f);

    c[ImGuiCol_TextSelectedBg]        = V(t.accent, 0.35f);
    c[ImGuiCol_DragDropTarget]        = V(t.accent, 0.9f);
    c[ImGuiCol_NavHighlight]          = V(t.accent);
    c[ImGuiCol_NavWindowingHighlight] = ImVec4(1, 1, 1, 0.6f);
    c[ImGuiCol_NavWindowingDimBg]     = ImVec4(0, 0, 0, 0.45f);
    c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0, 0, 0, 0.55f);

    c[ImGuiCol_Text]         = V(t.textPrimary);
    c[ImGuiCol_TextDisabled] = V(t.textFaint);

    s.WindowRounding    = t.windowRounding;
    s.ChildRounding     = t.windowRounding * 0.75f;
    s.FrameRounding     = t.frameRounding;
    s.PopupRounding     = t.windowRounding * 0.9f;
    s.ScrollbarRounding = t.windowRounding * 0.8f;
    s.GrabRounding      = t.frameRounding * 0.7f;
    s.TabRounding       = t.frameRounding;
    s.ScrollbarSize     = t.scrollbarSize;

    // Con viewports, cada ventana OS debe quedar cuadrada y opaca.
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        s.WindowRounding = 0.0f;
        c[ImGuiCol_WindowBg].w = 1.0f;
    }

    ProyecThor::UI::DS::SyncFromTheme(t);
    ProyecThor::UI::MonitorTheme::Sync(t); 
     ProyecThor::UI::HubTheme::Sync(t); 
     ProyecThor::UI::ControlTheme::Sync(t);
}

// ── Persistencia ─────────────────────────────────────────────────────────
void SettingsManager::SaveSettings() {
    json j;
    const auto& p = m_Settings.projection;
    const auto& t = m_Settings.theme;

    j["projection"]["targetMonitor"] = p.targetMonitor;
    j["projection"]["textSize"]      = p.textSize;
    j["projection"]["textColorR"]    = p.textColorR;
    j["projection"]["textColorG"]    = p.textColorG;
    j["projection"]["textColorB"]    = p.textColorB;
    j["projection"]["textColorA"]    = p.textColorA;
    j["projection"]["textAlignment"] = p.textAlignment;
    j["projection"]["vAlignment"]    = p.vAlignment;
    j["projection"]["marginTop"]     = p.marginTop;
    j["projection"]["marginBottom"]  = p.marginBottom;
    j["projection"]["marginLeft"]    = p.marginLeft;
    j["projection"]["marginRight"]   = p.marginRight;
    j["projection"]["autoScale"]     = p.autoScale;
    j["projection"]["selectedFont"]  = p.selectedFont;
    j["projection"]["defaultBgR"]    = p.defaultBgR;
    j["projection"]["defaultBgG"]    = p.defaultBgG;
    j["projection"]["defaultBgB"]    = p.defaultBgB;

    std::string langStr = "es";
    if      (m_Settings.general.language == Language::English)    langStr = "en";
    else if (m_Settings.general.language == Language::Portuguese) langStr = "pt";

    j["general"]["language"]            = langStr;
    j["general"]["dismissedChangelog"]  = m_Settings.general.dismissedChangelog;
    j["general"]["startMinimized"]      = m_Settings.general.startMinimized;
    j["general"]["rememberLayout"]      = m_Settings.general.rememberLayout;
    j["general"]["confirmOnExit"]       = m_Settings.general.confirmOnExit;
    j["general"]["autoSave"]            = m_Settings.general.autoSave;
    j["general"]["autoSaveIntervalSec"] = m_Settings.general.autoSaveIntervalSec;
    j["general"]["defaultBiblesFolder"] = m_Settings.general.defaultBiblesFolder;
    j["general"]["defaultMediaFolder"]  = m_Settings.general.defaultMediaFolder;

    j["audio"]["masterVolume"] = m_Settings.audio.masterVolume;
    j["audio"]["muted"]        = m_Settings.audio.muted;
    j["audio"]["muteOnBlank"]  = m_Settings.audio.muteOnBlank;
    j["audio"]["audioDevice"]  = m_Settings.audio.audioDevice;

    j["updates"]["checkOnStartup"] = m_Settings.updates.checkOnStartup;
    j["updates"]["autoDownload"]   = m_Settings.updates.autoDownload;
    j["updates"]["updateChannel"]  = m_Settings.updates.updateChannel;
    j["updates"]["lastChecked"]    = m_Settings.updates.lastChecked;

    j["theme"]["preset"]        = ThemePresetToKey(t.preset);
    j["theme"]["windowRounding"]= t.windowRounding;
    j["theme"]["frameRounding"] = t.frameRounding;
    j["theme"]["scrollbarSize"] = t.scrollbarSize;
    auto putCol = [&](const char* key, const float* v) {
        j["theme"][key] = { v[0], v[1], v[2], v[3] };
    };
    putCol("base", t.base); putCol("surface0", t.surface0);
    putCol("surface1", t.surface1); putCol("surface2", t.surface2);
    putCol("surface3", t.surface3); putCol("accent", t.accent);
    putCol("accentLight", t.accentLight); putCol("accentDim", t.accentDim);
    putCol("accentFaint", t.accentFaint); putCol("border", t.border);
    putCol("borderFaint", t.borderFaint); putCol("textPrimary", t.textPrimary);
    putCol("textDim", t.textDim); putCol("textFaint", t.textFaint);
    putCol("danger", t.danger); putCol("success", t.success);

    try {
        std::string path = GetSettingsPath();
        std::ofstream f(path);
        if (f.is_open()) { f << j.dump(4); }
        else std::cerr << "[Settings] No se pudo abrir para escritura: " << path << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[Settings] Error al guardar: " << e.what() << "\n";
    }
}

void SettingsManager::LoadSettings() {
    std::string path = GetSettingsPath();
    std::ifstream f(path);
    if (!f.is_open()) {
        m_Settings.theme = MakeThemePreset(ThemePreset::Dark);
        return;
    }

    try {
        json j;
        f >> j;

        if (j.contains("projection")) {
            auto& p = m_Settings.projection;
            const auto& jp = j["projection"];
            p.targetMonitor = jp.value("targetMonitor", -1);
            p.textSize      = jp.value("textSize",      48.0f);
            p.textColorR    = jp.value("textColorR",    1.0f);
            p.textColorG    = jp.value("textColorG",    1.0f);
            p.textColorB    = jp.value("textColorB",    1.0f);
            p.textColorA    = jp.value("textColorA",    1.0f);
            p.textAlignment = jp.value("textAlignment", 1);
            p.vAlignment    = jp.value("vAlignment",    1);
            p.marginTop     = jp.value("marginTop",     50.0f);
            p.marginBottom  = jp.value("marginBottom",  50.0f);
            p.marginLeft    = jp.value("marginLeft",    50.0f);
            p.marginRight   = jp.value("marginRight",   50.0f);
            p.autoScale     = jp.value("autoScale",     true);
            p.selectedFont  = jp.value("selectedFont",  "default");
            p.defaultBgR    = jp.value("defaultBgR",    0.0f);
            p.defaultBgG    = jp.value("defaultBgG",    0.0f);
            p.defaultBgB    = jp.value("defaultBgB",    0.0f);
        }

        if (j.contains("general")) {
            const auto& jg = j["general"];
            std::string langStr = jg.value("language", "es");
            if      (langStr == "en") m_Settings.general.language = Language::English;
            else if (langStr == "pt") m_Settings.general.language = Language::Portuguese;
            else                      m_Settings.general.language = Language::Spanish;

            m_Settings.general.dismissedChangelog  = jg.value("dismissedChangelog",  "");
            m_Settings.general.startMinimized      = jg.value("startMinimized",      false);
            m_Settings.general.rememberLayout      = jg.value("rememberLayout",      true);
            m_Settings.general.confirmOnExit       = jg.value("confirmOnExit",       true);
            m_Settings.general.autoSave             = jg.value("autoSave",            true);
            m_Settings.general.autoSaveIntervalSec  = jg.value("autoSaveIntervalSec", 120);
            m_Settings.general.defaultBiblesFolder  = jg.value("defaultBiblesFolder", "");
            m_Settings.general.defaultMediaFolder   = jg.value("defaultMediaFolder",  "");
        }

        if (j.contains("audio")) {
            const auto& ja = j["audio"];
            m_Settings.audio.masterVolume = ja.value("masterVolume", 100);
            m_Settings.audio.muted        = ja.value("muted",        false);
            m_Settings.audio.muteOnBlank  = ja.value("muteOnBlank",  false);
            m_Settings.audio.audioDevice  = ja.value("audioDevice",  "");
        }

        if (j.contains("updates")) {
            const auto& ju = j["updates"];
            m_Settings.updates.checkOnStartup = ju.value("checkOnStartup", true);
            m_Settings.updates.autoDownload   = ju.value("autoDownload",   false);
            m_Settings.updates.updateChannel  = ju.value("updateChannel",  "stable");
            m_Settings.updates.lastChecked    = ju.value("lastChecked",    "");
        }

        if (j.contains("theme")) {
            const auto& jt = j["theme"];
            ThemePreset preset = ThemePresetFromString(jt.value("preset", "dark"));

            // Los presets predefinidos siempre se regeneran desde código
            // (así si se ajusta un preset en una nueva versión, se actualiza).
            // "Custom" carga los colores guardados tal cual.
            if (preset == ThemePreset::Custom) {
                ThemeSettings t;
                t.preset = ThemePreset::Custom;
                auto getCol = [&](const char* key, float* out, const float* def) {
                    if (jt.contains(key) && jt[key].is_array() && jt[key].size() == 4) {
                        for (int i = 0; i < 4; i++) out[i] = jt[key][i].get<float>();
                    } else {
                        for (int i = 0; i < 4; i++) out[i] = def[i];
                    }
                };
                getCol("base", t.base, t.base); getCol("surface0", t.surface0, t.surface0);
                getCol("surface1", t.surface1, t.surface1); getCol("surface2", t.surface2, t.surface2);
                getCol("surface3", t.surface3, t.surface3); getCol("accent", t.accent, t.accent);
                getCol("accentLight", t.accentLight, t.accentLight); getCol("accentDim", t.accentDim, t.accentDim);
                getCol("accentFaint", t.accentFaint, t.accentFaint); getCol("border", t.border, t.border);
                getCol("borderFaint", t.borderFaint, t.borderFaint); getCol("textPrimary", t.textPrimary, t.textPrimary);
                getCol("textDim", t.textDim, t.textDim); getCol("textFaint", t.textFaint, t.textFaint);
                getCol("danger", t.danger, t.danger); getCol("success", t.success, t.success);
                t.windowRounding = jt.value("windowRounding", t.windowRounding);
                t.frameRounding  = jt.value("frameRounding",  t.frameRounding);
                t.scrollbarSize  = jt.value("scrollbarSize",  t.scrollbarSize);
                m_Settings.theme = t;
            } else {
                m_Settings.theme = MakeThemePreset(preset);
            }
        } else {
            m_Settings.theme = MakeThemePreset(ThemePreset::Dark);
        }

    } catch (const std::exception& e) {
        std::cerr << "[Settings] Error al cargar: " << e.what() << "\n";
        m_Settings.theme = MakeThemePreset(ThemePreset::Dark);
    }
}

} // namespace ProyecThor::Settings