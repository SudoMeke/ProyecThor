#pragma once
#include <imgui.h>
#include "CanvaStyleEditor.h"

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  TabEffects — contenido del tab "Efectos" del editor de estilos: fondo,
//  borde, sombra, aberracion cromatica, glow (bloom), neon y subrayado sobre
//  las letras (ver TextEffectsData en PresentationCore.h y el dibujo en
//  TextEffectsRenderer.h). Grid de tarjetas, una por efecto, mismo lenguaje
//  visual (Badge/SectionLabel/Surface*) que el resto de los tabs de este
//  editor.
// ─────────────────────────────────────────────────────────────────────────────
class TabEffects {
public:
    TabEffects() = default;

    void Render(StyleData& data, float colWidth);

private:
    // Tarjeta generica: titulo + toggle + (opcional) color + (opcional) un
    // slider de intensidad -- cubre los 7 efectos sin repetir el layout.
    void RenderEffectCard(const char* title, const ImVec4& accent, float colWidth,
                           bool& enabled, float* color4,
                           float* intensity, const char* intensityLabel);
};

} // namespace ProyecThor::UI
