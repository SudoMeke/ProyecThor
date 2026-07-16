#pragma once

#include <imgui.h>
#include <string>
#include "GlassRenderer.h"

namespace ProyecThor::Settings { struct ThemeSettings; }

namespace ProyecThor::UI::DS {

constexpr float RadiusSmall  =  8.0f;
constexpr float RadiusMedium = 12.0f;
constexpr float RadiusLarge  = 16.0f;
constexpr float RowHeight    = 40.0f;
constexpr float ButtonHeight = 36.0f;

inline ImU32 TextPrimary   = IM_COL32(228, 246, 250, 255);
inline ImU32 TextSecondary = IM_COL32(122, 184, 208, 255);
inline ImU32 TextHint      = IM_COL32( 58, 104, 128, 255);

inline ImU32 AccentColor    = IM_COL32( 40, 200, 216, 255);
inline ImU32 AccentLight    = IM_COL32(110, 232, 240, 255);
inline ImU32 AccentPastel   = IM_COL32(168, 240, 248, 255);
inline ImU32 AccentColorDim = IM_COL32( 40, 200, 216, 255);
inline ImU32 AccentColorHov = IM_COL32( 70, 220, 232, 255);

inline ImU32 DangerColor    = IM_COL32(240,  90, 100, 255);
inline ImU32 DangerColorDim = IM_COL32(240,  90, 100, 255);
inline ImU32 SuccessColor   = IM_COL32( 82, 224, 160, 255);

// Fondo del panel — ahora sólido (sin blur ni alpha), un solo tono parejo.
inline ImU32 GlassFillTop   = IM_COL32( 18,  22,  40, 255);
inline ImU32 GlassFillBot   = IM_COL32( 12,  15,  30, 255);
inline ImU32 GlassTint      = IM_COL32(  0,   0,   0,   0); // ya no se usa como overlay
inline ImU32 GlassBorder    = IM_COL32(100, 210, 230, 255);
inline ImU32 GlassHighlight = IM_COL32(180, 245, 255, 255);
inline ImU32 GlassShadow    = IM_COL32(  0,  10,  30, 255);

inline ImU32 RowSelectedFill = IM_COL32( 40, 200, 216, 255);
inline ImU32 RowSelectedBar  = IM_COL32( 60, 210, 220, 255);
inline ImU32 RowHoverFill    = IM_COL32( 30,  38,  60, 255);

inline ImU32 BtnDefaultFill  = IM_COL32( 26,  30,  48, 255);
inline ImU32 BtnDefaultBord  = IM_COL32(140, 210, 230, 255);
inline ImU32 BtnHoverFill    = IM_COL32( 34,  48,  62, 255);
inline ImU32 BtnHoverBord    = IM_COL32(100, 225, 240, 255);

inline ImU32 SepColor        = IM_COL32(100, 210, 230, 255);

void SyncFromTheme(const ProyecThor::Settings::ThemeSettings& theme);

bool BeginGlassPanel(const char* name, GlassRenderer& glass, bool* open = nullptr,
                     ImGuiWindowFlags flags = 0, ImVec2 windowPadding = ImVec2(14.0f, 12.0f));
void EndGlassPanel();
bool GlassButton(const char* label, const ImVec2& size = ImVec2(0.0f, ButtonHeight), ImU32 accent = AccentColor);
bool GlassListRow(const char* label, bool selected, float indent = 14.0f, float height = RowHeight);
void GlassSeparator(float thickness = 1.0f);
void GlassSectionHeader(const char* label);

}