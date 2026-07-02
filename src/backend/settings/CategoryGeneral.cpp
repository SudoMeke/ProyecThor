#include "SettingsPanel.h"
#include "SettingsManager.h"
#include <imgui.h>
#include <cstring>

namespace ProyecThor::UI::Settings {

void SettingsPanel::RenderCategoryGeneral() {
    auto& g = ProyecThor::Settings::SettingsManager::Get().GetSettings().general;

    SectionTitle("Inicio");

    ImGui::Checkbox("Iniciar minimizado", &g.startMinimized);
    HelpTooltip("Inicia ProyecThor en la barra de tareas sin mostrar la ventana.");

    ImGui::Checkbox("Recordar layout de paneles", &g.rememberLayout);
    HelpTooltip("Guarda la posicion y tamano de cada panel entre sesiones.");

    ImGui::Checkbox("Confirmar al cerrar", &g.confirmOnExit);
    HelpTooltip("Muestra un dialogo de confirmacion antes de cerrar la aplicacion.");

    SectionTitle("Guardado automatico");

    ImGui::Checkbox("Guardado automatico activo", &g.autoSave);
    HelpTooltip("Guarda los cambios periodicamente sin necesidad de hacerlo manualmente.");

    if (g.autoSave) {
        ImGui::SetNextItemWidth(200.0f);
        ImGui::SliderInt("Intervalo (segundos)##autosave", &g.autoSaveIntervalSec, 30, 600);
        HelpTooltip("Cada cuantos segundos se guardan los cambios automaticamente.");
    }

    SectionTitle("Carpetas por defecto");

    // Buffers locales sincronizados en cada frame con el valor real
    // Usamos buffers de tamaño fijo; no se necesita bandera de init
    static char bibleBuf[512] = {};
    static char mediaBuf[512] = {};

    // Sincronización unidireccional: settings -> buffer (solo si difiere)
    if (std::string(bibleBuf) != g.defaultBiblesFolder)
        strncpy(bibleBuf, g.defaultBiblesFolder.c_str(), sizeof(bibleBuf) - 1);

    if (std::string(mediaBuf) != g.defaultMediaFolder)
        strncpy(mediaBuf, g.defaultMediaFolder.c_str(), sizeof(mediaBuf) - 1);

    ImGui::SetNextItemWidth(-80.0f);
    if (ImGui::InputText("Carpeta de Biblias##bf", bibleBuf, sizeof(bibleBuf)))
        g.defaultBiblesFolder = bibleBuf;
    HelpTooltip("Ruta donde ProyecThor busca archivos XML de Biblia.");

    ImGui::SetNextItemWidth(-80.0f);
    if (ImGui::InputText("Carpeta de Medios##mf", mediaBuf, sizeof(mediaBuf)))
        g.defaultMediaFolder = mediaBuf;
    HelpTooltip("Ruta donde ProyecThor busca videos y fondos de pantalla.");
}

} // namespace ProyecThor::UI::Settings