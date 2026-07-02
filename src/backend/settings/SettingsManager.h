#pragma once
#include <string>
#include <imgui.h>
#include "Version.h"

namespace ProyecThor::Settings {

    // =========================================================================
    //  Projection Settings
    // =========================================================================
    struct ProjectionSettings {
        int   targetMonitor   = -1;   // -1 = auto (secundario si existe)
        int   outputWidth     = 0;    // 0 = nativa del monitor
        int   outputHeight    = 0;
        float contentScale    = 1.0f;
        float marginTop       = 0.05f;
        float marginBottom    = 0.05f;
        float marginLeft      = 0.05f;
        float marginRight     = 0.05f;
        int   aspectRatioMode = 1;    // 0=libre,1=16:9,2=4:3,3=21:9,4=custom
        float customAspectW   = 16.0f;
        float customAspectH   = 9.0f;
        float defaultBgR      = 0.0f;
        float defaultBgG      = 0.0f;
        float defaultBgB      = 0.0f;
        float textSize        = 48.0f;
        float textColorR      = 1.0f;
        float textColorG      = 1.0f;
        float textColorB      = 1.0f;
        int vAlignment = 1;
        float textColorA      = 1.0f;
        int   textAlignment   = 1;    // 0=izq, 1=centro, 2=der
        bool  autoScale       = true;

        std::string selectedFont = "Arial.ttf";
        
        float lineSpacing     = 1.2f;
        bool  fadeEnabled     = true;
        float fadeDuration    = 0.3f;
        bool  vsync           = true;
        int   targetFPS       = 60;
    };

    // =========================================================================
    //  Audio Settings
    // =========================================================================
    struct AudioSettings {
        int         masterVolume     = 100;
        int         previewVolume    = 80;
        bool        muted            = false;
        bool        muteOnStop       = true;
        bool        muteOnBlank      = false;
        bool        fadeOnTransition = true;
        float       fadeDuration     = 0.5f;
        std::string audioDevice      = "";
    };

    // =========================================================================
    //  General Settings
    // =========================================================================
    enum class Language { Spanish = 0, English = 1, Portuguese = 2, COUNT };

    inline const char* LanguageName(Language lang) {
        switch (lang) {
            case Language::Spanish:    return "Espanol";
            case Language::English:    return "English";
            case Language::Portuguese: return "Portugues";
            default:                   return "Unknown";
        }
    }

    struct GeneralSettings {
        bool        startMinimized      = false;
        bool        rememberLayout      = true;
        bool        confirmOnExit       = true;
        bool        autoSave            = true;
        int         autoSaveIntervalSec = 120;
        std::string defaultBiblesFolder = "";
        std::string defaultMediaFolder  = "";
        Language    language            = Language::Spanish;
        std::string dismissedChangelog = "";
    };

    // =========================================================================
    //  Theme Settings — float[4] = {R,G,B,A} compatible con ColorEdit4
    // =========================================================================
    struct ThemeSettings {
        float base[4]        = { 0.08f, 0.08f, 0.10f, 1.0f };
        float surface0[4]    = { 0.12f, 0.12f, 0.15f, 1.0f };
        float surface1[4]    = { 0.16f, 0.16f, 0.20f, 1.0f };
        float surface2[4]    = { 0.20f, 0.20f, 0.25f, 1.0f };
        float surface3[4]    = { 0.24f, 0.24f, 0.30f, 1.0f };
        float accent[4]      = { 0.19f, 0.66f, 1.00f, 1.0f };
        float accentLight[4] = { 0.39f, 0.78f, 1.00f, 1.0f };
        float accentDim[4]   = { 0.12f, 0.45f, 0.75f, 1.0f };
        float accentFaint[4] = { 0.10f, 0.30f, 0.50f, 1.0f };
        float border[4]      = { 0.30f, 0.30f, 0.38f, 1.0f };
        float borderFaint[4] = { 0.18f, 0.18f, 0.22f, 1.0f };
        float textPrimary[4] = { 0.92f, 0.92f, 0.95f, 1.0f };
        float textDim[4]     = { 0.60f, 0.60f, 0.65f, 1.0f };
        float textFaint[4]   = { 0.35f, 0.35f, 0.40f, 1.0f };
    };

    // =========================================================================
    //  Updates Settings
    // =========================================================================
    struct UpdatesSettings {
        std::string currentVersion = PROYECTHOR_VERSION_STRING;
        std::string lastChecked    = "";
        std::string updateChannel  = "beta";
        bool        checkOnStartup = true;
        bool        autoDownload   = false;
    };

    //STABLE BETA DEFINCIONES


#pragma once
#include <string>

namespace ProyecThor::Settings {

    enum class Language {
        English = 0,
        Spanish = 1,
        Portuguese = 2,
        COUNT = 3
    };

    inline const char* LanguageName(Language l) {
        switch (l) {
            case Language::English: return "English";
            case Language::Spanish: return "Español";
            case Language::Portuguese: return "Português";
            default: return "Unknown";
        }
    }

    struct GeneralSettings {
        Language language = Language::English; // Inglés por defecto
    };

    struct AppSettings {
        GeneralSettings general;
    };

    class SettingsManager {
    public:
        static SettingsManager& Get() {
            static SettingsManager instance;
            return instance;
        }

        AppSettings& GetSettings() { return m_Settings; }

        void Load();
        void Save();

    private:
        SettingsManager() { Load(); }
        AppSettings m_Settings;
    };

} // namespace ProyecThor::Settings











    // =========================================================================
    //  AppSettings — todos los structs DEBEN declararse antes de esta linea
    // =========================================================================
    struct AppSettings {
        ProjectionSettings projection;
        AudioSettings      audio;
        GeneralSettings    general;
        ThemeSettings      theme;
        UpdatesSettings    updates;
    };

