#pragma once

#include <string>
#include <imgui.h>

namespace ProyecThor::UI::TextUtils {

// Convierte a minusculas asumiendo bytes UTF-8 (solo el rango ASCII se
// transforma; los bytes multibyte de acentos se dejan intactos).
std::string ToLowerUTF8(const std::string& s);

// Reemplaza vocales acentuadas y "n" con virgulilla (en UTF-8, codificacion
// de 2 bytes iniciando en 0xC3) por su version sin acento en ASCII.
std::string StripAccents(const std::string& s);

// Normaliza un texto para comparaciones de busqueda: minuscula, sin acentos
// y sin espacios. Ej: "1 Corintios" -> "1corintios".
std::string Normalize(const std::string& s);

// Empaqueta un color RGBA (0.0-1.0) en un ImU32, tal como lo espera ImGui.
ImU32 Col(float r, float g, float b, float a = 1.0f);

} // namespace ProyecThor::UI::TextUtils