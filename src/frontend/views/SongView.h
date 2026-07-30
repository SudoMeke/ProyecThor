#pragma once
#include <string>
#include <vector>
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

        // ── Auto-avance por tempo (ver RenderBrowseGrid, barra "Tempo"/
        // Reproducir) — cache en memoria de LibrarySongMeta::tempoBpm/
        // verseDurationOverrideMs, releido cada vez que cambia la cancion
        // activa o se vuelve del editor (donde se edita el override por
        // estrofa, ver icono de reloj en SongEditView).
        int               m_TempoBpm              = 0;
        std::vector<int>  m_VerseDurationOverrideMs;
        bool              m_AutoAdvancePlaying    = false;
        double            m_AutoAdvanceDeadline   = 0.0; // ImGui::GetTime() absoluto

        void ReloadTempoMeta(const std::string& songFilename);
        float ComputeVerseDurationSeconds(const std::string& stanzaText, int stanzaIndex) const;

        void RenderBrowseGrid();
        void RenderSettingsCard(const std::string& songFilename, ImVec2 p_min, ImVec2 p_max, bool isHovered);
        void RenderSongSettingsPopup(const std::string& songFilename);
        void RenderStanzaColorBar(const std::string& songFilename, int stanzaIndex, ImVec2 p_min, ImVec2 p_max, float barH);
    };

} // namespace ProyecThor::UI
