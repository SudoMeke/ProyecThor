#pragma once
#include <string>

namespace ProyecThor::UI {

// Selector nativo de archivo (imagen o video): IFileOpenDialog en Windows,
// zenity/kdialog en Linux (fallback en cadena, igual que Audio.cpp y
// LayersBgTab.cpp). Devuelve "" si el usuario cancela o no hay ninguna
// herramienta disponible.
std::string PickImageOrVideoFile();

// Mismo patron, pero solo imagenes (jpg/jpeg/png) — usado para el Logo de
// pantalla de carga (Ajustes > Proyeccion), donde un video no tendria
// sentido.
std::string PickImageFile();

// Extension-sniffing simple para decidir si un path va por el pipeline de
// video o de imagen (mismo criterio que BackgroundLayer).
bool LooksLikeVideoPath(const std::string& path);

} // namespace ProyecThor::UI
