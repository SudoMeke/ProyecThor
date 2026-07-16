#include "DesignSystem.h"
#include "SettingsManager.h"
#include <imgui_internal.h>
#include <cmath>
#include <algorithm>
#include <cfloat>

namespace ProyecThor::UI::DS {

static ImU32 ToU32(const float* v) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(v[0], v[1], v[2], v[3]));
}
static ImU32 ToU32Opaque(const float* v) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(v[0], v[1], v[2], 1.0f));
}

// ── BlendOver ────────────────────────────────────────────────────────────
//  Pre-mezcla un tint semitransparente (tint, alpha) contra un color de
//  fondo base, y devuelve el resultado como color SOLIDO (alpha=255).
//  Esto es lo que reemplaza a "poner alpha 255 directo al tint": sin esto,
//  cualquier tint casi-blanco (como t.textPrimary usado al 6%) se veia
//  como blanco puro en vez de una insinuacion sutil sobre el fondo.
static ImU32 BlendOver(const float* tint, float alpha, const float* base)
{
    float r = tint[0] * alpha + base[0] * (1.0f - alpha);
    float g = tint[1] * alpha + base[1] * (1.0f - alpha);
    float b = tint[2] * alpha + base[2] * (1.0f - alpha);
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, 1.0f));
}

static ImU32 WithAlpha(ImU32 col, int /*a*/)
{
    return (col & 0x00FFFFFFu) | (255u << 24);
}

void SyncFromTheme(const ProyecThor::Settings::ThemeSettings& t) {
    TextPrimary   = ToU32(t.textPrimary);
    TextSecondary = ToU32(t.textDim);
    TextHint      = ToU32(t.textFaint);

    AccentColor    = ToU32(t.accent);
    AccentLight    = ToU32(t.accentLight);
    AccentPastel   = ToU32Opaque(t.accentLight);

    // Antes: tint low-alpha sobre lo que hubiera debajo. Ahora: pre-mezclado
    // contra t.base (el fondo real del panel), asi queda opaco pero con el
    // mismo aspecto visual aproximado.
    AccentColorDim = BlendOver(t.accent, 0.30f, t.base);
    AccentColorHov = BlendOver(t.accent, 0.78f, t.base);

    DangerColor    = ToU32(t.danger);
    DangerColorDim = BlendOver(t.danger, 0.25f, t.base);
    SuccessColor   = ToU32(t.success);

    GlassFillTop   = ToU32Opaque(t.surface1);
    GlassFillBot   = ToU32Opaque(t.base);
    GlassTint      = IM_COL32(0, 0, 0, 0); // sin uso, ya no hay overlay
    GlassBorder    = BlendOver(t.accent,      0.28f, t.base);
    GlassHighlight = BlendOver(t.textPrimary, 0.37f, t.base); // <- el culpable original
    GlassShadow    = IM_COL32(0, 0, 0, 255);

    RowSelectedFill = BlendOver(t.accent,      0.20f, t.base);
    RowSelectedBar  = ToU32(t.accentLight);
    RowHoverFill    = BlendOver(t.textPrimary, 0.06f, t.base); // <- y este tambien

    BtnDefaultFill  = BlendOver(t.textPrimary, 0.06f, t.base); // <- este era el peor: fondo de TODOS los botones "default"
    BtnDefaultBord  = BlendOver(t.accent,      0.22f, t.base);
    BtnHoverFill    = BlendOver(t.accent,      0.11f, t.base);
    BtnHoverBord    = BlendOver(t.accent,      0.35f, t.base);

    SepColor        = BlendOver(t.accent, 0.14f, t.base);
}

bool BeginGlassPanel(const char* name, GlassRenderer& /*glass*/, bool* open,
                     ImGuiWindowFlags flags, ImVec2 windowPadding)
{
    // Ventana con fondo sólido, dibujado a mano igual que antes, pero
    // sin la capa de blur ni ningun canal de transparencia.
    ImGui::PushStyleColor(ImGuiCol_WindowBg,    ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_Border,      ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg,     ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive,ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,  RadiusLarge);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    windowPadding);

    bool visible = ImGui::Begin(name, open, flags);

    if (visible)
    {
        ImVec2 winPos  = ImGui::GetWindowPos();
        ImVec2 winSize = ImGui::GetWindowSize();
        ImVec2 winMax  = ImVec2(winPos.x + winSize.x, winPos.y + winSize.y);
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // ── 1. Fondo sólido (reemplaza la capa de blur + overlay semi-opaco) ──
        //    Antes: AddImageRounded(blur) + AddRectFilledMultiColor con alpha.
        //    Ahora: un solo relleno solido con leve degrade top→bottom, sin
        //    ningun canal alpha < 255.
        dl->AddRectFilledMultiColor(winPos, winMax,
            GlassFillTop, GlassFillTop, GlassFillBot, GlassFillBot);

        // ── 2. Borde exterior ────────────────────────────────────────────────
        dl->AddRect(winPos, winMax, GlassBorder, RadiusLarge, 0, 1.0f);

        // ── 3. Highlight especular superior (línea de luz) ─────────────────
        {
            float hy  = winPos.y + 1.0f;
            float hx0 = winPos.x + RadiusLarge;
            float hx1 = winMax.x - RadiusLarge;
            if (hx1 > hx0)
                dl->AddLine(ImVec2(hx0, hy), ImVec2(hx1, hy), GlassHighlight, 1.0f);
        }

        // ── 4. Sombra inferior (borde de profundidad, ahora opaca) ─────────
        {
            float sy  = winMax.y - 1.5f;
            float sx0 = winPos.x + RadiusLarge;
            float sx1 = winMax.x - RadiusLarge;
            if (sx1 > sx0)
                dl->AddLine(ImVec2(sx0, sy), ImVec2(sx1, sy),
                            IM_COL32(0, 0, 0, 255), 1.5f);
        }
    }

    return visible;
}

