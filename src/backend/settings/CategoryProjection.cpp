#include "SettingsPanel.h"
#include "SettingsManager.h"
#include "ProjectionQualityPresets.h"
#include "backend/core/PresentationCore.h"
#include "backend/core/AppPaths.h"
#include "frontend/ui/FilePicker.h"
#include <imgui.h>
#include <filesystem>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <vector>
#include <string>

namespace ProyecThor::UI::Settings {

using namespace ProyecThor::Settings;

// Boton simple de modo/preset (texto only, sin color de acento) usado en la
// seccion "Calidad de Salida". Mas liviano que el PresetSwatch de temas, que
// esta pensado para mostrar un color; aca solo elegimos entre etiquetas.
static bool QualityModeButton(const char* id, const char* label, bool active, float width) {
    ImVec4 base   = active ? ImVec4(0.25f, 0.45f, 0.85f, 0.35f) : ImVec4(1,1,1,0.05f);
    ImVec4 hover  = active ? ImVec4(0.25f, 0.45f, 0.85f, 0.45f) : ImVec4(1,1,1,0.10f);
    ImVec4 border = active ? ImVec4(0.35f, 0.55f, 0.95f, 1.0f)  : ImVec4(1,1,1,0.12f);

    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_Button, base);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, hover);
    ImGui::PushStyleColor(ImGuiCol_Border, border);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, active ? 2.0f : 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

    bool clicked = ImGui::Button(label, ImVec2(width, 36.0f));

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    ImGui::PopID();
    return clicked;
}

    void SettingsPanel::RenderCategoryProjection() {
        auto& p   = ProyecThor::Settings::SettingsManager::Get().GetSettings().projection;
        bool  changed = false;

        ImGui::TextDisabled("Ajustes que afectan directamente la ventana proyectada.");
        ImGui::Spacing();

        // ── Monitor de Salida ─────────────────────────────────────────────────
        SectionTitle("Monitor de Salida");
        {
            int monitorCount = 0;
            GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

            if (monitors && monitorCount > 0) {
                std::vector<std::string> itemLabels;
                std::vector<const char*> names;

                for (int i = 0; i < monitorCount; i++) {
                    std::string label = "[" + std::to_string(i) + "] "
                                      + glfwGetMonitorName(monitors[i]);
                    if (i == 0) label += " (Principal)";
                    itemLabels.push_back(label);
                }
                for (const auto& label : itemLabels) {
                    names.push_back(label.c_str());
                }

                int sel = std::clamp(p.targetMonitor, 0, monitorCount - 1);

                ImGui::SetNextItemWidth(350.0f);
                if (ImGui::Combo("Monitor de Proyeccion##mon", &sel,
                                 names.data(), static_cast<int>(names.size()))) {
                    p.targetMonitor = sel;
                    changed = true;
                }
                HelpTooltip("Elige en que pantalla se mostrara la proyeccion.\n"
                            "Se recomienda usar la pantalla secundaria (indice 1 o superior).");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                                   "No se detectaron monitores adicionales.");
            }
        }

        ImGui::Spacing();

        // ── Calidad de Salida (video de fondo) ────────────────────────────────
        SectionTitle("Calidad de Salida (Video de Fondo)");
        {
            int monitorCount = 0;
            GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
            int monW = 1920, monH = 1080;
            if (monitors && monitorCount > 0) {
                int idx = std::clamp(p.targetMonitor, 0, monitorCount - 1);
                if (const GLFWvidmode* vm = glfwGetVideoMode(monitors[idx])) {
                    monW = vm->width; monH = vm->height;
                }
            }

            auto mode = static_cast<OutputQualityMode>(p.outputQualityMode);

            float w = ImGui::GetContentRegionAvail().x;
            float btnW = (w - 12.0f) / 3.0f;

            if (QualityModeButton("qmAuto", "Auto", mode == OutputQualityMode::Auto, btnW)) {
                p.outputQualityMode = (int)OutputQualityMode::Auto; changed = true;
            }
            ImGui::SameLine(0.0f, 6.0f);
            if (QualityModeButton("qmPreset", "Preset", mode == OutputQualityMode::Preset, btnW)) {
                p.outputQualityMode = (int)OutputQualityMode::Preset; changed = true;
            }
            ImGui::SameLine(0.0f, 6.0f);
            if (QualityModeButton("qmCustom", "Personalizado", mode == OutputQualityMode::Custom, btnW)) {
                p.outputQualityMode = (int)OutputQualityMode::Custom; changed = true;
            }
            HelpTooltip("Controla a que resolucion se procesa/reescala (FSR) el video de fondo "
                        "antes de mostrarlo. El texto en vivo siempre se ve nitido a resolucion "
                        "nativa, sin importar esta opcion.\n\n"
                        "Auto: la app elige un objetivo razonable segun el monitor.\n"
                        "Preset: estandares fijos de resolucion/fps.\n"
                        "Personalizado: tu eliges ancho, alto y fps.");

            ImGui::Spacing();
            mode = static_cast<OutputQualityMode>(p.outputQualityMode);

            if (mode == OutputQualityMode::Preset) {
                float pw = (w - 6.0f * (kQualityPresetCount - 1)) / kQualityPresetCount;
                for (int i = 0; i < kQualityPresetCount; i++) {
                    if (i > 0) ImGui::SameLine(0.0f, 6.0f);
                    if (QualityModeButton((std::string("qp") + std::to_string(i)).c_str(),
                                          kQualityPresets[i].label, p.outputPresetIndex == i, pw)) {
                        p.outputPresetIndex = i; changed = true;
                    }
                }
            } else if (mode == OutputQualityMode::Custom) {
                float half = (w - 16.0f) * 0.5f;
                ImGui::TextColored(ImVec4(0.7f,0.7f,0.75f,1.0f), "Ancho");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(half - 60.0f);
                int customW = p.outputWidth;
                if (ImGui::InputInt("##qw", &customW, 0, 0)) {
                    p.outputWidth = std::clamp(customW, 320, monW);
                    changed = true;
                }
                ImGui::SameLine(0.0f, 16.0f);
                ImGui::TextColored(ImVec4(0.7f,0.7f,0.75f,1.0f), "Alto");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(half - 60.0f);
                int customH = p.outputHeight;
                if (ImGui::InputInt("##qh", &customH, 0, 0)) {
                    p.outputHeight = std::clamp(customH, 180, monH);
                    changed = true;
                }

                ImGui::SetNextItemWidth(200.0f);
                int fps = p.targetFPS;
                if (ImGui::SliderInt("FPS objetivo", &fps, 15, 60)) {
                    p.targetFPS = fps;
                    changed = true;
                }
            }

            int qW = 0, qH = 0;
            ResolveQualityTarget(mode, p.outputPresetIndex, p.outputWidth, p.outputHeight,
                                 monW, monH, qW, qH);
            ImGui::Spacing();
            ImGui::TextDisabled("Objetivo actual: %dx%d (monitor: %dx%d)", qW, qH, monW, monH);

            if (mode != OutputQualityMode::Auto || qW < monW || qH < monH) {
                bool fsrOn = Core::PresentationCore::Get().GetFSREnabled();
                if (!fsrOn && (qW < monW || qH < monH)) {
                    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
                        "FSR esta desactivado: el video de fondo no se reescalara con nitidez.");
                }
            }
        }

        ImGui::Spacing();

        // ── Motor de renderizado (Videos) ─────────────────────────────────────
        SectionTitle("Motor de Renderizado (Videos)");
        {
            int engine = Core::PresentationCore::Get().GetVideoRenderEngine();
            float w2    = ImGui::GetContentRegionAvail().x;
            float btnW2 = (w2 - 6.0f) * 0.5f;

            if (QualityModeButton("engOpenGL", "OpenGL", engine == 0, btnW2)) {
                p.videoRenderEngine = 0;
                Core::PresentationCore::Get().SetVideoRenderEngine(0);
                changed = true;
            }
            ImGui::SameLine(0.0f, 6.0f);
            if (QualityModeButton("engLibvlc", "libvlc", engine == 1, btnW2)) {
                p.videoRenderEngine = 1;
                Core::PresentationCore::Get().SetVideoRenderEngine(1);
                changed = true;
            }
            HelpTooltip("Solo afecta a VIDEOS reales (Biblioteca > Videos / cola del Monitor "
                        "con audio) -- los Fondos (loops decorativos, imagenes, color solido) "
                        "siempre se muestran por OpenGL, con overlays y texto en vivo encima, "
                        "sin importar esta opcion.\n\n"
                        "OpenGL (default): el video se compone junto con overlays/texto/"
                        "anuncios en la misma salida.\n"
                        "libvlc: el video se muestra en una ventana nativa aparte, con el "
                        "renderer acelerado propio de VLC. Cambiar este ajuste requiere "
                        "reiniciar Audiencia para que tenga efecto.");

            if (engine == 1) {
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
                    "Con libvlc: mientras un video este activo, sin overlays/texto encima "
                    "y sin transicion animada entre clips (corte seco).");
            }
        }

        ImGui::Spacing();

        // ── FSR Upscaling ─────────────────────────────────────────────────────
        // FIX: Sección FSR colocada correctamente fuera del bloque del Combo,
        //      con todas las llaves balanceadas.
        ImGui::SeparatorText("FSR Upscaling");

        bool fsrEnabled = Core::PresentationCore::Get().GetFSREnabled();
        if (ImGui::Checkbox("Activar FSR 1.0", &fsrEnabled)) {
            Core::PresentationCore::Get().SetFSREnabled(fsrEnabled);
            changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Mejora calidad de video de baja resolucion)");

        if (fsrEnabled) {
            float sharpness = Core::PresentationCore::Get().GetFSRSharpness();
            ImGui::SetNextItemWidth(200.0f);
            if (ImGui::SliderFloat("Nitidez FSR", &sharpness, 0.0f, 2.0f, "%.2f")) {
                Core::PresentationCore::Get().SetFSRSharpness(sharpness);
                changed = true;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("0=Max  2=Suave");
        }

        ImGui::Spacing();

        // ── Logo (pantalla de carga) ─────────────────────────────────────────
        SectionTitle("Logo");
        {
            std::string display = p.loadingLogoPath.empty()
                ? "(sin logo)"
                : std::filesystem::path(p.loadingLogoPath).filename().string();
            ImGui::TextDisabled("%s", display.c_str());
            HelpTooltip("Imagen que se muestra a la salida real (al publico) mientras un "
                        "fondo o video esta cargando, en vez de dejar ver un frame "
                        "entrecortado o desactualizado. Si no se elige ninguna, la pantalla "
                        "simplemente mantiene el ultimo fondo listo hasta que el nuevo "
                        "termine de cargar (comportamiento de siempre).");

            if (ImGui::Button("Elegir imagen...")) {
                std::string picked = ProyecThor::UI::PickImageFile();
                if (!picked.empty()) {
                    // Se copia a la carpeta de datos de la app (igual que ya
                    // hace Fondos, ver LayersBgTab::ImportBackground) en vez
                    // de guardar la ruta externa tal cual: asi el logo queda
                    // junto con el resto de los assets de ProyecThor y no se
                    // rompe si el archivo original se mueve, se borra, o el
                    // perfil se usa en otra maquina.
                    std::filesystem::path src(picked);
                    std::filesystem::path destDir = ProyecThor::BrandingPath();
                    std::error_code ec;
                    std::filesystem::create_directories(destDir, ec);
                    std::filesystem::path dest = std::filesystem::path(destDir) / src.filename();
                    std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing, ec);
                    if (!ec) {
                        p.loadingLogoPath = dest.string();
                        changed = true;
                    }
                }
            }
            if (!p.loadingLogoPath.empty()) {
                ImGui::SameLine();
                if (ImGui::Button("Quitar##logo")) {
                    p.loadingLogoPath.clear();
                    changed = true;
                }
            }
        }

        ImGui::Spacing();

        // Guardar cambios si hubo alguno
        if (changed) {
            ProyecThor::Settings::SettingsManager::Get().Save();
        }
    }

} // namespace ProyecThor::UI::Settings
