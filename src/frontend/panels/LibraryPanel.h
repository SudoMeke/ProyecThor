#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstring>
#include "frontend/views/Audio.h"
#include "frontend/views/DocumentView.h"
#include "IPanel.h"
#include "biblio/LibraryContext.h"

namespace ProyecThor::UI { class UIManager; class MonitorView; }
enum class ActiveLeftPanel;

namespace ProyecThor::UI {

enum class LibraryCategory {
    Songs,
    Videos,
    Images,
    Bibles,
    Documents,
    Audio
};

class LibraryPanel : public IPanel {
public:
    LibraryPanel();
    ~LibraryPanel() override = default;

    std::string GetName() const override { return "Library"; }
    AudioPanel* GetAudioPanel() { return &m_AudioPanel; }
    void Render() override;
    void SetUIManager(UIManager* manager) { m_UIManagerRef = manager; }
    void SetMonitorView(MonitorView* monitor) { m_MonitorRef = monitor; }

private:
    Library::LibraryContext BuildContext();

    void RefreshList();
    void CreateNewSong();
    void SaveSong(const std::string& title, const std::string& content, const std::string& author);
    void ImportFile();
    void DeleteSelectedItem();
    std::vector<std::string> LoadSongVerses(const std::string& filename);
    void LoadStreamURLs();
    void SaveStreamURLs();

    void ShowFileInUseToast(const std::string& fileName);
    void RenderFileInUseToast();

    LibraryCategory          m_CurrentCategory     = LibraryCategory::Songs;
    LibraryCategory          m_PrevCategory        = LibraryCategory::Songs;
    // Flag: evita llamar SetSelection cada frame cuando estamos en Audio.
    // Solo se llama una vez al entrar a la categoria.
    bool                     m_AudioSelectionSet   = false;

    std::vector<std::string> m_Items;
    int                      m_SelectedIndex       = -1;
    char                     m_SearchBuffer[256]{};

    UIManager*               m_UIManagerRef        = nullptr;
    MonitorView*             m_MonitorRef          = nullptr;
    AudioPanel               m_AudioPanel;
    DocumentView              m_DocumentView;
    std::string              m_LoadedDocPath;

    bool m_ShowSongEditor = false;
    char m_EditTitle  [256]{};
    char m_EditContent[8192]{};
    char m_EditAuthor [256]{};

    std::vector<std::string> m_StreamURLs;
    int                      m_SelectedURLIndex    = -1;
    char                     m_URLInputBuffer[512]{};

    bool        m_ShowRenameModal   = false;
    std::string m_RenameOldName;
    std::string m_RenameExtension;
    char        m_RenameBuffer[512]{};
    bool        m_RenameIsURL       = false;
    int         m_RenameURLIndex    = -1;
bool        m_ShowPlaylistsTab   = false;
    std::string m_ActivePlaylistName;
    int         m_ActivePlaylistIndex = -1;
    char        m_EditTags[256]{};

    void SelectPlaylistSong(const std::string& playlistName, int index);
    // ── Toast "archivo en uso" ───────────────────────────────────────────
    // Aviso temporal que aparece cuando DeleteSelectedItem() no logra
    // eliminar un archivo porque sigue bloqueado por otro subsistema
    // (reproduccion de video, documento cargado, etc.). Se dibuja con el
    // ForegroundDrawList directamente en Render(), asi que no depende de
    // estar dentro de ningun BeginChild/BeginWindow especifico y no
    // interfiere con el layout del panel.
    bool        m_ShowFileInUseToast  = false;
    std::string m_FileInUseToastName;
    float       m_FileInUseToastTimer = 0.0f;
};

} // namespace ProyecThor::UI