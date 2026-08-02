#pragma once

#include <string>
#include <vector>
#include "BibleTypes.h"

namespace ProyecThor::UI::BibleBooks {

// Nombre canonico del libro segun su numero (1-66).
// Devuelve nullptr si canonicalNumber esta fuera de rango.
const char* GetCanonicalBookName(int canonicalNumber);

// Cantidad total de libros canonicos (66).
int GetBookCount();

// Resuelve una abreviatura EXACTA (ya normalizada: minuscula, sin acentos,
// sin espacios) al numero canonico del libro. Devuelve 0 si no hay
// coincidencia exacta en la tabla de abreviaturas.
int ResolveAbbrev(const std::string& normalizedText);

// Devuelve TODOS los numeros canonicos de libros cuyo nombre completo
// normalizado, o alguna de sus abreviaturas, EMPIEZA con el prefijo dado
// (ya normalizado: minuscula, sin acentos, sin espacios).
// El resultado viene ordenado ascendente por numero canonico y sin
// duplicados. Es la base del buscador rápido tipo "type-ahead": mientras
// el usuario escribe letra por letra, este vector se va reduciendo hasta
// quedar en un solo candidato (o el usuario confirma con Enter el mejor
// candidato actual, que es siempre el de menor numero canonico).
std::vector<int> FindBookCandidates(const std::string& normalizedPrefix);

BibleSection GetBookSection(int canonicalNumber);
void GetSectionColor(BibleSection section, float& r, float& g, float& b);

} // namespace ProyecThor::UI::BibleBooks