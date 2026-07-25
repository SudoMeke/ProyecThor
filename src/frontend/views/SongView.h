#pragma once
#include <string>
#include <imgui.h>
#include "SongEditView.h"

namespace ProyecThor::UI {

    // SongView es dueña del swap in-place Browse<->Edit (rework del editor):
    // Render() muestra la grilla de estrofas (RenderBrowseGrid) o el editor
    // unificado (m_EditView.Render()) segun m_ShowEditor, nunca los dos, y
    // nunca en una ventana flotante — mismo espiritu que el toggle
    // Canciones/Playlists de LibrarySongs.cpp, aplicado aca porque el punto
    // de entrada real ("Editar") vive en esta clase, no en LibraryPanel.
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

        bool         m_ShowEditor = false;
        SongEditView m_EditView;

        // Popup de color por estrofa: se abre una sola vez en el frame del
        // click (m_OpenColorPickerRequest), no en cada frame mientras esta
        // abierto — evita pisar el estado interno del popup de ImGui.
        int         m_ColorPickerForStanza    = -1;
        bool        m_OpenColorPickerRequest  = false;
        bool        m_OpenSongSettingsRequest = false;

        void RenderBrowseGrid();
        void RenderSettingsCard(const std::string& songFilename, ImVec2 p_min, ImVec2 p_max, bool isHovered);
        void RenderSongSettingsPopup(const std::string& songFilename);
        void RenderStanzaColorBar(const std::string& songFilename, int stanzaIndex, ImVec2 p_min, ImVec2 p_max, float barH);
    };

} // namespace ProyecThor::UI
