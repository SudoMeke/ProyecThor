#pragma once

#include <string>
#include "BibleTypes.h"

namespace ProyecThor::UI::Search {

// Interpreta una consulta completa como "Gn 1:1", "salmo 23" o
// "1 corintios 13:4" contra la Biblia cargada.
// outBook/outChap/outVerse son INDICES (no numeros de capitulo/versiculo)
// dentro de bible.books, bible.books[outBook].chapters, etc.
// outChap y outVerse quedan en -1 si el usuario no los especifico (en ese
// caso conviene usar el primero por defecto). Devuelve true si al menos se
// pudo resolver el libro.
bool ParseSmartQuery(const BibleData& bible, const std::string& rawQuery,
                      int& outBook, int& outChap, int& outVerse);

} // namespace ProyecThor::UI::Search