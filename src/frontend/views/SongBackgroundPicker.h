#pragma once
#include <string>
#include <vector>

namespace ProyecThor::UI {

// Extraido de SongView.cpp (donde vivia como funcion/struct static) para que
// el editor unificado de canciones (SongEditView, ver rework del editor)
// tambien pueda ofrecer el mismo picker de fondos por LINEA, no solo para el
// preset de la cancion completa (RenderSongSettingsPopup). Sin cambios de
// comportamiento respecto de la version original.
struct SongBgEntry {
    std::string fullPath;
    std::string label;
    bool        isImage = false;
};

// Lista los fondos de assets/backgrounds (misma carpeta que usa el tab
// "Fondos", ver LayersBgTab) — version liviana, sin miniaturas ni cache de
// texturas, solo para poblar un combo de seleccion.
std::vector<SongBgEntry> ListSongBackgrounds();

} // namespace ProyecThor::UI
