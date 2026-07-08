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
static ImU32 ToU32Alpha(const float* v, float a) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(v[0], v[1], v[2], a));
}

static ImU32 WithAlpha(ImU32 col, int a)
{
    return (col & 0x00FFFFFFu) | (static_cast<ImU32>(std::clamp(a, 0, 255)) << 24);
}

void SyncFromTheme(const ProyecThor::Settings::ThemeSettings& t) {
    // ... (sin cambios, igual que antes)
    TextPrimary   = ToU32(t.textPrimary);
    TextSecondary = ToU32(t.textDim);
    TextHint      = ToU32(t.textFaint);

    AccentColor    = ToU32(t.accent);
    AccentLight    = ToU32(t.accentLight);
    AccentPastel   = ToU32Alpha(t.accentLight, 0.9f);
    AccentColorDim = ToU32Alpha(t.accent, 0.30f);
    AccentColorHov = ToU32Alpha(t.accent, 0.78f);

    DangerColor    = ToU32(t.danger);
    DangerColorDim = ToU32Alpha(t.danger, 0.25f);
    SuccessColor   = ToU32(t.success);

    GlassFillTop   = ToU32Alpha(t.surface1, 0.74f);
    GlassFillBot   = ToU32Alpha(t.base,     0.82f);
    GlassTint      = ToU32Alpha(t.accent,   0.07f);
    GlassBorder    = ToU32Alpha(t.accent,   0.28f);
    GlassHighlight = ToU32Alpha(t.textPrimary, 0.37f);
    GlassShadow    = IM_COL32(0, 0, 0, 90);

    RowSelectedFill = ToU32Alpha(t.accent, 0.20f);
    RowSelectedBar  = ToU32(t.accentLight);
    RowHoverFill    = ToU32Alpha(t.textPrimary, 0.06f);

    BtnDefaultFill  = ToU32Alpha(t.textPrimary, 0.06f);
    BtnDefaultBord  = ToU32Alpha(t.accent, 0.22f);
    BtnHoverFill    = ToU32Alpha(t.accent, 0.11f);
    BtnHoverBord    = ToU32Alpha(t.accent, 0.35f);

    SepColor        = ToU32Alpha(t.accent, 0.14f);
}

