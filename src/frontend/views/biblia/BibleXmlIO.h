#pragma once

#include <string>
#include "BibleTypes.h"

namespace ProyecThor::UI::XmlIO {

// Carga una Biblia completa desde un archivo XML en outBible (se sobrescribe
// por completo). Devuelve true si se pudo abrir el archivo y se cargo al
// menos un libro.
bool LoadBible(const std::string& path, BibleData& outBible);

// Reescribe por completo el archivo XML en 'path' con el contenido actual
// de 'bible'. Devuelve true si se pudo abrir y escribir el archivo.
bool SaveBible(const std::string& path, const BibleData& bible);

} // namespace ProyecThor::UI::XmlIO