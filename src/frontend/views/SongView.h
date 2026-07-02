#pragma once
#include <string>
#include <filesystem>

struct ImGuiInputTextCallbackData;

namespace ProyecThor::UI {

    class SongView {
    public:
        SongView();
        ~SongView() = default;

        void Render();

    private:
        std::string m_CurrentSongTitle;
        int         m_ActiveStanzaIndex;

        bool        m_ShowEditor;
        bool        m_OpenEditorPopup;   // Flag diferido: abre el popup en el nivel raiz
        char        m_EditBuffer[16384];
        std::string m_EditingFilePath;
        bool        m_SaveSuccess;

        char        m_AuthorBuffer[256];

        bool        m_FocusStanzaPending;
        int         m_FocusStanzaCharStart;
        int         m_FocusStanzaCharEnd;

        void RenderEditorModal();
        bool SaveBufferToFile();
        void OpenEditorForSong(const std::string& songTitle, const std::string& stanzaText, bool focusStanza);

        static int EditorFocusCallback(ImGuiInputTextCallbackData* data);
    };

} // namespace ProyecThor::UI