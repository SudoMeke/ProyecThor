#include "ShadersPanel.h"
#include "frontend/ui/DesignSystem.h"
#include "backend/settings/SettingsManager.h"
#include "backend/core/PresentationCore.h"
#include "backend/core/SystemStats.h"
#include <imgui.h>
#include <cmath>
#include <functional>
#include <algorithm>
#include <vector>
#include <string>

namespace ProyecThor::UI {

namespace {

// ─────────────────────────────────────────────────────────────────────────
//  Iconos — glifos simples dibujados a mano con ImDrawList (mismo enfoque
//  que los iconos de categoria del panel de Ajustes: sin depender de
//  ningun PNG/asset externo), uno por efecto.
// ─────────────────────────────────────────────────────────────────────────
void IconUpscale(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddRect(ImVec2(c.x - r * 0.85f, c.y - r * 0.85f), ImVec2(c.x + r * 0.85f, c.y + r * 0.85f), col, 3.0f, 0, 1.1f);
    dl->AddLine(ImVec2(c.x - r * 0.35f, c.y + r * 0.35f), ImVec2(c.x + r * 0.35f, c.y - r * 0.35f), col, 1.5f);
    dl->AddLine(ImVec2(c.x + r * 0.35f, c.y - r * 0.35f), ImVec2(c.x - r * 0.05f, c.y - r * 0.35f), col, 1.5f);
    dl->AddLine(ImVec2(c.x + r * 0.35f, c.y - r * 0.35f), ImVec2(c.x + r * 0.35f, c.y + r * 0.05f), col, 1.5f);
}

void IconCRT(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    ImVec2 mn(c.x - r * 0.8f, c.y - r * 0.6f), mx(c.x + r * 0.8f, c.y + r * 0.6f);
    dl->AddRect(mn, mx, col, 2.0f, 0, 1.3f);
    for (int i = -1; i <= 1; i++) {
        float y = c.y + i * r * 0.35f;
        dl->AddLine(ImVec2(mn.x + 3.0f, y), ImVec2(mx.x - 3.0f, y), col, 1.0f);
    }
}

void IconGrain(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    static const float ox[] = { -0.5f, 0.1f, 0.4f, -0.3f, 0.55f, -0.6f, 0.15f };
    static const float oy[] = { -0.4f, -0.55f, 0.1f, 0.45f, -0.15f, 0.3f, 0.55f };
    for (int i = 0; i < 7; i++)
        dl->AddCircleFilled(ImVec2(c.x + ox[i] * r, c.y + oy[i] * r), r * 0.09f, col);
}

void IconFXAA(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->PathClear();
    dl->PathLineTo(ImVec2(c.x - r * 0.8f, c.y + r * 0.55f));
    dl->PathLineTo(ImVec2(c.x - r * 0.25f, c.y + r * 0.55f));
    dl->PathLineTo(ImVec2(c.x - r * 0.25f, c.y + r * 0.05f));
    dl->PathLineTo(ImVec2(c.x + r * 0.15f, c.y + r * 0.05f));
    dl->PathLineTo(ImVec2(c.x + r * 0.15f, c.y - r * 0.45f));
    dl->PathLineTo(ImVec2(c.x + r * 0.65f, c.y - r * 0.45f));
    dl->PathStroke(col, false, 1.4f);
}

void IconSaturation(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddCircle(c, r * 0.85f, col, 20, 1.3f);
    dl->AddCircleFilled(ImVec2(c.x, c.y - r * 0.42f), r * 0.17f, col);
    dl->AddCircleFilled(ImVec2(c.x + r * 0.40f, c.y + r * 0.22f), r * 0.17f, col);
    dl->AddCircleFilled(ImVec2(c.x - r * 0.40f, c.y + r * 0.22f), r * 0.17f, col);
}

void IconVignette(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    ImVec2 mn(c.x - r * 0.85f, c.y - r * 0.6f), mx(c.x + r * 0.85f, c.y + r * 0.6f);
    dl->AddRect(mn, mx, col, 3.0f, 0, 1.2f);
    float cr = r * 0.24f;
    ImU32 corner = IM_COL32(0, 0, 0, 110);
    dl->AddCircleFilled(mn, cr, corner);
    dl->AddCircleFilled(ImVec2(mx.x, mn.y), cr, corner);
    dl->AddCircleFilled(ImVec2(mn.x, mx.y), cr, corner);
    dl->AddCircleFilled(mx, cr, corner);
}

void IconBlur(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    // 3 circulos concentricos con alpha decreciente hacia afuera -- sugiere
    // desenfoque (un punto que se "esparce").
    ImVec4 colV = ImGui::ColorConvertU32ToFloat4(col);
    dl->AddCircleFilled(c, r * 0.85f, ImGui::ColorConvertFloat4ToU32(ImVec4(colV.x, colV.y, colV.z, 0.18f)), 20);
    dl->AddCircleFilled(c, r * 0.55f, ImGui::ColorConvertFloat4ToU32(ImVec4(colV.x, colV.y, colV.z, 0.40f)), 20);
    dl->AddCircleFilled(c, r * 0.28f, col, 16);
}

void IconSharpen(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    // Diamante con puntas marcadas -- opuesto visual al blur difuso.
    dl->AddLine(ImVec2(c.x, c.y - r * 0.85f), ImVec2(c.x, c.y + r * 0.85f), col, 1.6f);
    dl->AddLine(ImVec2(c.x - r * 0.85f, c.y), ImVec2(c.x + r * 0.85f, c.y), col, 1.6f);
    dl->PathClear();
    dl->PathLineTo(ImVec2(c.x, c.y - r * 0.55f));
    dl->PathLineTo(ImVec2(c.x + r * 0.55f, c.y));
    dl->PathLineTo(ImVec2(c.x, c.y + r * 0.55f));
    dl->PathLineTo(ImVec2(c.x - r * 0.55f, c.y));
    dl->PathStroke(col, true, 1.6f);
}

void IconBloom(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    // Centro solido + rayos cortos -- resplandor que se derrama.
    dl->AddCircleFilled(c, r * 0.32f, col, 16);
    for (int i = 0; i < 8; i++) {
        float a = (6.28318530f / 8.0f) * (float)i;
        ImVec2 dir(cosf(a), sinf(a));
        ImVec2 p0(c.x + dir.x * r * 0.45f, c.y + dir.y * r * 0.45f);
        ImVec2 p1(c.x + dir.x * r * 0.85f, c.y + dir.y * r * 0.85f);
        dl->AddLine(p0, p1, col, 1.4f);
    }
}

void IconChromaticAberration(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    // 3 circulos desplazados en rojo/verde/azul -- el efecto en si mismo.
    float off = r * 0.20f;
    dl->AddCircle(ImVec2(c.x - off, c.y), r * 0.5f, IM_COL32(235, 90, 90, 200), 16, 1.4f);
    dl->AddCircle(ImVec2(c.x, c.y),       r * 0.5f, IM_COL32(90, 235, 120, 200), 16, 1.4f);
    dl->AddCircle(ImVec2(c.x + off, c.y), r * 0.5f, IM_COL32(90, 150, 235, 200), 16, 1.4f);
    (void)col; // paleta fija (RGB), no usa el acento de la tarjeta a proposito
}

void IconFill(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    // Rectangulo nitido al centro (el contenido real) con un marco difuso
    // alrededor (2 rects concentricos de alpha decreciente) sugiriendo el
    // relleno desenfocado detras.
    ImVec4 colV = ImGui::ColorConvertU32ToFloat4(col);
    ImU32  faint1 = ImGui::ColorConvertFloat4ToU32(ImVec4(colV.x, colV.y, colV.z, 0.35f));
    ImU32  faint2 = ImGui::ColorConvertFloat4ToU32(ImVec4(colV.x, colV.y, colV.z, 0.16f));
    dl->AddRect(ImVec2(c.x - r * 0.85f, c.y - r * 0.85f), ImVec2(c.x + r * 0.85f, c.y + r * 0.85f), faint2, 3.0f, 0, 3.0f);
    dl->AddRect(ImVec2(c.x - r * 0.62f, c.y - r * 0.62f), ImVec2(c.x + r * 0.62f, c.y + r * 0.62f), faint1, 3.0f, 0, 2.0f);
    dl->AddRectFilled(ImVec2(c.x - r * 0.38f, c.y - r * 0.38f), ImVec2(c.x + r * 0.38f, c.y + r * 0.38f), col, 2.0f);
}

void IconVHS(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    // Cassette: cuerpo rectangular + 2 carretes (circulos).
    ImVec2 mn(c.x - r * 0.85f, c.y - r * 0.55f), mx(c.x + r * 0.85f, c.y + r * 0.55f);
    dl->AddRect(mn, mx, col, 2.0f, 0, 1.2f);
    dl->AddCircle(ImVec2(c.x - r * 0.38f, c.y), r * 0.28f, col, 16, 1.3f);
    dl->AddCircle(ImVec2(c.x + r * 0.38f, c.y), r * 0.28f, col, 16, 1.3f);
    dl->AddLine(ImVec2(mn.x + r * 0.15f, mx.y - r * 0.12f), ImVec2(mx.x - r * 0.15f, mx.y - r * 0.12f), col, 1.2f);
}

void IconCine(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    // Tira de pelicula: rectangulo con perforaciones a los lados.
    ImVec2 mn(c.x - r * 0.55f, c.y - r * 0.85f), mx(c.x + r * 0.55f, c.y + r * 0.85f);
    dl->AddRect(mn, mx, col, 2.0f, 0, 1.2f);
    for (int i = -1; i <= 1; i++) {
        float y = c.y + i * r * 0.55f;
        dl->AddRectFilled(ImVec2(mn.x - r * 0.16f, y - r * 0.09f), ImVec2(mn.x, y + r * 0.09f), col, 1.0f);
        dl->AddRectFilled(ImVec2(mx.x, y - r * 0.09f), ImVec2(mx.x + r * 0.16f, y + r * 0.09f), col, 1.0f);
    }
}

void IconContrast(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddCircle(c, r * 0.75f, col, 24, 1.3f);
    dl->PathArcTo(c, r * 0.75f, -1.5708f, 1.5708f, 16);
    dl->PathFillConvex(col);
}

void IconLuminosity(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddCircle(c, r * 0.42f, col, 20, 1.4f);
    for (int i = 0; i < 8; i++) {
        float a = (6.28318f / 8.0f) * (float)i;
        ImVec2 dir(cosf(a), sinf(a));
        dl->AddLine(ImVec2(c.x + dir.x * r * 0.62f, c.y + dir.y * r * 0.62f),
                    ImVec2(c.x + dir.x * r * 0.88f, c.y + dir.y * r * 0.88f), col, 1.4f);
    }
}

void IconTAA(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    // 3 cuadros superpuestos con alpha decreciente -- frames mezclandose.
    ImVec4 colV = ImGui::ColorConvertU32ToFloat4(col);
    for (int i = 2; i >= 0; i--) {
        float off = (float)i * r * 0.22f;
        ImU32 c2 = ImGui::ColorConvertFloat4ToU32(ImVec4(colV.x, colV.y, colV.z, 1.0f - (float)i * 0.3f));
        dl->AddRect(ImVec2(c.x - r * 0.5f + off, c.y - r * 0.5f - off),
                    ImVec2(c.x + r * 0.5f + off, c.y + r * 0.5f - off), c2, 2.0f, 0, 1.3f);
    }
}

using IconFn = void (*)(ImDrawList*, ImVec2, float, ImU32);

constexpr float kCardHeaderH = 64.0f;
constexpr float kCardDescH   = 34.0f;
constexpr float kCardSliderH = 40.0f;
constexpr float kCardModeH   = 34.0f;
constexpr float kCardPadding = 16.0f;

// Altura de una tarjeta ANTES de dibujarla -- hace falta calcularla por
// adelantado (sin depender del cursor de ImGui) para poder alinear una
// fila de 2 tarjetas de alturas distintas por su propio origen, en vez de
// dejar que ImGui::SameLine() infiera la altura de fila (ver el bug de
// grilla rota: mezclar SetCursorScreenPos manual con el layout automático
// de SameLine/Dummy hacía que la fila siguiente arrancara en una Y
// equivocada apenas una tarjeta era más alta que la otra -- exactamente lo
// que se veía roto en pantalla).
float ComputeCardHeight(bool hasSlider, bool enabled, bool hasMode = false) {
    float sliderH = (hasSlider && enabled) ? kCardSliderH : 0.0f;
    float modeH   = (hasMode && enabled)   ? kCardModeH   : 0.0f;
    return kCardHeaderH + kCardDescH + sliderH + modeH + kCardPadding;
}

// ─────────────────────────────────────────────────────────────────────────
//  Tarjeta de efecto: icono + titulo + descripcion + toggle + slider
//  opcional. Se dibuja en el origen 'origin' explícito (no en "donde sea
//  que este el cursor de ImGui"), para que el layout en grilla de arriba
//  tenga control total sobre la posición de cada una.
// ─────────────────────────────────────────────────────────────────────────
bool ShaderCard(ImVec2 origin, const char* id, IconFn icon, ImU32 accent,
                const char* title, const char* desc,
                bool* enabled, const char* sliderLabel,
                float* sliderVal, float sliderMin, float sliderMax, float cardW,
                bool recommended = false,
                int* modeVal = nullptr, const char* const* modeLabels = nullptr, int modeCount = 0)
{
    ImGui::PushID(id);
    bool changed = false;

    const bool  hasSlider = sliderLabel != nullptr;
    const bool  hasMode   = modeVal != nullptr && modeCount > 0;
    const float cardH     = ComputeCardHeight(hasSlider, *enabled, hasMode);

    ImVec2 p0 = origin;
    ImVec2 p1 = ImVec2(p0.x + cardW, p0.y + cardH);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec4 accentV = ImGui::ColorConvertU32ToFloat4(accent);
    ImU32 bgCol = *enabled
        ? ImGui::ColorConvertFloat4ToU32(ImVec4(accentV.x, accentV.y, accentV.z, 0.10f))
        : ImGui::ColorConvertFloat4ToU32(ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
    ImU32 borderCol = *enabled
        ? ImGui::ColorConvertFloat4ToU32(ImVec4(accentV.x, accentV.y, accentV.z, 0.65f))
        : DS::BtnDefaultBord;

    dl->AddRectFilled(p0, p1, bgCol, DS::RadiusLarge);
    dl->AddRect(p0, p1, borderCol, DS::RadiusLarge, 0, 1.3f);

    // Icono: insignia circular en el color de acento del efecto.
    ImVec2 iconCenter(p0.x + 34.0f, p0.y + 32.0f);
    dl->AddCircleFilled(iconCenter, 20.0f,
        ImGui::ColorConvertFloat4ToU32(ImVec4(accentV.x, accentV.y, accentV.z, *enabled ? 0.22f : 0.12f)));
    icon(dl, iconCenter, 11.0f, accent);

    // Titulo
    ImGui::SetCursorScreenPos(ImVec2(p0.x + 60.0f, p0.y + 14.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextPrimary));
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();

    // Estado (Activo/Inactivo) chico, debajo del titulo
    ImGui::SetCursorScreenPos(ImVec2(p0.x + 60.0f, p0.y + 34.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, *enabled ? accentV : ImGui::ColorConvertU32ToFloat4(DS::TextHint));
    ImGui::TextUnformatted(*enabled ? "Activo" : "Inactivo");
    ImGui::PopStyleColor();

    // Insignia "RECOMENDADO": entre el título y el toggle, con lugar de
    // sobra ya que los titulos de estas tarjetas son cortos.
    if (recommended) {
        ImGui::SetWindowFontScale(0.78f);
        const char* badgeText = "RECOMENDADO";
        ImVec2 textSz = ImGui::CalcTextSize(badgeText);
        ImGui::SetWindowFontScale(1.0f);

        float padX = 8.0f, padY = 3.0f;
        float badgeW = textSz.x + padX * 2.0f;
        float badgeH = textSz.y + padY * 2.0f;
        float badgeX = p1.x - 52.0f - badgeW;
        float badgeY = p0.y + 14.0f;

        ImU32 badgeBg = IM_COL32(80, 200, 130, 230);
        dl->AddRectFilled(ImVec2(badgeX, badgeY), ImVec2(badgeX + badgeW, badgeY + badgeH), badgeBg, badgeH * 0.5f);

        ImGui::SetWindowFontScale(0.78f);
        dl->AddText(ImVec2(badgeX + padX, badgeY + padY), IM_COL32(12, 30, 20, 255), badgeText);
        ImGui::SetWindowFontScale(1.0f);
    }

    // Toggle: circulo tipo switch arriba a la derecha
    float togR = 12.0f;
    ImVec2 togC(p1.x - 26.0f, p0.y + 26.0f);
    ImGui::SetCursorScreenPos(ImVec2(togC.x - togR, togC.y - togR));
    ImGui::InvisibleButton("##toggle", ImVec2(togR * 2.0f, togR * 2.0f));
    bool togHovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) {
        *enabled = !*enabled;
        changed  = true;
    }
    ImU32 togBg = *enabled ? accent : DS::BtnDefaultFill;
    dl->AddCircleFilled(togC, togR, togBg);
    dl->AddCircle(togC, togR, togHovered ? DS::AccentColorHov : DS::BtnDefaultBord, 20, 1.2f);
    if (*enabled) {
        dl->AddLine(ImVec2(togC.x - 4.5f, togC.y), ImVec2(togC.x - 1.0f, togC.y + 4.0f), IM_COL32(20, 20, 24, 255), 1.8f);
        dl->AddLine(ImVec2(togC.x - 1.0f, togC.y + 4.0f), ImVec2(togC.x + 5.5f, togC.y - 4.5f), IM_COL32(20, 20, 24, 255), 1.8f);
    }

    // Descripcion
    ImGui::SetCursorScreenPos(ImVec2(p0.x + 16.0f, p0.y + kCardHeaderH));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
    // PushTextWrapPos espera coordenadas LOCALES a la ventana (internamente
    // le suma window->Pos.x), no coordenadas de pantalla absolutas -- pasarle
    // p1.x directo (screen-space) duplicaba el offset de la ventana y
    // empujaba el limite de wrap muy lejos a la derecha, asi que el texto
    // nunca envolvia de verdad y quedaba cortado a la mitad de una palabra
    // por el clip rect de afuera. Se convierte a local antes de pasarlo.
    float wrapLocalX = (p1.x - 16.0f) - ImGui::GetWindowPos().x + ImGui::GetScrollX();
    ImGui::PushTextWrapPos(wrapLocalX);
    ImGui::TextWrapped("%s", desc);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();

    // Slider (solo si esta activo)
    if (hasSlider && *enabled) {
        ImGui::SetCursorScreenPos(ImVec2(p0.x + 16.0f, p0.y + kCardHeaderH + kCardDescH));
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
        ImGui::TextUnformatted(sliderLabel);
        ImGui::PopStyleColor();
        ImGui::SetCursorScreenPos(ImVec2(p0.x + 16.0f, p0.y + kCardHeaderH + kCardDescH + 16.0f));
        if (DS::ModernSlider("##val", sliderVal, sliderMin, sliderMax, cardW - 32.0f, accent)) {
            changed = true;
        }
    }

    // Selector de modo (3 botones, ej. Cine: rojo/verde/azul) -- debajo del
    // slider si hay uno, si no debajo de la descripcion.
    if (hasMode && *enabled) {
        float modeY = p0.y + kCardHeaderH + kCardDescH + (hasSlider ? kCardSliderH : 0.0f) + 6.0f;
        ImGui::SetCursorScreenPos(ImVec2(p0.x + 16.0f, modeY));

        const float modeW  = cardW - 32.0f;
        const float gap    = 4.0f;
        const float segW   = (modeW - gap * (modeCount - 1)) / (float)modeCount;

        for (int m = 0; m < modeCount; m++) {
            if (m > 0) ImGui::SameLine(0.0f, gap);
            bool active = (*modeVal == m);

            ImGui::PushStyleColor(ImGuiCol_Button,
                active ? ImVec4(accentV.x * 0.35f, accentV.y * 0.35f, accentV.z * 0.55f, 1.0f)
                       : ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
            ImGui::PushStyleColor(ImGuiCol_Text, active ? accentV : ImGui::ColorConvertU32ToFloat4(DS::TextHint));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

            std::string btnId = std::string(modeLabels[m]) + "##mode" + std::to_string(m);
            if (ImGui::Button(btnId.c_str(), ImVec2(segW, 26.0f))) {
                *modeVal = m;
                changed  = true;
            }

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
        }
    }

    ImGui::PopID();
    return changed;
}

} // namespace

void ShadersPanel::RenderContent() {
    auto& settingsMgr = Settings::SettingsManager::Get();
    auto& p           = settingsMgr.GetSettings().projection;
    auto& core        = Core::PresentationCore::Get();
    bool  changed     = false;

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.68f, 0.78f, 1.0f));
    ImGui::TextWrapped("Efectos de post-proceso sobre la salida en vivo (Audiencia). Los cambios se aplican al instante.");
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, 12.0f));

    const float gap    = 12.0f;
    const float availW = ImGui::GetContentRegionAvail().x;
    const float cardW  = (availW - gap) * 0.5f;

    struct Effect {
        const char* id;
        IconFn      icon;
        ImU32       accent;
        const char* title;
        const char* desc;
        bool*       enabled;
        const char* sliderLabel;
        float*      sliderVal;
        float       sliderMin, sliderMax;
        std::function<void(bool)>  onToggle;
        std::function<void(float)> onSlide;
        bool        recommended = false;

        // Selector de modo opcional (ej. Cine: rojo/verde/azul) -- 3
        // botones debajo del slider, ver ShaderCard.
        int*                       modeVal   = nullptr;
        const char* const*        modeLabels = nullptr;
        int                        modeCount = 0;
        std::function<void(int)>  onModeChange;
    };

    // NIS (escalador exclusivo de NVIDIA) solo se ofrece con GPU NVIDIA real
    // detectada (ver SystemStats::IsNvidiaGpu) -- en cualquier otra placa
    // no corre nada utilizable, asi que ni se muestra la tarjeta. FSR y NIS
    // son mutuamente excluyentes (ver PresentationCore::SetFSREnabled/
    // SetNISEnabled): activar uno apaga el otro, por eso comparten "peine"
    // de vector en vez de ser dos entradas de un array fijo.
    const bool hasNvidiaGpu = Core::SystemStats::Get().IsNvidiaGpu();

    std::vector<Effect> effects;
    effects.push_back(
        { "fsr", IconUpscale, IM_COL32(90, 170, 245, 255), "FSR 1.0", "Reescala y afila video de baja resolución (AMD, funciona en cualquier GPU).",
          &p.fsrEnabled, "Nitidez", &p.fsrSharpness, 0.0f, 2.0f,
          [&](bool v){ core.SetFSREnabled(v); }, [&](float v){ core.SetFSRSharpness(v); } });
    if (hasNvidiaGpu) {
        effects.push_back(
            { "nis", IconUpscale, IM_COL32(118, 185, 0, 255), "NIS (NVIDIA)", "Escalador de NVIDIA Image Scaling -- alternativa a FSR, exclusiva de placas NVIDIA.",
              &p.nisEnabled, "Nitidez", &p.nisSharpness, 0.0f, 1.0f,
              [&](bool v){ core.SetNISEnabled(v); }, [&](float v){ core.SetNISSharpness(v); } });
    }
    effects.push_back(
        { "crt", IconCRT, IM_COL32(120, 220, 150, 255), "Modo CRT", "Scanlines y curvatura estilo tubo antiguo.",
          &p.crtEnabled, "Intensidad de scanlines", &p.crtScanlineIntensity, 0.0f, 1.0f,
          [&](bool v){ core.SetCRTEnabled(v); }, [&](float v){ core.SetCRTScanlineIntensity(v); } });
    effects.push_back(
        { "grain", IconGrain, IM_COL32(230, 190, 90, 255), "Grano de pelicula", "Ruido animado tipo acabado fotografico.",
          &p.grainEnabled, "Intensidad", &p.grainIntensity, 0.0f, 1.0f,
          [&](bool v){ core.SetGrainEnabled(v); }, [&](float v){ core.SetGrainIntensity(v); } });
    effects.push_back(
        { "saturation", IconSaturation, IM_COL32(235, 110, 165, 255), "Saturación", "Colores mas vivos o desaturados hasta blanco y negro.",
          &p.saturationEnabled, "Cantidad", &p.saturationAmount, 0.0f, 2.0f,
          [&](bool v){ core.SetSaturationEnabled(v); }, [&](float v){ core.SetSaturationAmount(v); } });
    effects.push_back(
        { "vignette", IconVignette, IM_COL32(180, 140, 235, 255), "Vinetado", "Oscurece los bordes para enfocar el centro.",
          &p.vignetteEnabled, "Intensidad", &p.vignetteIntensity, 0.0f, 1.0f,
          [&](bool v){ core.SetVignetteEnabled(v); }, [&](float v){ core.SetVignetteIntensity(v); } });
    effects.push_back(
        { "fxaa", IconFXAA, IM_COL32(120, 190, 230, 255), "FXAA", "Suaviza bordes dentados (antialiasing).",
          &p.fxaaEnabled, nullptr, nullptr, 0.0f, 0.0f,
          [&](bool v){ core.SetFXAAEnabled(v); }, nullptr });
    effects.push_back(
        { "blur", IconBlur, IM_COL32(150, 170, 235, 255), "Blur", "Desenfoque gaussiano suave de toda la imagen.",
          &p.blurEnabled, "Intensidad", &p.blurIntensity, 0.0f, 1.0f,
          [&](bool v){ core.SetBlurEnabled(v); }, [&](float v){ core.SetBlurIntensity(v); } });
    effects.push_back(
        { "sharpen", IconSharpen, IM_COL32(235, 160, 90, 255), "Sharpen", "Realza bordes y detalle -- lo opuesto al blur.",
          &p.sharpenEnabled, "Intensidad", &p.sharpenIntensity, 0.0f, 1.0f,
          [&](bool v){ core.SetSharpenEnabled(v); }, [&](float v){ core.SetSharpenIntensity(v); } });
    effects.push_back(
        { "bloom", IconBloom, IM_COL32(255, 220, 120, 255), "Bloom", "Resplandor que se derrama desde las zonas mas brillantes.",
          &p.bloomEnabled, "Intensidad", &p.bloomIntensity, 0.0f, 1.0f,
          [&](bool v){ core.SetBloomEnabled(v); }, [&](float v){ core.SetBloomIntensity(v); } });
    effects.push_back(
        { "chromaticaberration", IconChromaticAberration, IM_COL32(235, 100, 200, 255), "Aberración cromática", "Desfase de color RGB en los bordes, efecto retro/cinematografico.",
          &p.chromaticAberrationEnabled, "Intensidad", &p.chromaticAberrationIntensity, 0.0f, 1.0f,
          [&](bool v){ core.SetChromaticAberrationEnabled(v); }, [&](float v){ core.SetChromaticAberrationIntensity(v); } });
    effects.push_back(
        { "contrast", IconContrast, IM_COL32(150, 150, 160, 255), "Contraste", "Mas diferencia entre sombras y luces.",
          &p.contrastEnabled, "Cantidad", &p.contrastAmount, 0.0f, 2.0f,
          [&](bool v){ core.SetContrastEnabled(v); }, [&](float v){ core.SetContrastAmount(v); } });
    effects.push_back(
        { "luminosity", IconLuminosity, IM_COL32(255, 230, 140, 255), "Luminosidad", "Brillo general de la imagen.",
          &p.luminosityEnabled, "Cantidad", &p.luminosityAmount, 0.0f, 2.0f,
          [&](bool v){ core.SetLuminosityEnabled(v); }, [&](float v){ core.SetLuminosityAmount(v); } });

    static const char* kCineTintLabels[] = { "Rojo", "Verde", "Azul" };
    effects.push_back(
        { "cine", IconCine, IM_COL32(235, 200, 90, 255), "Cine", "Curva de contraste filmica + tinte hacia un color, como una gradacion de cine.",
          &p.cineEnabled, "Intensidad", &p.cineIntensity, 0.0f, 1.0f,
          [&](bool v){ core.SetCineEnabled(v); }, [&](float v){ core.SetCineIntensity(v); },
          /*recommended=*/false, &p.cineTint, kCineTintLabels, 3,
          [&](int m){ core.SetCineTint(m); } });
    effects.push_back(
        { "vhs", IconVHS, IM_COL32(200, 90, 220, 255), "VHS", "Cinta de video vieja: sangrado de color, scanlines, tracking y bamboleo.",
          &p.vhsEnabled, "Intensidad", &p.vhsIntensity, 0.0f, 1.0f,
          [&](bool v){ core.SetVHSEnabled(v); }, [&](float v){ core.SetVHSIntensity(v); } });
    effects.push_back(
        { "taa", IconTAA, IM_COL32(120, 200, 235, 255), "TAA (antialiasing temporal)", "Mezcla con el frame anterior para suavizar bordes -- de paso agrega algo de desenfoque de movimiento.",
          &p.taaEnabled, "Intensidad", &p.taaIntensity, 0.0f, 1.0f,
          [&](bool v){ core.SetTAAEnabled(v); }, [&](float v){ core.SetTAAIntensity(v); } });
    effects.push_back(
        { "fillblur", IconFill, IM_COL32(90, 210, 190, 255), "Rellenado", "Llena las barras negras con el mismo fondo, estirado y muy desenfocado.",
          &p.fillBlurEnabled, "Brillo", &p.fillBlurBrightness, 0.0f, 1.0f,
          [&](bool v){ core.SetFillBlurEnabled(v); }, [&](float v){ core.SetFillBlurBrightness(v); },
          /*recommended=*/true });

    // Grilla de 2 columnas con posicionamiento explícito por fila: cada
    // tarjeta puede tener una altura distinta (el slider solo se muestra si
    // el efecto esta activo), asi que la fila avanza segun la MAS ALTA de
    // las dos, no segun el layout automático de ImGui (que fue justamente
    // lo que rompia la grilla antes).
    const int   count    = (int)effects.size();
    const float originX  = ImGui::GetCursorScreenPos().x;
    float       cursorY  = ImGui::GetCursorScreenPos().y;

    for (int i = 0; i < count; i += 2) {
        Effect& left  = effects[i];
        Effect* right = (i + 1 < count) ? &effects[i + 1] : nullptr;

        float leftH  = ComputeCardHeight(left.sliderLabel != nullptr, *left.enabled, left.modeVal != nullptr);
        float rightH = right ? ComputeCardHeight(right->sliderLabel != nullptr, *right->enabled, right->modeVal != nullptr) : 0.0f;
        float rowH   = std::max(leftH, rightH);

        if (ShaderCard(ImVec2(originX, cursorY), left.id, left.icon, left.accent, left.title, left.desc,
                       left.enabled, left.sliderLabel, left.sliderVal, left.sliderMin, left.sliderMax, cardW,
                       left.recommended, left.modeVal, left.modeLabels, left.modeCount)) {
            left.onToggle(*left.enabled);
            if (left.onSlide) left.onSlide(*left.sliderVal);
            if (left.onModeChange && left.modeVal) left.onModeChange(*left.modeVal);
            changed = true;
        }

        if (right) {
            if (ShaderCard(ImVec2(originX + cardW + gap, cursorY), right->id, right->icon, right->accent,
                           right->title, right->desc, right->enabled, right->sliderLabel, right->sliderVal,
                           right->sliderMin, right->sliderMax, cardW, right->recommended,
                           right->modeVal, right->modeLabels, right->modeCount)) {
                right->onToggle(*right->enabled);
                if (right->onSlide) right->onSlide(*right->sliderVal);
                if (right->onModeChange && right->modeVal) right->onModeChange(*right->modeVal);
                changed = true;
            }
        }

        cursorY += rowH + gap;
    }

    // Registra el alto real de la grilla para que el scroll/tamaño del
    // contenido del panel lo tenga en cuenta (los rectangulos de fondo de
    // las tarjetas se dibujan por ImDrawList directo, que ImGui no cuenta
    // solo mirando los widgets internos).
    ImGui::SetCursorScreenPos(ImVec2(originX, cursorY));
    ImGui::Dummy(ImVec2(availW, 0.0f));

    if (changed) settingsMgr.Save();
}

} // namespace ProyecThor::UI
