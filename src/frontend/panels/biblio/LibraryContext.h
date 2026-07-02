#pragma once

#include <string>
#include <vector>
#include <functional>

namespace ProyecThor::UI { class MonitorView; }

namespace ProyecThor::Library {

struct LibraryContext
{
    // ── Categoria y lista ─────────────────────────────────────────────────
    int&                      currentCategoryInt;
    std::vector<std::string>& items;
    int&                      selectedIndex;
    char*                     searchBuffer;
    int                       searchBufferSize;

    // ── Stream URLs ───────────────────────────────────────────────────────
    std::vector<std::string>& streamURLs;
    int&                      selectedURLIndex;
    char*                     urlInputBuffer;
    int                       urlInputBufferSize;

    // ── Editor de canciones ───────────────────────────────────────────────
    bool&  showSongEditor;
    char*  editTitle;
    char*  editContent;
    char*  editAuthor;

    // ── Modal de renombrar ────────────────────────────────────────────────
    bool&        showRenameModal;
    std::string& renameOldName;
    std::string& renameExtension;
    char*        renameBuffer;
    bool&        renameIsURL;
    int&         renameURLIndex;

    // ── Documento cargado ─────────────────────────────────────────────────
    std::string& loadedDocPath;

    // ── Referencia al monitor (para cola de videos) ───────────────────────
    UI::MonitorView* monitorRef;

    // ── Callbacks hacia LibraryPanel ──────────────────────────────────────
    std::function<void()>                                       refreshList;
    std::function<void()>                                       deleteSelectedItem;
    std::function<void()>                                       importFile;
    std::function<void()>                                       createNewSong;
    std::function<void(const std::string&, const std::string&, const std::string&)> saveSong;
    std::function<void()>                                       loadStreamURLs;
    std::function<void()>                                       saveStreamURLs;
    std::function<std::vector<std::string>(const std::string&)> loadSongVerses;

    // Aplica un estilo guardado por nombre al canvas activo.
    // La lambda la registra LibraryPanel, que tiene acceso a PresentationCore
    // y sabe como cargar y aplicar un StyleData por nombre.
    // Si no hay estilo configurado o el nombre no existe, la lambda no hace nada.
    std::function<void(const std::string&)>                     applyStyle;
};

} // namespace ProyecThor::Library