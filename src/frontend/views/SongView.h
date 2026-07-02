#pragma once
#include <string>

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

        void RenderEditorModal();
        bool SaveBufferToFile();
    };

} // namespace ProyecThor::UI