    // =========================================================================
    //  SettingsManager — Singleton
    // =========================================================================
    class SettingsManager {
    public:
        static SettingsManager& Get() {
            static SettingsManager instance;
            return instance;
        }

        AppSettings&       GetSettings()       { return m_Settings; }
        const AppSettings& GetSettings() const { return m_Settings; }

        void SaveSettings();
        void LoadSettings();
        void ResetToDefaults() { m_Settings = AppSettings{}; }

        // Alias corto para la UI
        void Save() { SaveSettings(); }

        // ---------------------------------------------------------------------
        //  ApplyTheme — propaga ThemeSettings a ImGui en tiempo real
        // ---------------------------------------------------------------------
        void ApplyTheme() {
            const auto& t = m_Settings.theme;
            ImGuiStyle& s = ImGui::GetStyle();
            s.Colors[ImGuiCol_WindowBg]          = ImVec4(t.base[0],        t.base[1],        t.base[2],        t.base[3]);
            s.Colors[ImGuiCol_ChildBg]            = ImVec4(t.surface0[0],    t.surface0[1],    t.surface0[2],    t.surface0[3]);
            s.Colors[ImGuiCol_PopupBg]            = ImVec4(t.surface1[0],    t.surface1[1],    t.surface1[2],    t.surface1[3]);
            s.Colors[ImGuiCol_FrameBg]            = ImVec4(t.surface1[0],    t.surface1[1],    t.surface1[2],    t.surface1[3]);
            s.Colors[ImGuiCol_FrameBgHovered]     = ImVec4(t.surface2[0],    t.surface2[1],    t.surface2[2],    t.surface2[3]);
            s.Colors[ImGuiCol_FrameBgActive]      = ImVec4(t.surface3[0],    t.surface3[1],    t.surface3[2],    t.surface3[3]);
            s.Colors[ImGuiCol_TitleBg]            = ImVec4(t.surface0[0],    t.surface0[1],    t.surface0[2],    t.surface0[3]);
            s.Colors[ImGuiCol_TitleBgActive]      = ImVec4(t.surface1[0],    t.surface1[1],    t.surface1[2],    t.surface1[3]);
            s.Colors[ImGuiCol_MenuBarBg]          = ImVec4(t.surface0[0],    t.surface0[1],    t.surface0[2],    t.surface0[3]);
            s.Colors[ImGuiCol_Header]             = ImVec4(t.accentFaint[0], t.accentFaint[1], t.accentFaint[2], t.accentFaint[3]);
            s.Colors[ImGuiCol_HeaderHovered]      = ImVec4(t.accentDim[0],   t.accentDim[1],   t.accentDim[2],   t.accentDim[3]);
            s.Colors[ImGuiCol_HeaderActive]       = ImVec4(t.accent[0],      t.accent[1],      t.accent[2],      t.accent[3]);
            s.Colors[ImGuiCol_Button]             = ImVec4(t.surface2[0],    t.surface2[1],    t.surface2[2],    t.surface2[3]);
            s.Colors[ImGuiCol_ButtonHovered]      = ImVec4(t.accentDim[0],   t.accentDim[1],   t.accentDim[2],   t.accentDim[3]);
            s.Colors[ImGuiCol_ButtonActive]       = ImVec4(t.accent[0],      t.accent[1],      t.accent[2],      t.accent[3]);
            s.Colors[ImGuiCol_SliderGrab]         = ImVec4(t.accent[0],      t.accent[1],      t.accent[2],      t.accent[3]);
            s.Colors[ImGuiCol_SliderGrabActive]   = ImVec4(t.accentLight[0], t.accentLight[1], t.accentLight[2], t.accentLight[3]);
            s.Colors[ImGuiCol_CheckMark]          = ImVec4(t.accent[0],      t.accent[1],      t.accent[2],      t.accent[3]);
            s.Colors[ImGuiCol_Separator]          = ImVec4(t.border[0],      t.border[1],      t.border[2],      t.border[3]);
            s.Colors[ImGuiCol_Border]             = ImVec4(t.border[0],      t.border[1],      t.border[2],      t.border[3]);
            s.Colors[ImGuiCol_Text]               = ImVec4(t.textPrimary[0], t.textPrimary[1], t.textPrimary[2], t.textPrimary[3]);
            s.Colors[ImGuiCol_TextDisabled]       = ImVec4(t.textFaint[0],   t.textFaint[1],   t.textFaint[2],   t.textFaint[3]);
            s.Colors[ImGuiCol_Tab]                = ImVec4(t.surface1[0],    t.surface1[1],    t.surface1[2],    t.surface1[3]);
            s.Colors[ImGuiCol_TabHovered]         = ImVec4(t.accentDim[0],   t.accentDim[1],   t.accentDim[2],   t.accentDim[3]);
            s.Colors[ImGuiCol_TabActive]          = ImVec4(t.accent[0],      t.accent[1],      t.accent[2],      t.accent[3]);
            s.Colors[ImGuiCol_TabUnfocused]       = ImVec4(t.surface0[0],    t.surface0[1],    t.surface0[2],    t.surface0[3]);
            s.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(t.surface2[0],    t.surface2[1],    t.surface2[2],    t.surface2[3]);
        }

        // ---------------------------------------------------------------------
        //  ApplyProjection — propaga ProjectionSettings a PresentationCore
        //  Implementado en SettingsManager.cpp para evitar include circular.
        // ---------------------------------------------------------------------
        void ApplyProjection();

    private:
        SettingsManager()                                  = default;
        ~SettingsManager()                                 = default;
        SettingsManager(const SettingsManager&)            = delete;
        SettingsManager& operator=(const SettingsManager&) = delete;

        AppSettings m_Settings;
    };

} // namespace ProyecThor::Settings
