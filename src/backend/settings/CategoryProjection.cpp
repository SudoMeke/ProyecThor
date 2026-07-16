#include "SettingsPanel.h"
#include "SettingsManager.h"
#include "ProjectionQualityPresets.h"
#include "backend/core/PresentationCore.h"
#include <imgui.h>
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

        // Guardar cambios si hubo alguno
        if (changed) {
            ProyecThor::Settings::SettingsManager::Get().Save();
        }
    }

} // namespace ProyecThor::UI::Settings