bool BeginGlassPanel(const char* name, GlassRenderer& glass, bool* open,
                     ImGuiWindowFlags flags, ImVec2 windowPadding)
{
    // Ventana completamente transparente — dibujamos nosotros el fondo
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

        const float W = static_cast<float>(glass.GetScreenWidth());
        const float H = static_cast<float>(glass.GetScreenHeight());

        // ── 1. Capa de blur (fondo desenfocado capturado) ─────────────────
        if (W > 0.f && H > 0.f)
        {
            // UV con Y invertido (OpenGL)
            ImVec2 uv0(winPos.x / W,  1.0f - winPos.y / H);
            ImVec2 uv1(winMax.x / W,  1.0f - winMax.y / H);

            dl->AddImageRounded(
                static_cast<ImTextureID>(glass.GetBlurredTexture()),
                winPos, winMax,
                uv0, uv1,
                IM_COL32(255, 255, 255, 230),   // α: opacidad del blur
                RadiusLarge);
        }

        // ── 2. Capa de color semi-opaca sobre el blur ─────────────────────
        //    Top más claro (toque azulado) → bottom más oscuro
        ImU32 bodyTop = IM_COL32( 30,  36,  72, 185); // azul oscuro α≈73%
        ImU32 bodyBot = IM_COL32( 14,  18,  42, 210); // azul muy oscuro α≈82%
        dl->AddRectFilledMultiColor(winPos, winMax,
            bodyTop, bodyTop, bodyBot, bodyBot);
        // Clip a borde redondeado
        dl->AddRectFilled(winPos, winMax, IM_COL32(0,0,0,0), RadiusLarge);

        // ── 3. Tint glass (reflejo frío) ───────────────────────────────────
        dl->AddRectFilled(winPos, winMax, GlassTint, RadiusLarge);

        // ── 4. Borde luminoso exterior ─────────────────────────────────────
        dl->AddRect(winPos, winMax, GlassBorder, RadiusLarge, 0, 1.0f);

        // ── 5. Highlight especular superior (línea de luz) ─────────────────
        {
            float hy  = winPos.y + 1.0f;
            float hx0 = winPos.x + RadiusLarge;
            float hx1 = winMax.x - RadiusLarge;
            if (hx1 > hx0)
                dl->AddLine(ImVec2(hx0, hy), ImVec2(hx1, hy), GlassHighlight, 1.0f);
        }

        // ── 6. Sombra suave inferior (borde de profundidad) ────────────────
        {
            float sy  = winMax.y - 1.5f;
            float sx0 = winPos.x + RadiusLarge;
            float sx1 = winMax.x - RadiusLarge;
            if (sx1 > sx0)
                dl->AddLine(ImVec2(sx0, sy), ImVec2(sx1, sy),
                            IM_COL32(0, 0, 0, 60), 1.5f);
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

    // Auto-size: margen horizontal generoso, altura mínima
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

    // ── Fondo del botón ────────────────────────────────────────────────────
    ImU32 bgTop, bgBot, border, highlight;

    if (active) {
        // Presionado: fondo de acento sólido
        bgTop     = WithAlpha(accent, 230);
        bgBot     = WithAlpha(accent, 200);
        border    = WithAlpha(accent, 255);
        highlight = IM_COL32(255, 255, 255, 120);
    } else if (hovered) {
        // Hover: translúcido más visible + borde más brillante
        bgTop     = IM_COL32(255, 255, 255, 38);
        bgBot     = IM_COL32(255, 255, 255, 22);
        border    = BtnHoverBord;
        highlight = GlassHighlight;
    } else {
        // Reposo: glass muy sutil
        bgTop     = IM_COL32(255, 255, 255, 22);
        bgBot     = IM_COL32(255, 255, 255, 10);
        border    = BtnDefaultBord;
        highlight = WithAlpha(GlassHighlight, 60);
    }

    // Gradiente vertical
    dl->AddRectFilledMultiColor(bMin, bMax, bgTop, bgTop, bgBot, bgBot);
    // Borde
    dl->AddRect(bMin, bMax, border, RadiusMedium, 0, 1.0f);
    // Highlight superior
    float hx0 = bMin.x + RadiusMedium;
    float hx1 = bMax.x - RadiusMedium;
    if (hx1 > hx0)
        dl->AddLine(ImVec2(hx0, bMin.y + 0.5f), ImVec2(hx1, bMin.y + 0.5f),
                    highlight, 1.0f);

    // ── Texto centrado ─────────────────────────────────────────────────────
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

    // ID único basado en label + posición
    ImGui::PushID(label);
    bool clicked = ImGui::InvisibleButton("##row", ImVec2(rowW, height));
    bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 rMin = cursor;
    ImVec2 rMax = ImVec2(cursor.x + rowW, cursor.y + height);

    // ── Fondo de fila ──────────────────────────────────────────────────────
    if (selected) {
        // Degradado sutil del acento
        dl->AddRectFilledMultiColor(rMin, rMax,
            IM_COL32( 99, 112, 255, 65), IM_COL32( 99, 112, 255, 45),
            IM_COL32( 99, 112, 255, 35), IM_COL32( 99, 112, 255, 55));

        // Barra lateral indicadora
        dl->AddRectFilled(
            rMin,
            ImVec2(rMin.x + 3.0f, rMax.y),
            RowSelectedBar,
            1.5f);

        // Borde inferior sutil
        dl->AddLine(
            ImVec2(rMin.x + 4.0f, rMax.y - 0.5f),
            ImVec2(rMax.x,        rMax.y - 0.5f),
            IM_COL32(99, 112, 255, 40), 1.0f);

    } else if (hovered) {
        dl->AddRectFilled(rMin, rMax, RowHoverFill, RadiusSmall * 0.5f);
        // Borde sutil al hover
        dl->AddRect(rMin, rMax, IM_COL32(255, 255, 255, 18), RadiusSmall * 0.5f, 0, 0.5f);
    }

    // ── Texto alineado verticalmente ───────────────────────────────────────
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

    // Gradiente: transparente → visible → transparente
    float mid = p.x + w * 0.5f;
    dl->AddRectFilledMultiColor(
        ImVec2(p.x,   p.y),
        ImVec2(mid,   p.y + thickness),
        IM_COL32(255,255,255,0),
        IM_COL32(255,255,255,40),
        IM_COL32(255,255,255,40),
        IM_COL32(255,255,255,0));
    dl->AddRectFilledMultiColor(
        ImVec2(mid,   p.y),
        ImVec2(p.x+w, p.y + thickness),
        IM_COL32(255,255,255,40),
        IM_COL32(255,255,255,0),
        IM_COL32(255,255,255,0),
        IM_COL32(255,255,255,40));

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

    // Texto
    dl->AddText(p, TextHint, label);

    // Línea decorativa a la derecha
    float lx0 = p.x + textSz.x + 8.0f;
    float lx1 = p.x + w;
    if (lx1 > lx0)
        dl->AddRectFilledMultiColor(
            ImVec2(lx0, lineY),
            ImVec2(lx1, lineY + 1.0f),
            IM_COL32(255,255,255,30),
            IM_COL32(255,255,255,0),
            IM_COL32(255,255,255,0),
            IM_COL32(255,255,255,30));

    ImGui::Dummy(ImVec2(w, textSz.y + 6.0f));
}

} // namespace ProyecThor::UI::DS