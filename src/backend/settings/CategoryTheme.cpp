#include "SettingsPanel.h"
#include "SettingsManager.h"
#include <imgui.h>
#include <string>

namespace ProyecThor::UI::Settings {

using namespace ProyecThor::Settings;

// Botón cuadrado con el color de acento del preset, usado como swatch.
static bool PresetSwatch(const char* label, ThemePreset preset, ThemePreset active) {
    ThemeSettings preview = MakeThemePreset(preset);
    ImVec4 accent = ImVec4(preview.accent[0], preview.accent[1], preview.accent[2], 1.0f);
    ImVec4 base   = ImVec4(preview.base[0],   preview.base[1],   preview.base[2],   1.0f);

    bool selected = (preset == active);

    ImGui::PushID(label);
    ImGui::BeginGroup();

    ImGui::PushStyleColor(ImGuiCol_Button, base);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(base.x+0.05f, base.y+0.05f, base.z+0.05f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, base);
    ImGui::PushStyleColor(ImGuiCol_Border, selected ? accent : ImVec4(1,1,1,0.12f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, selected ? 2.0f : 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);

    bool clicked = ImGui::Button("##swatch", ImVec2(52, 52));

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetItemRectMin();
    dl->AddCircleFilled(ImVec2(p.x + 38, p.y + 14), 8.0f, ImGui::ColorConvertFloat4ToU32(accent));

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);

    ImGui::TextUnformatted(label);
    ImGui::EndGroup();
    ImGui::PopID();

    return clicked;
}

void SettingsPanel::RenderCategoryTheme() {
    auto& theme = ProyecThor::Settings::SettingsManager::Get().GetSettings().theme;

    ImGui::TextDisabled("Elige un tema predeterminado o personaliza los colores.");
    ImGui::Spacing();

    SectionTitle("Temas predeterminados");

    struct PresetEntry { const char* label; ThemePreset preset; };
    static const PresetEntry presets[] = {
        { "Oscuro",           ThemePreset::Dark        },
        { "Claro",            ThemePreset::Light       },
        { "Naranja y Negro",  ThemePreset::OrangeBlack },
        { "Jazz",             ThemePreset::Jazz        },
        { "Ko-fi",            ThemePreset::Kofi        },
        { "Verde",            ThemePreset::Deadlock    },
        { "Galaxia",          ThemePreset::Galaxy      },
    };

    int perRow = std::max(1, (int)(ImGui::GetContentRegionAvail().x / 90.0f));
    for (int i = 0; i < (int)(sizeof(presets) / sizeof(presets[0])); i++) {
        if (PresetSwatch(presets[i].label, presets[i].preset, theme.preset)) {
            ProyecThor::Settings::SettingsManager::Get().ApplyPreset(presets[i].preset);
        }
        if ((i + 1) % perRow != 0) ImGui::SameLine();
    }

    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    ImGui::TextColored(ImVec4(0.6f, 0.75f, 0.9f, 1.0f), "Preset activo: %s",
        ProyecThor::Settings::ThemePresetName(theme.preset));

    ImGui::Dummy(ImVec2(0.0f, 15.0f));

    SectionTitle("Personalizar colores");
    ImGui::TextDisabled("Editar cualquier color aquí lo marca como tema \"Personalizado\".");
    ImGui::Spacing();

    bool changed = false;
    static ImGuiColorEditFlags flags =
        ImGuiColorEditFlags_AlphaBar |
        ImGuiColorEditFlags_AlphaPreviewHalf |
        ImGuiColorEditFlags_NoInputs;

    SectionTitle("Fondos y superficies");
    changed |= ImGui::ColorEdit4("Fondo principal##base", theme.base, flags);
    HelpTooltip("Color de ventanas principales.");
    changed |= ImGui::ColorEdit4("Superficie 0##s0", theme.surface0, flags);
    changed |= ImGui::ColorEdit4("Superficie 1##s1", theme.surface1, flags);
    changed |= ImGui::ColorEdit4("Superficie 2##s2", theme.surface2, flags);
    changed |= ImGui::ColorEdit4("Superficie 3##s3", theme.surface3, flags);

    SectionTitle("Acento");
    changed |= ImGui::ColorEdit4("Acento##ac",       theme.accent,      flags);
    changed |= ImGui::ColorEdit4("Acento claro##acl",theme.accentLight, flags);
    changed |= ImGui::ColorEdit4("Acento oscuro##acd",theme.accentDim,  flags);
    changed |= ImGui::ColorEdit4("Acento tenue##acf",theme.accentFaint, flags);

    SectionTitle("Bordes");
    changed |= ImGui::ColorEdit4("Borde##bd",        theme.border,      flags);
    changed |= ImGui::ColorEdit4("Borde tenue##bdf",  theme.borderFaint, flags);

    SectionTitle("Texto");
    changed |= ImGui::ColorEdit4("Texto principal##tp",   theme.textPrimary, flags);
    changed |= ImGui::ColorEdit4("Texto secundario##td",  theme.textDim,     flags);
    changed |= ImGui::ColorEdit4("Texto inactivo##tf",    theme.textFaint,   flags);

    SectionTitle("Estados");
    changed |= ImGui::ColorEdit4("Error##dg",   theme.danger,  flags);
    changed |= ImGui::ColorEdit4("Éxito##sc",   theme.success, flags);

    SectionTitle("Forma");
    changed |= ImGui::SliderFloat("Redondeo de ventanas", &theme.windowRounding, 0.0f, 24.0f, "%.0f");
    changed |= ImGui::SliderFloat("Redondeo de controles", &theme.frameRounding, 0.0f, 16.0f, "%.0f");
    changed |= ImGui::SliderFloat("Grosor de scrollbar",   &theme.scrollbarSize, 4.0f, 16.0f, "%.0f");

    if (changed) {
        theme.preset = ThemePreset::Custom;
        ProyecThor::Settings::SettingsManager::Get().ApplyTheme(); // preview en vivo
    }

    ImGui::Dummy(ImVec2(0.0f, 15.0f));

    // ── Colores de categorías (sidebar de Biblioteca) ───────────────────────
    // Independiente del tema general: solo afecta el color de identidad de
    // cada categoría en el sidebar izquierdo de la Biblioteca (Letra/Video/
    // Imagen/Biblia/Documentos/Audio). Ver LibrarySidebar.cpp.
    SectionTitle("Colores de categorías (Biblioteca)");
    ImGui::TextDisabled("Color de identidad de cada categoría en el sidebar de la Biblioteca.");
    ImGui::Spacing();

    auto& sidebar = ProyecThor::Settings::SettingsManager::Get().GetSettings().librarySidebar;
    static const char* kCatLabels[6] = { "Letra", "Video", "Imagen", "Biblia", "Documentos", "Audio" };
    bool sidebarChanged = false;
    for (int i = 0; i < 6; i++) {
        std::string id = std::string(kCatLabels[i]) + "##libcat" + std::to_string(i);
        sidebarChanged |= ImGui::ColorEdit4(id.c_str(), sidebar.categoryColor[i], flags);
    }
    if (sidebarChanged) {
        ProyecThor::Settings::SettingsManager::Get().Save();
    }

    ImGui::Dummy(ImVec2(0.0f, 15.0f));

    // ── Colores de categorías (sidebar de Home) ─────────────────────────────
    // Independiente del tema general: solo afecta el color de identidad de
    // cada sección en el sidebar de Home (Home/Reloj/Anuncios/Notas/Captura/
    // Transmisión). Ver HomeSidebar.cpp.
    SectionTitle("Colores de categorías (Home)");
    ImGui::TextDisabled("Color de identidad de cada sección en el sidebar de Home.");
    ImGui::Spacing();

    auto& homeSidebar = ProyecThor::Settings::SettingsManager::Get().GetSettings().homeSidebar;
    static const char* kHomeCatLabels[6] = {
        "Home", "Reloj y Contadores", "Anuncios", "Notas Rápidas", "Captura", "Transmisión en Red"
    };
    bool homeSidebarChanged = false;
    for (int i = 0; i < 6; i++) {
        std::string id = std::string(kHomeCatLabels[i]) + "##homecat" + std::to_string(i);
        homeSidebarChanged |= ImGui::ColorEdit4(id.c_str(), homeSidebar.categoryColor[i], flags);
    }
    if (homeSidebarChanged) {
        ProyecThor::Settings::SettingsManager::Get().Save();
    }

    ImGui::Dummy(ImVec2(0.0f, 15.0f));

    // ── Colores de categorías (hub de Control) ──────────────────────────────
    SectionTitle("Colores de categorías (Control)");
    ImGui::TextDisabled("Color de identidad de cada sección en el sidebar de Control.");
    ImGui::Spacing();

    auto& controlHub = ProyecThor::Settings::SettingsManager::Get().GetSettings().controlHub;
    static const char* kControlCatLabels[2] = { "Control", "Stage Display" };
    bool controlHubChanged = false;
    for (int i = 0; i < 2; i++) {
        std::string id = std::string(kControlCatLabels[i]) + "##controlcat" + std::to_string(i);
        controlHubChanged |= ImGui::ColorEdit4(id.c_str(), controlHub.categoryColor[i], flags);
    }
    if (controlHubChanged) {
        ProyecThor::Settings::SettingsManager::Get().Save();
    }

    ImGui::Dummy(ImVec2(0.0f, 15.0f));

    // ── Colores de categorías (hub de Diseño) ───────────────────────────────
    SectionTitle("Colores de categorías (Diseño)");
    ImGui::TextDisabled("Color de identidad de cada sección en el sidebar de Diseño.");
    ImGui::Spacing();

    auto& stylesHub = ProyecThor::Settings::SettingsManager::Get().GetSettings().stylesHub;
    static const char* kStylesCatLabels[3] = { "Fondos", "Estilos", "Transiciones" };
    bool stylesHubChanged = false;
    for (int i = 0; i < 3; i++) {
        std::string id = std::string(kStylesCatLabels[i]) + "##stylescat" + std::to_string(i);
        stylesHubChanged |= ImGui::ColorEdit4(id.c_str(), stylesHub.categoryColor[i], flags);
    }
    if (stylesHubChanged) {
        ProyecThor::Settings::SettingsManager::Get().Save();
    }
}

} // namespace ProyecThor::UI::Settings