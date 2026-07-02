#pragma once
#include "../styles/CanvaStyleEditor.h"
#include <string>
#include <vector>
#include <memory>
#include <imgui.h>

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  LayersStyleTab — toda la logica del tab "Estilos de Letra"
// ─────────────────────────────────────────────────────────────────────────────
class LayersStyleTab {
public:
    LayersStyleTab();
    ~LayersStyleTab() = default;

    void Render();

    // Acceso externo para sincronizar lista de fuentes tras importar
    void ReloadFonts();

    // Permite al panel principal obtener el estado de estilo activo
    const StyleData& GetCurrentStyle() const { return m_CurrentStyle; }

private:
    // ── Datos ─────────────────────────────────────────────────────────────────
    StyleData                m_CurrentStyle;
    std::string              m_SelectedTheme;
    std::vector<std::string> m_AvailableThemes;
    std::vector<std::string> m_AvailableFonts;

    bool m_GridMode = true;

    std::unique_ptr<CanvaStyleEditor> m_StyleEditor;

    // ── Render helpers ────────────────────────────────────────────────────────
    void RenderThemeGrid();
    void RenderQuickAdjust();
    void RenderStyleEditorModal();
    void RenderViewToggleBar(bool& gridMode);

    void RenderThemeCard(const std::string& name, float cardW, float cardH,
                         int idx, int col, int cols);
    void RenderThemeRow(const std::string& name, float panelW, float rowH, int idx);

    // ── Temas ─────────────────────────────────────────────────────────────────
    bool SaveTheme(const std::string& name, const StyleData& data);
    bool LoadThemeData(const std::string& name, StyleData& outData);
    void ApplyTheme(const std::string& name);
    void DeleteTheme(const std::string& name);
    void ApplyCurrentStyleToCore();

    void LoadThemeList();
    void LoadFontsList();
};

} // namespace ProyecThor::UI
