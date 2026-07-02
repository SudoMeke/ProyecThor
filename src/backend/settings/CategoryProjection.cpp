#include "SettingsPanel.h"
#include "SettingsManager.h"
#include "backend/core/PresentationCore.h"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <vector>
#include <string>

namespace ProyecThor::UI::Settings {

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
