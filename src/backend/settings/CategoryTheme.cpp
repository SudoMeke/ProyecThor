#include "SettingsPanel.h"
#include "SettingsManager.h"
#include "backend/core/PresentationCore.h"
#include "backend/core/AppPaths.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <cmath>
#include <GL/glew.h>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#else
#include <cstdio>
#include <array>
#endif

extern GLuint LoadTextureFromFile(const char* filename);

namespace ProyecThor::UI::Settings {

using namespace ProyecThor::Settings;

#ifndef _WIN32
// Selector de archivos para Linux/macOS: no hay dialogo nativo unico en
// estos sistemas, asi que se delega en zenity/kdialog (lo que este
// instalado). Mismo enfoque que TabTypography::OpenFontFileDialogUnix
// (reimplementado localmente aca, no se comparte cabecera entre ambos por
// ser un helper chico y de un solo uso en cada archivo).
static std::string OpenFontFileDialogUnix() {
    const char* commands[] = {
        "zenity --file-selection --title=\"Seleccionar fuente de la interfaz\" "
        "--file-filter=\"Fuentes | *.ttf *.otf *.ttc\" 2>/dev/null",
        "kdialog --getopenfilename . \"*.ttf *.otf *.ttc|Fuentes\" 2>/dev/null"
    };

    for (const char* cmd : commands) {
        std::array<char, 1024> buffer{};
        std::string result;

        FILE* pipe = popen(cmd, "r");
        if (!pipe) continue;

        while (fgets(buffer.data(), (int)buffer.size(), pipe) != nullptr)
            result += buffer.data();

        int status = pclose(pipe);
        if (status != 0) continue; // el usuario cancelo o la herramienta no existe

        while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();

        if (!result.empty())
            return result;
    }
    return {};
}
#endif

// Abre el dialogo nativo (Windows) o zenity/kdialog (Linux/macOS) para
// elegir un archivo de fuente. Devuelve la ruta absoluta, o vacio si el
// usuario cancelo / no hay herramienta disponible.
static std::string PickFontFileDialog() {
#ifdef _WIN32
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn       = {};
    ofn.lStructSize         = sizeof(ofn);
    ofn.hwndOwner           = NULL;
    ofn.lpstrFilter         = "Fuentes\0*.ttf;*.otf;*.ttc\0Todos los archivos\0*.*\0";
    ofn.lpstrFile           = filename;
    ofn.nMaxFile            = MAX_PATH;
    ofn.Flags               = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameA(&ofn)) return {};
    return filename;
#else
    return OpenFontFileDialogUnix();
#endif
}

// Resuelve un nombre de fuente (stem, sin extension) a su ruta completa
// dentro de assets/fonts. Misma lógica que
// PresentationCore::ResolveFontFilePath, reimplementada acá porque ese
// método es privado (solo lo usa PresentationCore internamente para
// resolver la fuente activa de las diapositivas).
static std::string ResolveFontPathByName(const std::string& fontName) {
    if (fontName.empty() || fontName == "Predeterminada") return "";

    std::filesystem::path fontsDir = std::filesystem::path(ProyecThor::GetAssetsPath()) / "fonts";
    for (const char* ext : { ".ttf", ".otf", ".ttc" }) {
        std::filesystem::path candidate = fontsDir / (fontName + ext);
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec))
            return candidate.string();
    }
    return "";
}

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

// Textura decorativa junto al nombre del preset activo. Se carga una sola
// vez (cacheada) desde bin/assets/ui/textures -- misma convencion de rutas
// relativas al ejecutable que ya usan los iconos de StyleGeneralApp.
static GLuint GetThemeShowcaseTexture() {
    static GLuint texId = 0;
    static bool   tried = false;
    if (!tried) {
        tried = true;
        texId = LoadTextureFromFile("bin/assets/ui/textures/20260524_104505.jpg");
    }
    return texId;
}

