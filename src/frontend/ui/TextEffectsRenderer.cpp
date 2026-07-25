#include "TextEffectsRenderer.h"
#include <cmath>

namespace ProyecThor::UI {

using Core::TextEffectsData;

static ImU32 ColU32(const float c[4], float alphaMul = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3] * alphaMul));
}

// Anillo de N copias del texto a un radio dado, alpha decreciente hacia
// afuera -- aproxima un glow/blur sin necesitar renderizar el texto a una
// textura aparte (ver TextEffectsRenderer.h).
static void DrawTextGlowRing(ImDrawList* dl, ImFont* font, float fontSize, ImVec2 pos,
                             const char* text, float wrapWidth,
                             const float color[4], float intensity, float maxRadiusPx)
{
    if (intensity <= 0.0f || maxRadiusPx <= 0.0f) return;

    const int kRings       = 3;
    const int kPtsPerRing  = 8;
    for (int r = 1; r <= kRings; r++) {
        float radius = maxRadiusPx * ((float)r / (float)kRings);
        float alpha  = intensity * (1.0f - (float)(r - 1) / (float)kRings) * 0.35f;
        ImU32 col    = ColU32(color, alpha);

        for (int i = 0; i < kPtsPerRing; i++) {
            float a = (6.2831853f / (float)kPtsPerRing) * (float)i;
            ImVec2 off(cosf(a) * radius, sinf(a) * radius);
            dl->AddText(font, fontSize, { pos.x + off.x, pos.y + off.y }, col, text, nullptr, wrapWidth);
        }
    }
}

void DrawStyledText(ImDrawList* dl, ImFont* font, float fontSize, ImVec2 pos,
                     ImU32 textCol, const char* text, float wrapWidth, float scale,
                     const TextEffectsData& fx, float globalAlpha)
{
    if (!text || !*text || !font) return;
    if (globalAlpha <= 0.001f) return;

    // 1) Fondo -- rectangulo detras de todo el bloque. Padding proporcional
    // al tamaño de fuente, no fijo, para que se vea bien en cualquier
    // resolucion/tamaño de texto.
    if (fx.bgEnabled) {
        ImVec2 textSz = font->CalcTextSizeA(fontSize, FLT_MAX, wrapWidth, text);
        float  pad    = fontSize * 0.22f;
        dl->AddRectFilled(
            { pos.x - pad, pos.y - pad },
            { pos.x + textSz.x + pad, pos.y + textSz.y + pad },
            ColU32(fx.bgColor, globalAlpha), 6.0f * scale);
    }

    // 2) Glow ("bloom") y Neon -- anillos concentricos de copias del texto,
    // mismo truco que el halo de DrawPadButton (ViewPanel.cpp) pero aplicado
    // a glifos. Neon usa un radio mas ajustado (glow "pegado" al texto, mas
    // saturado) en vez del glow difuso y amplio de Bloom.
    if (fx.glowEnabled)
        DrawTextGlowRing(dl, font, fontSize, pos, text, wrapWidth, fx.glowColor, fx.glowIntensity * globalAlpha, fontSize * 0.35f);
    if (fx.neonEnabled)
        DrawTextGlowRing(dl, font, fontSize, pos, text, wrapWidth, fx.neonColor, fx.neonIntensity * globalAlpha, fontSize * 0.20f);

    // 3) Borde -- 8 copias del texto en anillo cerrado detras del texto
    // principal (outline clasico sin shader).
    if (fx.borderEnabled) {
        float radius = (1.0f + fx.borderWidth * 3.0f) * scale;
        ImU32 col    = ColU32(fx.borderColor, globalAlpha);
        static const float kAngles[8] = { 0.0f, 45.0f, 90.0f, 135.0f, 180.0f, 225.0f, 270.0f, 315.0f };
        for (float degrees : kAngles) {
            float  a = degrees * 3.14159265f / 180.0f;
            ImVec2 off(cosf(a) * radius, sinf(a) * radius);
            dl->AddText(font, fontSize, { pos.x + off.x, pos.y + off.y }, col, text, nullptr, wrapWidth);
        }
    }

    // 4) Aberracion cromatica -- copias rojo/azul desfasadas horizontalmente
    // detras del texto (verde lo aporta el texto principal, blanco = RGB).
    if (fx.chromaticAberrationEnabled) {
        float shift = fx.chromaticAberrationIntensity * 4.0f * scale;
        ImU32 redCol  = IM_COL32(235, 60, 60, (int)(140 * globalAlpha));
        ImU32 blueCol = IM_COL32(60, 120, 235, (int)(140 * globalAlpha));
        dl->AddText(font, fontSize, { pos.x - shift, pos.y }, redCol, text, nullptr, wrapWidth);
        dl->AddText(font, fontSize, { pos.x + shift, pos.y }, blueCol, text, nullptr, wrapWidth);
    }

    // 5) Sombra -- una copia offset (mismo comportamiento que antes tenia
    // fijo este codebase, ahora configurable).
    if (fx.shadowEnabled) {
        float off = (1.0f + fx.shadowIntensity * 4.0f) * scale;
        dl->AddText(font, fontSize, { pos.x + off, pos.y + off }, ColU32(fx.shadowColor, globalAlpha), text, nullptr, wrapWidth);
    }

    // 6) Texto principal.
    dl->AddText(font, fontSize, pos, textCol, text, nullptr, wrapWidth);

    // 7) Subrayado.
    if (fx.underlineEnabled) {
        ImVec2 textSz = font->CalcTextSizeA(fontSize, FLT_MAX, wrapWidth, text);
        float  thick  = (1.0f + fx.underlineThickness * 5.0f) * scale;
        float  y      = pos.y + textSz.y + 2.0f * scale;
        dl->AddLine({ pos.x, y }, { pos.x + textSz.x, y }, ColU32(fx.underlineColor, globalAlpha), thick);
    }
}

} // namespace ProyecThor::UI