void EndGlassPanel()
{
    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(4);
}

// ── GlassButton ────────────────────────────────────────────────────────────

bool GlassButton(const char* label, const ImVec2& size, ImU32 accent)
{
    ImVec2 sz = size;
    ImVec2 textSize = ImGui::CalcTextSize(label, nullptr, true);

    if (sz.x <= 0.0f) sz.x = textSize.x + 32.0f;
    if (sz.y <= 0.0f) sz.y = ButtonHeight;

    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(label, sz);

    bool hovered = ImGui::IsItemHovered();
    bool active  = ImGui::IsItemActive();
    bool clicked = ImGui::IsItemClicked();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 bMin = cursor;
    ImVec2 bMax = ImVec2(cursor.x + sz.x, cursor.y + sz.y);

    ImU32 bgTop, bgBot, border, highlight;

    if (active) {
        bgTop     = WithAlpha(accent, 255);
        bgBot     = WithAlpha(accent, 255);
        border    = WithAlpha(accent, 255);
        highlight = IM_COL32(255, 255, 255, 255);
    } else if (hovered) {
        bgTop     = BtnHoverFill;
        bgBot     = BtnHoverFill;
        border    = BtnHoverBord;
        highlight = GlassHighlight;
    } else {
        bgTop     = BtnDefaultFill;
        bgBot     = BtnDefaultFill;
        border    = BtnDefaultBord;
        highlight = GlassHighlight;
    }

    dl->AddRectFilledMultiColor(bMin, bMax, bgTop, bgTop, bgBot, bgBot);
    dl->AddRect(bMin, bMax, border, RadiusMedium, 0, 1.0f);

    float hx0 = bMin.x + RadiusMedium;
    float hx1 = bMax.x - RadiusMedium;
    if (hx1 > hx0)
        dl->AddLine(ImVec2(hx0, bMin.y + 0.5f), ImVec2(hx1, bMin.y + 0.5f),
                    highlight, 1.0f);

    ImU32 textCol = active ? IM_COL32(255, 255, 255, 255) : TextPrimary;
    ImVec2 tp(
        bMin.x + std::floor((sz.x - textSize.x) * 0.5f),
        bMin.y + std::floor((sz.y - textSize.y) * 0.5f));
    dl->AddText(tp, textCol, label);

    return clicked;
}

// ── GlassListRow ───────────────────────────────────────────────────────────

bool GlassListRow(const char* label, bool selected, float indent, float height)
{
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    float  rowW   = ImGui::GetContentRegionAvail().x;

    ImGui::PushID(label);
    bool clicked = ImGui::InvisibleButton("##row", ImVec2(rowW, height));
    bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 rMin = cursor;
    ImVec2 rMax = ImVec2(cursor.x + rowW, cursor.y + height);

    if (selected) {
        dl->AddRectFilled(rMin, rMax, IM_COL32(45, 52, 90, 255));

        dl->AddRectFilled(
            rMin,
            ImVec2(rMin.x + 3.0f, rMax.y),
            RowSelectedBar,
            1.5f);

        dl->AddLine(
            ImVec2(rMin.x + 4.0f, rMax.y - 0.5f),
            ImVec2(rMax.x,        rMax.y - 0.5f),
            IM_COL32(99, 112, 255, 255), 1.0f);

    } else if (hovered) {
        dl->AddRectFilled(rMin, rMax, RowHoverFill, RadiusSmall * 0.5f);
        dl->AddRect(rMin, rMax, IM_COL32(255, 255, 255, 255), RadiusSmall * 0.5f, 0, 0.5f);
    }

    ImFont* font = ImGui::GetFont();
    float fontSize     = ImGui::GetFontSize();
    ImVec2 textSz      = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label);

    float textX = rMin.x + indent;
    float textY = rMin.y + std::floor((height - textSz.y) * 0.5f);

    ImU32 textCol = selected ? TextPrimary : TextSecondary;
    dl->AddText(ImVec2(textX, textY), textCol, label);

    return clicked;
}

// ── GlassSeparator ─────────────────────────────────────────────────────────

void GlassSeparator(float thickness)
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    float  w = ImGui::GetContentRegionAvail().x;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Antes tenia un degrade a transparente en los extremos; ahora es una
    // linea solida pareja.
    dl->AddRectFilled(
        ImVec2(p.x,   p.y),
        ImVec2(p.x+w, p.y + thickness),
        SepColor);

    ImGui::Dummy(ImVec2(w, thickness + 2.0f));
}

// ── GlassSectionHeader ─────────────────────────────────────────────────────

void GlassSectionHeader(const char* label)
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    float  w = ImGui::GetContentRegionAvail().x;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec2 textSz = ImGui::CalcTextSize(label);
    float lineY   = p.y + textSz.y * 0.5f;

    dl->AddText(p, TextHint, label);

    float lx0 = p.x + textSz.x + 8.0f;
    float lx1 = p.x + w;
    if (lx1 > lx0)
        dl->AddRectFilled(
            ImVec2(lx0, lineY),
            ImVec2(lx1, lineY + 1.0f),
            SepColor);

    ImGui::Dummy(ImVec2(w, textSz.y + 6.0f));
}

} // namespace ProyecThor::UI::DS