// Tarjeta con inclinacion 3D al estilo "tilt" de sitios web (vanilla-tilt.js
// y similares): en reposo queda plana, y solo mientras el mouse esta encima
// las esquinas se distorsionan en perspectiva segun la posicion del cursor
// dentro de la tarjeta, con una sombra que se despega y un brillo diagonal
// que sigue la inclinacion. Todo interpolado cuadro a cuadro (no salta de
// golpe entre plano <-> inclinado), y vuelve a quedar plana en cuanto el
// mouse se va -- no hay animacion en reposo.
static void RenderTiltTextureCard(ImVec2 size) {
    GLuint texId = GetThemeShowcaseTexture();

    ImGuiID   id = ImGui::GetID("##themeTiltCard");
    ImVec2    p0 = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##themeTiltHit", size);
    bool hovered = ImGui::IsItemHovered();

    ImGuiStorage* storage = ImGui::GetStateStorage();
    float* tiltX = storage->GetFloatRef(id ^ 0x54494C31u, 0.0f); // "TIL1"
    float* tiltY = storage->GetFloatRef(id ^ 0x54494C32u, 0.0f); // "TIL2"
    float* lift  = storage->GetFloatRef(id ^ 0x54494C33u, 0.0f); // "TIL3"

    ImVec2 center = ImVec2(p0.x + size.x * 0.5f, p0.y + size.y * 0.5f);
    ImVec2 mouse  = ImGui::GetIO().MousePos;
    float nx = hovered ? std::clamp((mouse.x - center.x) / (size.x * 0.5f), -1.0f, 1.0f) : 0.0f;
    float ny = hovered ? std::clamp((mouse.y - center.y) / (size.y * 0.5f), -1.0f, 1.0f) : 0.0f;

    const float speed = std::min(1.0f, ImGui::GetIO().DeltaTime * 10.0f);
    *tiltX += (nx - *tiltX) * speed;
    *tiltY += (ny - *tiltY) * speed;
    *lift  += ((hovered ? 1.0f : 0.0f) - *lift) * speed;

    const float maxAngle = 0.20f; // ~11.5 grados
    float rotY =  (*tiltX) * maxAngle; // giro izquierda/derecha
    float rotX = -(*tiltY) * maxAngle; // giro arriba/abajo
    const float focal = 480.0f;

    float halfW = size.x * 0.5f, halfH = size.y * 0.5f;
    ImVec2 local[4] = {
        ImVec2(-halfW, -halfH), ImVec2(halfW, -halfH),
        ImVec2(halfW,   halfH), ImVec2(-halfW,  halfH),
    };
    ImVec2 screen[4];
    for (int i = 0; i < 4; i++) {
        float x = local[i].x, y = local[i].y, z = 0.0f;
        float x1 =  x * std::cos(rotY) + z * std::sin(rotY);
        float z1 = -x * std::sin(rotY) + z * std::cos(rotY);
        float y2 =  y * std::cos(rotX) - z1 * std::sin(rotX);
        float z2 =  y * std::sin(rotX) + z1 * std::cos(rotX);
        float persp = focal / (focal + z2);
        screen[i] = ImVec2(center.x + x1 * persp, center.y + y2 * persp);
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Sombra que se despega debajo de la tarjeta al inclinarse.
    ImVec2 shadowCenter = ImVec2(center.x + (*tiltX) * 6.0f, center.y + halfH * 0.65f + (*lift) * 8.0f);
    dl->AddEllipseFilled(shadowCenter, ImVec2(halfW * 0.92f, halfH * 0.16f + (*lift) * 3.0f),
                          IM_COL32(0, 0, 0, (int)(60 + (*lift) * 50)), 0.0f, 24);

    if (texId != 0) {
        dl->AddImageQuad((ImTextureID)(intptr_t)texId,
            screen[0], screen[1], screen[2], screen[3],
            ImVec2(0, 0), ImVec2(1, 0), ImVec2(1, 1), ImVec2(0, 1),
            IM_COL32(255, 255, 255, 255));
    } else {
        dl->AddQuadFilled(screen[0], screen[1], screen[2], screen[3], IM_COL32(40, 40, 46, 255));
        ImGui::SetCursorScreenPos(ImVec2(p0.x + 8, p0.y + size.y * 0.5f - 8));
        ImGui::TextDisabled("(sin textura)");
    }

    // Brillo diagonal que sigue la inclinacion -- solo visible con el
    // mouse encima, reforzando la sensacion de superficie satinada.
    if (*lift > 0.01f) {
        ImVec2 glareCenter = ImVec2(center.x + (*tiltX) * halfW * 0.55f,
                                     center.y + (*tiltY) * halfH * 0.55f);
        dl->AddCircleFilled(glareCenter, halfW * 0.5f,
            ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.12f * (*lift))), 32);
    }

    dl->AddQuad(screen[0], screen[1], screen[2], screen[3],
        ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.12f + (*lift) * 0.18f)), 1.5f);
}

