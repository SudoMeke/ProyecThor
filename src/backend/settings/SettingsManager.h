#pragma once
#include <string>
#include <imgui.h>
#include "Version.h"
#include "StageLayoutTemplates.h"

namespace ProyecThor::Settings {

    // ── Idiomas ──────────────────────────────────────────────────────────
    enum class Language { Spanish = 0, English, Portuguese, COUNT };

    inline const char* LanguageName(Language l) {
        switch (l) {
            case Language::Spanish:    return "Español";
            case Language::English:    return "English";
            case Language::Portuguese: return "Português";
            default:                   return "Español";
        }
    }

    // ── Proyección ───────────────────────────────────────────────────────
    struct ProjectionSettings {
        int   targetMonitor   = -1;
        int   outputWidth     = 0;
        int   outputHeight    = 0;
        float contentScale    = 1.0f;
        float marginTop       = 50.0f;
        float marginBottom    = 50.0f;
        float marginLeft      = 50.0f;
        float marginRight     = 50.0f;
        int   aspectRatioMode = 1;
        float customAspectW   = 16.0f;
        float customAspectH   = 9.0f;
        float defaultBgR      = 0.0f;
        float defaultBgG      = 0.0f;
        float defaultBgB      = 0.0f;
        float textSize        = 48.0f;
        float textColorR      = 1.0f;
        float textColorG      = 1.0f;
        float textColorB      = 1.0f;
        float textColorA      = 1.0f;
        int   textAlignment   = 1;
        int   vAlignment      = 1;
        bool  autoScale       = true;

        std::string selectedFont = "Arial.ttf";

        float lineSpacing  = 1.2f;
        bool  fadeEnabled  = true;
        float fadeDuration = 0.3f;
        bool  vsync        = true;
        int   targetFPS    = 60;

        // ── Calidad de salida (video de fondo) ──────────────────────────
        // outputWidth/outputHeight/targetFPS (arriba) se reutilizan como el
        // tamano/fps del modo Custom. outputQualityMode: 0=Auto, 1=Preset,
        // 2=Custom (ver ProjectionQualityPresets.h).
        int   outputQualityMode = 0;
        int   outputPresetIndex = 3; // default: "1080p / 60 FPS"
    };

    // ── Audio ────────────────────────────────────────────────────────────
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

    // ── General ──────────────────────────────────────────────────────────
    struct GeneralSettings {
        bool        startMinimized      = false;
        bool        rememberLayout      = true;
        bool        confirmOnExit       = true;
        bool        autoSave            = true;
        int         autoSaveIntervalSec = 120;
        std::string defaultBiblesFolder = "";
        std::string defaultMediaFolder  = "";
        Language    language            = Language::Spanish;
        std::string dismissedChangelog  = "";
    };

    // ── Tema ─────────────────────────────────────────────────────────────
    // Set reducido de tokens de diseño. ApplyTheme() los expande a todos
    // los colores de ImGui, así que un solo token cambia toda la app.
    enum class ThemePreset {
        Dark, Light, OrangeBlack, Jazz, Kofi, Deadlock, Galaxy, Custom
    };

    const char* ThemePresetName(ThemePreset preset);
    ThemePreset ThemePresetFromString(const std::string& s);

    struct ThemeSettings {
        ThemePreset preset = ThemePreset::Dark;

        float base[4]        = { 0.036f, 0.040f, 0.060f, 1.0f }; // ventana principal
        float surface0[4]    = { 0.060f, 0.065f, 0.090f, 1.0f }; // paneles hijos
        float surface1[4]    = { 0.080f, 0.085f, 0.115f, 1.0f }; // popups / inputs
        float surface2[4]    = { 0.110f, 0.115f, 0.150f, 1.0f }; // hover
        float surface3[4]    = { 0.140f, 0.145f, 0.185f, 1.0f }; // active

        float accent[4]      = { 0.369f, 0.420f, 1.000f, 1.0f };
        float accentLight[4] = { 0.520f, 0.575f, 1.000f, 1.0f };
        float accentDim[4]   = { 0.250f, 0.290f, 0.700f, 1.0f };
        float accentFaint[4] = { 0.369f, 0.420f, 1.000f, 0.18f };

        float border[4]      = { 1.000f, 1.000f, 1.000f, 0.08f };
        float borderFaint[4] = { 1.000f, 1.000f, 1.000f, 0.04f };

        float textPrimary[4] = { 0.920f, 0.930f, 0.960f, 1.0f };
        float textDim[4]     = { 0.700f, 0.720f, 0.780f, 1.0f };
        float textFaint[4]   = { 1.000f, 1.000f, 1.000f, 0.28f };

        float danger[4]      = { 0.940f, 0.350f, 0.390f, 1.0f };
        float success[4]     = { 0.320f, 0.880f, 0.630f, 1.0f };

