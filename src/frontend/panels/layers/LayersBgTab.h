#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <imgui.h>
#include "../BackgroundsPanel.h"

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  LayersBgTab — toda la logica del tab "Fondos"
// ─────────────────────────────────────────────────────────────────────────────
class LayersBgTab {
public:
    LayersBgTab();
    ~LayersBgTab() = default;

    // Punto de entrada: renderiza el contenido del tab (sin la barra de tabs)
    void Render();

    // Llamar externamente si se necesita forzar recarga
    void ReloadList();

private:
    // ── Datos ─────────────────────────────────────────────────────────────────
    std::vector<BgEntry>     m_AllBackgrounds;
    std::vector<std::string> m_BgFolders;
    std::string              m_CurrentBgFolder;

    // Previene autoclick al entrar en carpeta: se activa al cambiar de carpeta
    // y se limpia tras el primer frame renderizado en la nueva vista
    bool m_JustEnteredFolder = false;

    // ── Thumbnails ────────────────────────────────────────────────────────────
    std::unordered_map<std::string, ImTextureID> m_ThumbnailCache;
    ImTextureID GetThumbnail(const std::string& path, bool isVideo);

    // ── Vistas ────────────────────────────────────────────────────────────────
    bool m_GridMode = true;

    // ── Estado de renombrado ──────────────────────────────────────────────────
    bool        m_RenamingBg      = false;
    std::string m_RenameOldPath;
    char        m_RenameBuf[256]  = {};

    bool        m_RenamingFolder  = false;
    std::string m_RenameFolderOld;
    char        m_RenameFolderBuf[128] = {};

    // ── Estado de creacion de carpeta ─────────────────────────────────────────
    bool m_CreatingFolder = false;
    char m_NewFolderBuf[128] = {};

    // ── Render helpers ────────────────────────────────────────────────────────
    void RenderToolbar();
    void RenderBreadcrumb();
    void RenderFolderView();        // vista raiz
    void RenderFilesInFolder();     // vista dentro de una carpeta

    void RenderFolderCard(const std::string& name, float cardW, float cardH, int col, int cols);
    void RenderFolderRow(const std::string& name, float panelW, float rowH);
    void RenderBgCard(const BgEntry& e, float cardW, float cardH, int col, int cols);
    void RenderBgRow(const BgEntry& e, float panelW, float rowH);

    void RenderViewToggleBar(bool& gridMode);
    void BgContextMenu(const BgEntry& entry);
    void FolderContextMenu(const std::string& folderName);

    // Modales
    void RenderCreateFolderModal();
    void RenderRenameBgModal();
    void RenderRenameFolderModal();

    // ── Operaciones de disco ─────────────────────────────────────────────────
    bool ImportBackground();
    bool CreateBgFolder(const std::string& name);
    bool RenameBgFile(const std::string& oldPath, const std::string& newName);
    bool RenameBgFolder(const std::string& oldName, const std::string& newName);
    bool DeleteBgFile(const std::string& fullPath);
    bool DeleteBgFolder(const std::string& folderName);
    bool MoveBgToFolder(const std::string& srcFullPath, const std::string& destFolder);
};

} // namespace ProyecThor::UI