void SettingsPanel::RenderCategoryTheme() {
    auto& theme = ProyecThor::Settings::SettingsManager::Get().GetSettings().theme;

    ImGui::TextDisabled("Elige un tema predeterminado o personaliza los colores.");
    ImGui::Spacing();

    if (SectionTitle("Temas predeterminados", "Temas")) {
        struct PresetEntry { const char* label; ThemePreset preset; };
        static const PresetEntry presets[] = {
            { "Oscuro",           ThemePreset::Dark        },
            { "Claro",            ThemePreset::Light       },
            { "Naranja y Negro",  ThemePreset::OrangeBlack },
            { "Jazz",             ThemePreset::Jazz        },
            { "Ko-fi",            ThemePreset::Kofi        },
            { "Verde",            ThemePreset::Deadlock    },
            { "Galaxia",          ThemePreset::Galaxy      },
            { "Mek",              ThemePreset::Mek         },
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
        ImGui::SameLine(0.0f, 16.0f);
        RenderTiltTextureCard(ImVec2(96.0f, 60.0f));
    }

    // Compartidas por todos los bloques de "Colores"/"Diseño" de abajo --
    // declaradas afuera de cualquier if(SectionTitle) para que `changed`
    // acumule sin importar cual bloque este visible este frame, y el
    // chequeo final (ver mas abajo) sea incondicional.
    bool changed = false;
    static ImGuiColorEditFlags flags =
        ImGuiColorEditFlags_AlphaBar |
        ImGuiColorEditFlags_AlphaPreviewHalf |
        ImGuiColorEditFlags_NoInputs;

    if (SectionTitle("Personalizar colores", "Colores")) {
        ImGui::TextDisabled("Editar cualquier color aquí lo marca como tema \"Personalizado\".");
        ImGui::Spacing();
    }

    if (SectionTitle("Fondos y superficies", "Colores")) {
        changed |= ImGui::ColorEdit4("Fondo principal##base", theme.base, flags);
        HelpTooltip("Color de ventanas principales.");
        changed |= ImGui::ColorEdit4("Superficie 0##s0", theme.surface0, flags);
        changed |= ImGui::ColorEdit4("Superficie 1##s1", theme.surface1, flags);
        changed |= ImGui::ColorEdit4("Superficie 2##s2", theme.surface2, flags);
        changed |= ImGui::ColorEdit4("Superficie 3##s3", theme.surface3, flags);
    }

    if (SectionTitle("Acento", "Colores")) {
        changed |= ImGui::ColorEdit4("Acento##ac",       theme.accent,      flags);
        changed |= ImGui::ColorEdit4("Acento claro##acl",theme.accentLight, flags);
        changed |= ImGui::ColorEdit4("Acento oscuro##acd",theme.accentDim,  flags);
        changed |= ImGui::ColorEdit4("Acento tenue##acf",theme.accentFaint, flags);
    }

    if (SectionTitle("Bordes", "Colores")) {
        changed |= ImGui::ColorEdit4("Borde##bd",        theme.border,      flags);
        changed |= ImGui::ColorEdit4("Borde tenue##bdf",  theme.borderFaint, flags);
    }

    if (SectionTitle("Texto", "Colores")) {
        changed |= ImGui::ColorEdit4("Texto principal##tp",   theme.textPrimary, flags);
        changed |= ImGui::ColorEdit4("Texto secundario##td",  theme.textDim,     flags);
        changed |= ImGui::ColorEdit4("Texto inactivo##tf",    theme.textFaint,   flags);
    }

    if (SectionTitle("Estados", "Colores")) {
        changed |= ImGui::ColorEdit4("Error##dg",   theme.danger,  flags);
        changed |= ImGui::ColorEdit4("Éxito##sc",   theme.success, flags);
    }

    if (SectionTitle("Forma", "Diseño")) {
        changed |= ImGui::SliderFloat("Redondeo de ventanas", &theme.windowRounding, 0.0f, 24.0f, "%.0f");
        changed |= ImGui::SliderFloat("Redondeo de controles", &theme.frameRounding, 0.0f, 16.0f, "%.0f");
        changed |= ImGui::SliderFloat("Grosor de scrollbar",   &theme.scrollbarSize, 4.0f, 16.0f, "%.0f");
    }

    if (changed) {
        theme.preset = ThemePreset::Custom;
        ProyecThor::Settings::SettingsManager::Get().ApplyTheme(); // preview en vivo
    }

    // ── Fuente de la interfaz ────────────────────────────────────────────────
    // Mismo selector que "Edición de estilo" (ver TabTypography::
    // RenderFontSelector): combo con las fuentes que ya están en
    // assets/fonts, + un botón para importar una nueva. Nada de elegir un
    // .ttf suelto del disco cada vez -- se elige de la misma lista/carpeta
    // que usa el resto de la app. El cambio se valida (IsValidFontFile) y
    // se aplica la próxima vez que se abra ProyecThor -- ver main.cpp.
    if (SectionTitle("Fuente de la interfaz", "Fuentes")) {
        ImGui::TextDisabled("Cambia la tipografía de toda la app.");
        ImGui::Spacing();

        std::vector<std::string> fontList;
        ProyecThor::Core::PresentationCore::Get().SyncFontListFromDisk(fontList);

        std::string currentFontName = theme.customFontPath.empty()
            ? "Predeterminada"
            : std::filesystem::path(theme.customFontPath).stem().string();

        const float importBtnW = 90.0f;
        const float comboGap    = 6.0f;
        float comboW = ImGui::GetContentRegionAvail().x - importBtnW - comboGap;

        ImGui::SetNextItemWidth(comboW);
        if (ImGui::BeginCombo("##uiFont", currentFontName.c_str())) {
            for (const auto& name : fontList) {
                bool sel = (name == currentFontName);
                if (ImGui::Selectable(name.c_str(), sel)) {
                    theme.customFontPath = ResolveFontPathByName(name);
                    ProyecThor::Settings::SettingsManager::Get().Save();
                    m_ShowFontRestartPrompt = true;
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine(0.0f, comboGap);
        if (ImGui::Button("+ Fuente", ImVec2(importBtnW, 0.0f))) {
            std::string picked = PickFontFileDialog();
            if (!picked.empty()) {
                try {
                    std::filesystem::path fontsDir = std::filesystem::path(ProyecThor::GetAssetsPath()) / "fonts";
                    std::filesystem::create_directories(fontsDir);

                    std::filesystem::path src(picked);
                    std::filesystem::path dst = fontsDir / src.filename();
                    std::filesystem::copy(src, dst, std::filesystem::copy_options::overwrite_existing);

                    theme.customFontPath = dst.string();
                    ProyecThor::Settings::SettingsManager::Get().Save();
                    m_ShowFontRestartPrompt = true;
                } catch (const std::exception&) {
                    // Import fallido (permisos, disco, etc.): se deja la
                    // selección de fuente tal como estaba.
                }
            }
        }

        if (!theme.customFontPath.empty() && !IsValidFontFile(theme.customFontPath)) {
            ImGui::TextColored(ImVec4(0.93f, 0.35f, 0.35f, 1.0f),
                "\"%s\" no se pudo leer -- se usará la predeterminada.", currentFontName.c_str());
        }
    }

    // La fuente de la interfaz cambia el atlas de ImGui completo (y el de
    // la pantalla de carga) -- eso no se puede "reemplazar en caliente" de
    // forma segura mientras la app esta corriendo con VLC/GL en varias
    // ventanas a la vez, asi que en vez de aplicarla en silencio recien en
    // el proximo arranque, se ofrece reiniciar ya mismo. Fuera del
    // if(SectionTitle) a proposito: el modal tiene que seguir pudiendo
    // dibujarse aunque el usuario cambie de subcategoria mientras esta
    // abierto.
    if (m_ShowFontRestartPrompt) {
        ImGui::OpenPopup("Reiniciar para aplicar la fuente");
        m_ShowFontRestartPrompt = false;
    }
    ImGui::SetNextWindowSize(ImVec2(380.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Reiniciar para aplicar la fuente", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("La nueva fuente de la interfaz se aplica reiniciando ProyecThor. ¿Reiniciar ahora?");
        ImGui::Dummy(ImVec2(0.0f, 12.0f));

        const float btnW = 150.0f;
        if (ImGui::Button("Reiniciar ahora", ImVec2(btnW, 34.0f))) {
            ProyecThor::Settings::SettingsManager::Get().RequestRestart();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0.0f, 10.0f);
        if (ImGui::Button("Más tarde", ImVec2(btnW, 34.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ── Colores de categorías (sidebar de Biblioteca) ───────────────────────
    // Independiente del tema general: solo afecta el color de identidad de
    // cada categoría en el sidebar izquierdo de la Biblioteca (Letra/Video/
    // Imagen/Biblia/Documentos/Audio). Ver LibrarySidebar.cpp.
    if (SectionTitle("Colores de categorías (Biblioteca)", "Colores")) {
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
    }

    // ── Colores de categorías (sidebar de Home) ─────────────────────────────
    // Independiente del tema general: solo afecta el color de identidad de
    // cada sección en el sidebar de Home (Home/Reloj/Anuncios/Notas/Captura/
    // Transmisión). Ver HomeSidebar.cpp.
    if (SectionTitle("Colores de categorías (Home)", "Colores")) {
        ImGui::TextDisabled("Color de identidad de cada sección en el sidebar de Home.");
        ImGui::Spacing();

        auto& homeSidebar = ProyecThor::Settings::SettingsManager::Get().GetSettings().homeSidebar;
        static const char* kHomeCatLabels[6] = {
            "Home", "Contadores", "Anuncios", "Notas Rápidas", "Captura", "Transmisión en Red"
        };
        bool homeSidebarChanged = false;
        for (int i = 0; i < 6; i++) {
            std::string id = std::string(kHomeCatLabels[i]) + "##homecat" + std::to_string(i);
            homeSidebarChanged |= ImGui::ColorEdit4(id.c_str(), homeSidebar.categoryColor[i], flags);
        }
        if (homeSidebarChanged) {
            ProyecThor::Settings::SettingsManager::Get().Save();
        }
    }

    // ── Colores de categorías (hub de Control) ──────────────────────────────
    if (SectionTitle("Colores de categorías (Control)", "Colores")) {
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
    }

    // ── Colores de categorías (hub de Diseño) ───────────────────────────────
    if (SectionTitle("Colores de categorías (Diseño)", "Colores")) {
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
}

} // namespace ProyecThor::UI::Settings