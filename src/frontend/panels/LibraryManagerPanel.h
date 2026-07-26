#pragma once
#include "IPanel.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace ProyecThor::UI {

// ── LibraryManagerPanel ──────────────────────────────────────────────────────
// Seccion "Biblioteca" del workspace (ver WorkspaceMode en UIManager.h):
// SOLO para ver/gestionar archivos de Video/Imagen/Audio ya importados --
// a diferencia de la Biblioteca que ya vive dentro de Proyector
// (LibraryPanel.cpp/LibraryVideos.cpp), un click aca NUNCA selecciona nada
// para Vista en Vivo ni dispara proyeccion.
//
// A proposito NO reusa el codigo de LibraryVideos.cpp/LibraryPanel.cpp:
// esta implementada desde cero (lee directo del disco con std::filesystem)
// para no arriesgar ni tener que desacoplar codigo que ya funciona bien en
// Proyector.
class LibraryManagerPanel : public IPanel {
public:
    ~LibraryManagerPanel() override;

    void        Render()  override;
    std::string GetName() const override { return "Biblioteca"; }

private:
    enum class AssetKind { Video, Image, Audio };

    std::string              FolderFor(AssetKind kind) const;
    std::vector<std::string> ExtensionsFor(AssetKind kind) const;
    const char*               LabelFor(AssetKind kind) const;

    void RefreshItems();
    void RenderRail();
    void RenderGrid();
    void RenderCard(int index, float w, float h);
    void RenderRenameModal();
    void RenderDeleteModal();
    void ClearThumbCache();

    AssetKind m_Kind       = AssetKind::Video;
    AssetKind m_LoadedKind = AssetKind::Video;
    bool      m_NeedsRefresh = true;

    std::vector<std::string> m_Items; // solo nombre de archivo, sin ruta
    int                       m_SelectedIndex = -1;

    bool        m_ShowRenameModal = false;
    int         m_RenameIndex     = -1;
    std::string m_RenameExt;
    std::string m_RenameBuffer;

    bool        m_ShowDeleteModal = false;
    int         m_DeleteIndex     = -1;

    std::string m_StatusMessage;
    bool        m_StatusIsError = false;

    // Miniaturas de imagen (la imagen misma, cargada como textura). Video y
    // Audio usan un icono generico -- no hay extraccion de frame de video
    // aca (eso vive en el ThumbnailWorker de LibraryVideos.cpp, que a
    // proposito no se toca).
    std::unordered_map<std::string, unsigned int> m_ImageThumbCache;
};

} // namespace ProyecThor::UI
