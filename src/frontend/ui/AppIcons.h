#pragma once
#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>

namespace ProyecThor::UI::AppIcons {

// Mismo helper que LibraryIcons.h::IcPt / HomeIcons.h::IcPt.
inline ImVec2 IcPt(ImVec2 o, float sz, float rx, float ry)
{
    return { o.x + rx * sz, o.y + ry * sz };
}

// Control — sliders de mezcla (mixer)
inline void DrawIcon_Mixer(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.07f;
    const float xs[3]    = { 0.26f, 0.50f, 0.74f };
    const float knobY[3] = { 0.62f, 0.34f, 0.50f };
    for (int i = 0; i < 3; i++) {
        dl->AddLine(IcPt(o, sz, xs[i], 0.14f), IcPt(o, sz, xs[i], 0.86f), col, thick);
        dl->AddCircleFilled(IcPt(o, sz, xs[i], knobY[i]), sz * 0.09f, col, 12);
    }
}

// Stage Display — monitor con base
inline void DrawIcon_Monitor(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.07f;
    dl->AddRect(IcPt(o, sz, 0.12f, 0.14f), IcPt(o, sz, 0.88f, 0.66f),
                col, sz * 0.05f, ImDrawFlags_RoundCornersAll, thick);
    dl->AddLine(IcPt(o, sz, 0.5f, 0.66f), IcPt(o, sz, 0.5f, 0.80f), col, thick);
    dl->AddLine(IcPt(o, sz, 0.30f, 0.86f), IcPt(o, sz, 0.70f, 0.86f), col, thick);
}

// Fondos — capas apiladas
inline void DrawIcon_Layers(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.065f;
    const float ys[3] = { 0.28f, 0.50f, 0.72f };
    for (float y : ys) {
        dl->AddRect(IcPt(o, sz, 0.16f, y), IcPt(o, sz, 0.84f, y + 0.16f),
                    col, sz * 0.03f, ImDrawFlags_RoundCornersAll, thick);
    }
}

// Estilos — paleta de pintor
inline void DrawIcon_Palette(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.06f;
    dl->PathArcTo(IcPt(o, sz, 0.5f, 0.55f), sz * 0.36f, IM_PI * 0.85f, IM_PI * 2.65f, 24);
    dl->PathStroke(col, ImDrawFlags_None, thick);
    // Hueco del pulgar
    dl->AddCircleFilled(IcPt(o, sz, 0.5f, 0.78f), sz * 0.08f, col, 10);
    // Puntos de color (mismo color, solo como marcas de la paleta)
    const float dotsX[3] = { 0.32f, 0.50f, 0.68f };
    for (float x : dotsX)
        dl->AddCircleFilled(IcPt(o, sz, x, 0.30f), sz * 0.06f, col, 10);
}

// Overlays — marco de imagen (sol + montaña) con una linea de texto debajo
inline void DrawIcon_Overlay(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.065f;
    dl->AddRect(IcPt(o, sz, 0.14f, 0.14f), IcPt(o, sz, 0.86f, 0.68f),
                col, sz * 0.04f, ImDrawFlags_RoundCornersAll, thick);
    dl->AddCircleFilled(IcPt(o, sz, 0.32f, 0.32f), sz * 0.06f, col, 10);
    dl->AddTriangleFilled(
        IcPt(o, sz, 0.20f, 0.60f), IcPt(o, sz, 0.42f, 0.36f), IcPt(o, sz, 0.62f, 0.60f), col);
    dl->AddTriangleFilled(
        IcPt(o, sz, 0.44f, 0.60f), IcPt(o, sz, 0.66f, 0.40f), IcPt(o, sz, 0.80f, 0.60f), col);
    dl->AddRectFilled(IcPt(o, sz, 0.20f, 0.80f), IcPt(o, sz, 0.80f, 0.88f), col, sz * 0.02f);
}

// Estilos — "Aa" (icono tipico de formato de texto/tipografia)
inline void DrawIcon_TextAa(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    const char* label     = "Aa";
    float       fontSize  = sz * 0.60f;
    ImFont*     font      = ImGui::GetFont();
    ImVec2      textSz    = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label);
    ImVec2      pos       = IcPt(o, sz, 0.5f, 0.5f);
    pos.x -= textSz.x * 0.5f;
    pos.y -= textSz.y * 0.5f;
    dl->AddText(font, fontSize, pos, col, label);
}

// Transiciones — flechas cruzadas (swap)
inline void DrawIcon_Swap(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = sz * 0.07f;
    // Flecha superior (izquierda a derecha)
    dl->AddLine(IcPt(o, sz, 0.18f, 0.36f), IcPt(o, sz, 0.78f, 0.36f), col, thick);
    dl->AddTriangleFilled(
        IcPt(o, sz, 0.70f, 0.22f), IcPt(o, sz, 0.70f, 0.50f), IcPt(o, sz, 0.88f, 0.36f), col);
    // Flecha inferior (derecha a izquierda)
    dl->AddLine(IcPt(o, sz, 0.82f, 0.64f), IcPt(o, sz, 0.22f, 0.64f), col, thick);
    dl->AddTriangleFilled(
        IcPt(o, sz, 0.30f, 0.50f), IcPt(o, sz, 0.30f, 0.78f), IcPt(o, sz, 0.12f, 0.64f), col);
}

} // namespace ProyecThor::UI::AppIcons
