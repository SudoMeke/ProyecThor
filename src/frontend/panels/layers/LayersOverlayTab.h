#pragma once
#include "overlay/OverlayTypes.h"
#include "overlay/OverlayCanvasEditor.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <imgui.h>

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  LayersOverlayTab — galeria de Overlays guardados (imagenes generadas con
//  OverlayCanvasEditor). Mismo lenguaje visual que Fondos/Estilos: toolbar
//  compacta arriba (solo iconos) con nuevo overlay / zoom / grid-lista.
//  Un click sobre una tarjeta proyecta el overlay como fondo (igual que un
//  Fondo comun — ver PresentationCore::SetBackgroundMedia).
// ─────────────────────────────────────────────────────────────────────────────
class LayersOverlayTab {
public:
    LayersOverlayTab();
    ~LayersOverlayTab() = default;

    void Render();
    void ReloadList();

private:
    struct OverlayEntry {
        std::string name;
        std::string pngPath;
    };

    std::vector<OverlayEntry> m_Overlays;
    std::vector<std::string>  m_AvailableFonts;

    std::unordered_map<std::string, ImTextureID> m_ThumbnailCache;
    ImTextureID GetThumbnail(const std::string& path);

    bool  m_GridMode  = true;
    float m_ThumbZoom = 1.0f;

    std::unique_ptr<OverlayCanvasEditor> m_Editor;

    void RenderTopBar();
    void RenderGallery();
    void RenderCard(const OverlayEntry& e, float cardW, float cardH, int col, int cols);
    void RenderRow(const OverlayEntry& e, float panelW, float rowH);
    void RenderEditorModal();

    void LoadFontsList();

    bool SaveOverlayRecipe(const std::string& name, const OverlayDoc& doc);
    bool LoadOverlayRecipe(const std::string& name, OverlayDoc& outDoc);
    bool DeleteOverlay(const std::string& name);
    bool RenameOverlay(const std::string& oldName, const std::string& newName);
    std::string ResolvePngPath(const std::string& name);

    // Imagenes para capas: reusa las ya importadas en el tab "Fondos", o
    // importa un archivo nuevo del disco a la carpeta propia de assets del
    // overlay (ver OverlayCanvasEditor::ListBgImagesFn/ImportImageFn).
    std::vector<std::string> ListBgImages();
    std::string ImportOverlayImage();
};

} // namespace ProyecThor::UI
