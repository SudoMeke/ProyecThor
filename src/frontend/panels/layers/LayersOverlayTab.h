#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <imgui.h>
#include "backend/core/MacroTypes.h"

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  LayersOverlayTab — dos modos:
//   - Galeria: overlays estaticos (.png), propios o leidos automaticamente
//     de la carpeta de datos de FoudreVue si esta instalado (sigue siendo
//     posible generarlos ahi, pero ya no hay boton para abrir/instalar
//     FoudreVue ni para importar un paquete a mano — solo auto-deteccion).
//   - Macros: secuencias de cues con tiempo (fondo/overlay/estilo de
//     reloj/texto) que se ejecutan en orden al reproducir, tipo "playlist
//     de comandos" (ver backend/core/MacroTypes.h). La reproduccion en si
//     vive en PresentationCore (PlayMacro/NextMacroCue/...), no aca, para
//     que el transporte "Control Overlays" de ViewPanel controle el mismo
//     macro que se dispara desde este editor.
// ─────────────────────────────────────────────────────────────────────────────
class LayersOverlayTab {
public:
    LayersOverlayTab() = default;
    ~LayersOverlayTab() = default;

    void Render();
    void ReloadList();

private:
    struct OverlayEntry {
        std::string name;
        std::string pngPath;
        // true = leido directo de la carpeta de overlays de FoudreVue (no
        // copiado); no se ofrece Eliminar/Renombrar sobre estas entradas,
        // solo "Copiar a mis overlays" (ver CopyExternalToMine).
        bool external = false;
    };

    std::vector<OverlayEntry> m_Overlays;
    bool m_Loaded = false;

    std::unordered_map<std::string, ImTextureID> m_ThumbnailCache;
    ImTextureID GetThumbnail(const std::string& path);

    bool  m_GridMode  = true;
    float m_ThumbZoom = 1.0f;

    std::string m_StatusMsg;
    float       m_StatusTimer = 0.0f;
    void SetStatus(const std::string& msg);

    void RenderTopBar();
    void RenderGallery();
    void RenderCard(const OverlayEntry& e, float cardW, float cardH, int col, int cols);
    void RenderRow(const OverlayEntry& e, float panelW, float rowH);

    bool DeleteOverlay(const std::string& name);
    bool RenameOverlay(const std::string& oldName, const std::string& newName);
    std::string ResolvePngPath(const std::string& name);
    bool CopyExternalToMine(const OverlayEntry& e);

    // ── Modo: Galeria / Macros ───────────────────────────────────────────
    enum class TabMode { Gallery, Macros };
    TabMode m_Mode = TabMode::Gallery;

    // ── Macros ────────────────────────────────────────────────────────────
    // La reproduccion (Play/Stop/Next/Prev) vive en PresentationCore, no
    // aca — este editor solo arma/persiste el Macro y le pide a
    // PresentationCore que lo reproduzca.
    std::vector<std::string> m_MacroNames;
    bool                     m_MacrosLoaded = false;
    Core::Macro              m_EditingMacro;
    bool                     m_HasEditingMacro = false;
    int                      m_EditingCueIndex = -1; // -1 = ninguno (formulario "agregar")

    void ReloadMacroList();
    void RenderMacrosBrowser();
    void RenderMacroEditor();
    void RenderCueForm();
    void NewMacro();
    void OpenMacro(const std::string& name);
    void SaveEditingMacro();

    // Macros como tarjetas reproducibles DENTRO de la Galeria (ademas de la
    // lista de RenderMacrosBrowser en la pestaña Macros, que ahora es solo
    // para editar) — mismo esqueleto visual que RenderCard/RenderRow.
    void RenderMacroCard(const std::string& macroName, float cardW, float cardH, int col, int cols);
    void RenderMacroRow(const std::string& macroName, float panelW, float rowH);

    // Path de imagen usado como thumbnail de una tarjeta de macro (primer
    // cue ChangeBackground/SetOverlay con imagen), "" si no hay ninguno.
    // Cacheado por nombre para no hacer LoadMacro() (lee+parsea JSON) en
    // cada frame por cada tarjeta visible — se invalida en ReloadMacroList().
    std::unordered_map<std::string, std::string> m_MacroThumbPathCache;
    std::string ResolveMacroThumbPath(const std::string& macroName);
};

} // namespace ProyecThor::UI
