#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <imgui.h>

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  LayersOverlayTab — galeria de Overlays guardados. La CREACION/edicion de
//  overlays vive en FoudreVue (app hermana, open source, editor profesional
//  de overlays con Pexels/rotacion/etc — ver /FoudreVue en el repo). Esta
//  pestana en ProyecThor solo consume overlays ya renderizados: los muestra,
//  permite proyectarlos (click = fondo, igual que un Fondo comun) y ofrece
//  importar el paquete portable que FoudreVue exporta (ver ImportBundle) —
//  el mismo patron de interoperabilidad "tipo Adobe" entre apps de una suite.
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

    // "Abrir FoudreVue" (o instalar si no esta) e "Importar overlay..." (lee
    // un paquete .foudrevue exportado — solo el render.png, sin depender de
    // parsear el recipe.json interno de FoudreVue).
    void OpenOrOfferFoudreVue();
    void ImportBundle();

    // ── Modal de descarga cuando FoudreVue no esta instalado ────────────
    // Chequea la ultima release por canal (estable/beta) contra el repo de
    // GitHub y ofrece abrir esa release en el navegador (ver GitHubRelease.h
    // y la decision de no auto-instalar un binario cuyo formato todavia no
    // esta definido, ya que FoudreVue no tiene releases publicados aun).
    enum class FvCheckStatus { Idle, Checking, Found, NoReleases, Error };

    bool                          m_ShowFoudreVueModal = false;
    std::atomic<FvCheckStatus>    m_FvStatus{ FvCheckStatus::Idle };
    std::string                  m_FvVersion;
    std::string                  m_FvHtmlUrl;
    std::string                  m_FvPublishedAt;
    std::thread                  m_FvThread;

    void StartFoudreVueCheck();
    void RenderFoudreVueDownloadModal();
};

} // namespace ProyecThor::UI
