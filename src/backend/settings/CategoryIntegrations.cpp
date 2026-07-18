#include "SettingsPanel.h"
#include "SettingsManager.h"
#include "../../external/tools/OpenURL.h"
#include <imgui.h>
#include <cstring>

namespace ProyecThor::UI::Settings {

void SettingsPanel::RenderCategoryIntegrations() {
    auto& integ = ProyecThor::Settings::SettingsManager::Get().GetSettings().integrations;

    SectionTitle("Pexels (banco de fotos)");

    ImGui::TextWrapped(
        "Permite buscar fotos libres de derechos directamente desde el editor "
        "de Overlays. Cada usuario necesita su propia API key gratuita: "
        "ProyecThor es open source y nunca incluye una key propia en el "
        "codigo (quedaria expuesta publicamente).");
    ImGui::Spacing();

    if (ImGui::Button("Conseguir una key gratis en pexels.com/api"))
        ProyecThor::External::OpenURL("https://www.pexels.com/api/");
    HelpTooltip("Abre la pagina de registro de la API de Pexels en el navegador.");

    ImGui::Spacing();

    static char keyBuf[256] = {};
    static bool showKey     = false;

    if (std::string(keyBuf) != integ.pexelsApiKey)
        strncpy(keyBuf, integ.pexelsApiKey.c_str(), sizeof(keyBuf) - 1);

    ImGuiInputTextFlags flags = showKey ? 0 : ImGuiInputTextFlags_Password;
    ImGui::SetNextItemWidth(-80.0f);
    if (ImGui::InputText("API key##pexelsKey", keyBuf, sizeof(keyBuf), flags))
        integ.pexelsApiKey = keyBuf;
    HelpTooltip("Se guarda solo en tu configuracion local (settings.json), nunca en el repositorio.");

    ImGui::SameLine();
    ImGui::Checkbox("Mostrar##pexelsShowKey", &showKey);

    ImGui::Spacing();
    if (integ.pexelsApiKey.empty()) {
        ImGui::TextDisabled("Sin key configurada: la busqueda de Pexels aparecera deshabilitada en Overlays.");
    } else {
        ImGui::TextColored(ImVec4(0.40f, 0.85f, 0.55f, 1.0f), "Key configurada.");
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.53f, 1.0f));
    ImGui::TextWrapped(
        "Pexels limita el uso gratuito (por hora y por mes) a nivel de cada "
        "key. ProyecThor respeta ese limite: cachea busquedas repetidas, no "
        "dispara una peticion por cada tecla presionada, y muestra la cuota "
        "restante informada por Pexels en el propio buscador, bloqueando "
        "nuevas busquedas si se agota hasta que la cuota se renueve.");
    ImGui::PopStyleColor();
}

} // namespace ProyecThor::UI::Settings
