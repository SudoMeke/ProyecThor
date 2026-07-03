#include "WikiPanel.h"
#include <imgui.h>

namespace ProyecThor::UI {

void WikiPanel::Open()
{
    m_Show = true;
}

void WikiPanel::Render()
{
    if (!m_Show) return;

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, { 0.5f, 0.5f });
    ImGui::SetNextWindowSize({ 620.f, 520.f }, ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints({ 420.f, 320.f }, { FLT_MAX, FLT_MAX });

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  { 20.0f, 18.0f });

    bool windowOpen = true;
    ImGui::Begin("Wiki##wikiWin", &windowOpen,
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse);

    ImGui::PopStyleVar(2);

    if (!windowOpen)
    {
        m_Show = false;
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("##wikiTabs"))
    {
        if (ImGui::BeginTabItem("Acerca de"))
        {
            ImGui::Spacing();
            ImGui::TextWrapped(
                "ProyecThor es un programa de proyeccion de letras, avisos y "
                "multimedia pensado para servicios en vivo. Aqui puedes revisar "
                "informacion general y ayuda rapida sobre su funcionamiento.");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Atajos de teclado"))
        {
            ImGui::Spacing();
            ImGui::Columns(2, nullptr, false);

            auto Row = [](const char* action, const char* keys)
            {
                ImGui::TextUnformatted(action);
                ImGui::NextColumn();
                ImGui::TextDisabled("%s", keys);
                ImGui::NextColumn();
            };

            Row("Estrofa siguiente",  "Flecha derecha");
            Row("Estrofa anterior",   "Flecha izquierda");
            Row("Abrir preferencias", "Ctrl + P");
            Row("Ver documentacion",  "F1");
            Row("Cerrar el programa", "Alt + F4");

            ImGui::Columns(1);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Preguntas frecuentes"))
        {
            ImGui::Spacing();
            ImGui::TextWrapped(
                "Como agrego canciones nuevas? Usa Archivo > Base de datos para "
                "importar canciones incluidas con el programa, o crea una nueva "
                "desde la biblioteca.");
            ImGui::Spacing();
            ImGui::TextWrapped(
                "Donde se guardan mis canciones? En la carpeta de datos de la "
                "aplicacion dentro de AppData, para que no se pierdan al "
                "actualizar el programa.");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Soporte"))
        {
            ImGui::Spacing();
            ImGui::TextWrapped(
                "Si encuentras un error o tienes una sugerencia, reportalo desde "
                "Ayuda > Reporte de bugs, o unete a la comunidad desde los "
                "canales de Discord y WhatsApp en el menu de Ayuda.");
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

} // namespace ProyecThor::UI
