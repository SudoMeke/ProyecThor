#pragma once
#include <string>
#include <filesystem>
#include <imgui.h>

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
        bool        m_HasRecordedCurrentSongProjection;
        float       m_StanzaCardZoom = 1.0f; // slider: agranda/achica las tarjetas de estrofa

        bool        m_ShowEditor;
        bool        m_OpenEditorPopup;   // Flag diferido: abre el popup en el nivel raiz
        char        m_EditBuffer[16384];
        std::string m_EditingFilePath;
        bool        m_SaveSuccess;

        char        m_AuthorBuffer[256];

        bool        m_FocusStanzaPending;
        int         m_FocusStanzaCharStart;
        int         m_FocusStanzaCharEnd;

        // Popup de color por estrofa: se abre una sola vez en el frame del
        // click (m_OpenColorPickerRequest), no en cada frame mientras esta
        // abierto — evita pisar el estado interno del popup de ImGui.
        int         m_ColorPickerForStanza    = -1;
        bool        m_OpenColorPickerRequest  = false;
        bool        m_OpenSongSettingsRequest = false;

        void RenderEditorModal();
        bool SaveBufferToFile();
        void OpenEditorForSong(const std::string& songTitle, const std::string& stanzaText, bool focusStanza);
        void RenderSettingsCard(const std::string& songFilename, ImVec2 p_min, ImVec2 p_max, bool isHovered);
        void RenderSongSettingsPopup(const std::string& songFilename);
        void RenderStanzaColorBar(const std::string& songFilename, int stanzaIndex, ImVec2 p_min, ImVec2 p_max, float barH);

        static int EditorFocusCallback(ImGuiInputTextCallbackData* data);
    };

} // namespace ProyecThor::UI