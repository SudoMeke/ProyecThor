#pragma once
#include <imgui.h>
#include <string>

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

        // ── Estado de animación ─────────────────────────────────────────────
        bool  m_WasOpenLastFrame = false; // para detectar la transición cerrado -> abierto
        float m_OpenAnim         = 1.0f;  // 0..1 progreso del "pop-in" al abrir la ventana
        float m_ContentFade      = 1.0f;  // 0..1 fade/slide del contenido al cambiar de categoría
        bool  m_PillInit         = false;
        float m_PillY            = 0.0f;  // posición Y animada del indicador de selección del sidebar

        void RenderSidebar();
        void RenderContent();
        void RenderSaveBar();

        void RenderCategoryTheme();
        void RenderCategoryGeneral();
        void RenderCategoryProjection();
        void RenderCategoryAudio();
        void RenderCategoryLanguage();
        void RenderCategoryUpdates();
        void RenderCategoryShortcuts();
void RenderCategorySongs();
        // Helpers
        void SectionTitle(const char* label);
        void HelpTooltip(const char* desc);
        void AnimatedProgressBar(float fraction, ImVec2 size, ImVec4 col);
        void SpinnerWidget(float radius, float thickness, const ImVec4& color);
    };

} // namespace ProyecThor::UI::Settings