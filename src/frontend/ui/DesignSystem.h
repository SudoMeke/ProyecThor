#pragma once

#include <imgui.h>
#include <string>
#include "GlassRenderer.h"

// ─────────────────────────────────────────────────────────────────────────────
//  ProyecThor Design System — Paleta Cian / Pastel / Liquid Glass
//
//  Fondo oscuro marino (#0d1824) + acento cian (#28c8d8) + pastel suave.
// ─────────────────────────────────────────────────────────────────────────────

namespace ProyecThor::UI::DS {

// ── Radios ─────────────────────────────────────────────────────────────────
constexpr float RadiusSmall  =  8.0f;
constexpr float RadiusMedium = 12.0f;
constexpr float RadiusLarge  = 16.0f;

// ── Alturas ────────────────────────────────────────────────────────────────
constexpr float RowHeight    = 40.0f;
constexpr float ButtonHeight = 36.0f;

// ── Texto ──────────────────────────────────────────────────────────────────
constexpr ImU32 TextPrimary   = IM_COL32(228, 246, 250, 255); // #e4f6fa blanco cian
constexpr ImU32 TextSecondary = IM_COL32(122, 184, 208, 255); // #7ab8d0 azul pizarra
constexpr ImU32 TextHint      = IM_COL32( 58, 104, 128, 255); // #3a6880 hint oscuro

// ── Acento cian (identidad ProyecThor) ─────────────────────────────────────
constexpr ImU32 AccentColor    = IM_COL32( 40, 200, 216, 255); // #28c8d8
constexpr ImU32 AccentLight    = IM_COL32(110, 232, 240, 255); // #6ee8f0
constexpr ImU32 AccentPastel   = IM_COL32(168, 240, 248, 255); // #a8f0f8
constexpr ImU32 AccentColorDim = IM_COL32( 40, 200, 216,  75);
constexpr ImU32 AccentColorHov = IM_COL32( 70, 220, 232, 200);

// ── Danger / Success ───────────────────────────────────────────────────────
constexpr ImU32 DangerColor    = IM_COL32(240,  90, 100, 255);
constexpr ImU32 DangerColorDim = IM_COL32(240,  90, 100,  65);
constexpr ImU32 SuccessColor   = IM_COL32( 82, 224, 160, 255);

// ── Liquid Glass ───────────────────────────────────────────────────────────
constexpr ImU32 GlassFillTop   = IM_COL32( 14,  34,  58, 188); // marino oscuro
constexpr ImU32 GlassFillBot   = IM_COL32(  8,  18,  36, 210); // marino profundo
constexpr ImU32 GlassTint      = IM_COL32( 60, 200, 220,  18); // tint cian suave
constexpr ImU32 GlassBorder    = IM_COL32(100, 210, 230,  70); // borde cian
constexpr ImU32 GlassHighlight = IM_COL32(180, 245, 255,  95); // luz fría superior
constexpr ImU32 GlassShadow    = IM_COL32(  0,  10,  30,  90);

// ── Filas ──────────────────────────────────────────────────────────────────
constexpr ImU32 RowSelectedFill = IM_COL32( 40, 200, 216,  50);
constexpr ImU32 RowSelectedBar  = IM_COL32( 60, 210, 220, 255);
constexpr ImU32 RowHoverFill    = IM_COL32(200, 240, 255,  14);

// ── Botones ────────────────────────────────────────────────────────────────
constexpr ImU32 BtnDefaultFill  = IM_COL32(255, 255, 255,  16);
constexpr ImU32 BtnDefaultBord  = IM_COL32(140, 210, 230,  55);
constexpr ImU32 BtnHoverFill    = IM_COL32( 60, 200, 220,  28);
constexpr ImU32 BtnHoverBord    = IM_COL32(100, 225, 240,  90);

// ── Separador ──────────────────────────────────────────────────────────────
constexpr ImU32 SepColor        = IM_COL32(100, 210, 230,  35);

// ---------------------------------------------------------------------------
bool BeginGlassPanel(const char*      name,
                     GlassRenderer&   glass,
                     bool*            open          = nullptr,
                     ImGuiWindowFlags flags         = 0,
                     ImVec2           windowPadding = ImVec2(14.0f, 12.0f));
void EndGlassPanel();

bool GlassButton(const char*   label,
                 const ImVec2& size   = ImVec2(0.0f, ButtonHeight),
                 ImU32         accent = AccentColor);

bool GlassListRow(const char* label,
                  bool        selected,
                  float       indent = 14.0f,
                  float       height = RowHeight);

void GlassSeparator(float thickness = 1.0f);
void GlassSectionHeader(const char* label);

} // namespace ProyecThor::UI::DS