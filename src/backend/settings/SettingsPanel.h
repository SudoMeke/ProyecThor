#pragma once
#include <imgui.h>
#include <string>
#include <vector>
#include <utility>
#include "frontend/panels/StageDisplayPanel.h"

namespace ProyecThor::UI::Settings {

    class SettingsPanel {
    public:
        SettingsPanel();
        void Render(bool* isOpen);
        void InitializeTheme();
        void SetInitialCategory(int idx) { m_SelectedCategory = idx; }
    private:
        int         m_SelectedCategory  = 0;
        int         m_PrevCategory      = -1;   // para detectar cambio de categoría
        float       m_SaveTimer         = 0.0f;
        std::string m_SaveStatusMsg     = "";
        float       m_CheckingAnim      = 0.0f; // ángulo del spinner manual

        bool  m_WasOpenLastFrame = false; // para detectar la transición cerrado -> abierto (sin animar la apertura)

        // ── Subcategorías (navegación por ancla dentro de la misma página) ──
        // Cada llamada a SectionTitle() durante el render de la categoría
        // activa registra aquí su (etiqueta, posición Y local en
        // "##content_scroll"). El sidebar, para la categoría seleccionada,
        // muestra esta lista como subcategorías clickeables -- clickear una
        // no cambia de categoría, solo hace scroll hasta esa sección (sigue
        // siendo la misma página). Se recalcula cada frame en RenderContent(),
        // así que no hace falta declarar nada a mano por categoría.
        std::vector<std::pair<std::string, float>> m_SectionAnchors;
        std::string m_ActiveSubsection;   // cual sección esta a la vista segun el scroll actual
        bool        m_HasPendingScroll = false;
        float       m_PendingScrollY   = 0.0f;

        // La fuente de la interfaz solo se aplica reiniciando (ver
        // CategoryTheme.cpp): al elegir una nueva se dispara este modal de
        // confirmación en vez de aplicarla en caliente.
        bool m_ShowFontRestartPrompt = false;

        void RenderSidebar();
        void RenderContent();
        void RenderSaveBar();

        void RenderCategoryTheme();
        void RenderCategoryGeneral();
        void RenderCategoryProjection();
        void RenderCategoryStage();
        void RenderCategoryAudio();
        void RenderCategoryLanguage();
        void RenderCategoryUpdates();
        void RenderCategoryShortcuts();
void RenderCategorySongs();

        // Antes vivia dentro del hub "Control" (ver ControlPanel, eliminado);
        // ahora es directamente el contenido de la categoria Stage de Ajustes.
        ProyecThor::UI::StageDisplayPanel m_StageDisplay;

        // Helpers
        // navGroup: agrupa varios SectionTitle bajo UNA sola entrada de
        // subcategoría en el sidebar (la primera con ese grupo define la
        // posición del ancla) -- por defecto (nullptr) cada título es su
        // propia subcategoría, como antes. Ver uso agrupado en
        // CategoryTheme.cpp (Temas/Colores/Fuentes/Diseño en vez de una
        // subcategoría por cada bloque de color).
        void SectionTitle(const char* label, const char* navGroup = nullptr);
        void HelpTooltip(const char* desc);
        void AnimatedProgressBar(float fraction, ImVec2 size, ImVec4 col);
        void SpinnerWidget(float radius, float thickness, const ImVec4& color);
    };

} // namespace ProyecThor::UI::Settings