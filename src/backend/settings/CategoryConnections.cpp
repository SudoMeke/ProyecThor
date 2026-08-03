#include "SettingsPanel.h"
#include "SettingsManager.h"
#include "frontend/panels/OSCPanel.h"
#include "frontend/panels/BroadcastPanel.h"
#include "frontend/panels/StreamingPanel.h"
#include "frontend/panels/SyncPanel.h"
#include <imgui.h>

namespace ProyecThor::UI::Settings {

using namespace ProyecThor::Settings;

// Categoria propia (antes vivian como subcategorias fusionadas dentro de
// Proyeccion, y antes de eso como categorias de nivel superior sueltas --
// pedido explicito de volver a subirlas a su propia categoria, esta vez
// con las 4 como subcategorias propias, no fusionadas entre si) -- las 4
// son "como se conecta la app hacia afuera": la sala (LAN), celulares
// (Mobile), transmision en vivo (Streaming RTMP) y luces/controladores
// (OSC).
void SettingsPanel::RenderCategoryConnections() {
    ImGui::TextDisabled("Como se conecta la app hacia afuera: sala, celulares y transmision.");
    ImGui::Spacing();

    if (SectionTitle("Red (LAN)")) {
        ImGui::TextDisabled("Conexion LAN con Stage y otros equipos de la sala.");
        ImGui::Spacing();
        if (m_StreamingPanelRef)
            m_StreamingPanelRef->RenderContent();
        else
            ImGui::TextDisabled("Red no disponible.");
    }

    ImGui::Spacing();

    if (SectionTitle("Mobile")) {
        ImGui::TextDisabled("App movil complementaria: control remoto y sincronizacion.");
        ImGui::Spacing();
        if (m_SyncPanelRef)
            m_SyncPanelRef->RenderContent();
        else
            ImGui::TextDisabled("Mobile no disponible.");
    }

    ImGui::Spacing();

    // Los 3 bloques comparten navGroup="Streaming": una sola entrada en el
    // sidebar en vez de 3 sueltas, mismo criterio que CategoryTheme.cpp usa
    // para Temas/Colores/Fuentes/Diseño.
    if (SectionTitle("Captura", "Streaming")) {
        ImGui::TextDisabled("Transmision RTMP: que se captura, como se compone y cuando arranca.");
        ImGui::Spacing();
        if (m_BroadcastPanelRef)
            m_BroadcastPanelRef->RenderCaptureSection();
        else
            ImGui::TextDisabled("Streaming no disponible.");

        ImGui::Spacing();

        if (m_BroadcastPanelRef) {
            ImGui::SeparatorText("Capa (Layer)");
            m_BroadcastPanelRef->RenderLayerSection();

            ImGui::Spacing();

            ImGui::SeparatorText("Iniciar");
            m_BroadcastPanelRef->RenderStartSection();
        }
    }

    ImGui::Spacing();

    if (SectionTitle("OSC")) {
        ImGui::TextDisabled("Luces y controladores externos via OSC.");
        ImGui::Spacing();
        if (m_OSCPanelRef)
            m_OSCPanelRef->RenderContent();
        else
            ImGui::TextDisabled("OSC no disponible.");
    }
}

} // namespace ProyecThor::UI::Settings