        float windowRounding = 14.0f;
        float frameRounding  =  9.0f;
        float scrollbarSize  =  8.0f;
    };

    ThemeSettings MakeThemePreset(ThemePreset preset);

    // ── Actualizaciones ──────────────────────────────────────────────────
    struct UpdatesSettings {
        std::string currentVersion = PROYECTHOR_VERSION_STRING;
        std::string lastChecked    = "";
        std::string updateChannel  = "beta";
        bool        checkOnStartup = true;
        bool        autoDownload   = false;
    };

    // ── Stage Display (monitor de control) ──────────────────────────────
    struct StageDisplaySettings {
        int layoutTemplateIndex = 0; // indice en kStageLayoutTemplates
        int cellWidget[kStageMaxCells] = {
            (int)StageWidgetType::LiveText, (int)StageWidgetType::Clock, 0, 0
        };
    };

    // ── Sidebar de Biblioteca (Letra/Video/Imagen/Biblia/Doc/Audio) ──────
    // Un color de identidad por categoria; el resto del look (fondo activo,
    // barra lateral, tinte de icono/label) se deriva de este en tiempo real
    // (ver LibrarySidebar.cpp). Los valores por defecto son los mismos tonos
    // que ya se usaban hardcodeados, para no cambiar nada hasta que el
    // usuario decida personalizar.
    struct LibrarySidebarSettings {
        float categoryColor[6][4] = {
            { 0.31f, 0.55f, 1.00f, 1.0f }, // Letra
            { 0.86f, 0.24f, 0.24f, 1.0f }, // Video
            { 0.24f, 0.86f, 0.39f, 1.0f }, // Imagen
            { 0.86f, 0.67f, 0.16f, 1.0f }, // Biblia
            { 0.65f, 0.31f, 0.94f, 1.0f }, // Documentos
            { 0.16f, 0.75f, 0.75f, 1.0f }, // Audio
        };
    };

    // ── Sidebar de Home (Home/Reloj/Anuncios/Notas/Captura/Transmision) ──
    // Mismo mecanismo que LibrarySidebarSettings: un color de identidad por
    // seccion, ver HomeSidebar.cpp.
    struct HomeSidebarSettings {
        float categoryColor[6][4] = {
            { 0.55f, 0.60f, 0.68f, 1.0f }, // Home
            { 0.95f, 0.75f, 0.20f, 1.0f }, // Reloj y Contadores
            { 0.45f, 0.60f, 1.00f, 1.0f }, // Anuncios
            { 0.35f, 0.80f, 0.55f, 1.0f }, // Notas Rapidas
            { 0.90f, 0.35f, 0.45f, 1.0f }, // Captura
            { 0.30f, 0.80f, 0.85f, 1.0f }, // Transmision en Red
        };
    };

    // ── Sidebar del hub de Control (Control/Stage Display) ───────────────
    struct ControlHubSettings {
        float categoryColor[2][4] = {
            { 0.40f, 0.55f, 0.95f, 1.0f }, // Control
            { 0.90f, 0.55f, 0.20f, 1.0f }, // Stage Display
        };
    };

    // ── Sidebar del hub de Diseño (Fondos/Estilos/Transiciones) ──────────
    struct StylesHubSettings {
        float categoryColor[3][4] = {
            { 0.35f, 0.80f, 0.55f, 1.0f }, // Fondos
            { 0.65f, 0.31f, 0.94f, 1.0f }, // Estilos
            { 0.90f, 0.35f, 0.45f, 1.0f }, // Transiciones
        };
    };

    struct AppSettings {
        ProjectionSettings     projection;
        AudioSettings          audio;
        GeneralSettings        general;
        ThemeSettings          theme;
        UpdatesSettings        updates;
        StageDisplaySettings   stageDisplay;
        LibrarySidebarSettings librarySidebar;
        HomeSidebarSettings    homeSidebar;
        ControlHubSettings     controlHub;
        StylesHubSettings      stylesHub;
    };

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
        void Save() { SaveSettings(); }

        // Aplica el tema activo (m_Settings.theme) a todo ImGui y a
        // DesignSystem (paneles "glass"). Se llama al iniciar y al guardar.
        void ApplyTheme();

        // Aplica un preset y lo deja como tema activo (sin guardar a disco).
        void ApplyPreset(ThemePreset preset) {
            m_Settings.theme = MakeThemePreset(preset);
            ApplyTheme();
        }

        void ApplyProjection();

    private:
        SettingsManager()                                  = default;
        ~SettingsManager()                                 = default;
        SettingsManager(const SettingsManager&)            = delete;
        SettingsManager& operator=(const SettingsManager&) = delete;

        AppSettings m_Settings;
    };

} // namespace ProyecThor::Settings