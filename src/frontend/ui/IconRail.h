#pragma once
#include <imgui.h>

namespace ProyecThor::UI {

using DrawIconFn = void(*)(ImDrawList*, ImVec2, float, ImU32);

struct IconRailItem {
    int         index;     // valor logico a asignar a currentIndex al hacer click
    DrawIconFn  drawIcon;
    const char* label;
};

enum class IconRailOrientation {
    Vertical,   // columna apilada — usado tanto para rail izquierdo como derecho
                // (el orden de layout del panel que lo llama decide el lado)
    Horizontal, // fila — usado para barras arriba (ej. Home)
};

// Ancho fijo de columna (Vertical) / alto fijo de fila (Horizontal).
inline constexpr float kIconRailVerticalSize    = 82.0f;
inline constexpr float kIconRailHorizontalSize  = 64.0f;
inline constexpr float kIconRailHorizontalItemW = 90.0f;

// Dibuja el rail completo (fondo, hover, barra de seleccion, icono+label) y
// actualiza currentIndex al click. categoryColor debe tener exactamente
// `count` entradas, mismo orden que items. Debe llamarse ya posicionado
// dentro del child/ventana que le corresponde (mismo patron que
// LibrarySidebar::RenderCategoryButtons / HomeSidebar::RenderHomeSidebar).
void RenderIconRail(const IconRailItem* items, int count, int& currentIndex,
                     IconRailOrientation orientation, const float (*categoryColor)[4]);

} // namespace ProyecThor::UI
