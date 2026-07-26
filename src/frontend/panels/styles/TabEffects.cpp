#include "TabEffects.h"
#include "DesignSystem.h"
#include <imgui.h>

namespace ProyecThor::UI {

void TabEffects::RenderEffectCard(const char* title, const ImVec4& accent, float colWidth,
                                   bool& enabled, float* color4,
                                   float* intensity, const char* intensityLabel)
{
    ImGui::PushID(title);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    float  cardH = 40.0f + (enabled ? ((color4 ? 34.0f : 0.0f) + (intensity ? 34.0f : 0.0f)) : 0.0f);
    ImVec2 p1 = { p0.x + colWidth, p0.y + cardH };

    ImU32 bg = enabled
        ? CanvaPalette::ToU32(ImVec4(accent.x, accent.y, accent.z, 0.12f))
        : CanvaPalette::ToU32(CanvaPalette::Surface1);
    ImU32 border = enabled ? CanvaPalette::ToU32(accent) : CanvaPalette::ToU32(CanvaPalette::Border);

    dl->AddRectFilled(p0, p1, bg, 5.0f);
    dl->AddRect(p0, p1, border, 5.0f, 0, enabled ? 1.4f : 1.0f);

    // Titulo
    ImGui::SetCursorScreenPos({ p0.x + 12.0f, p0.y + 10.0f });
    ImGui::PushStyleColor(ImGuiCol_Text, enabled ? accent : CanvaPalette::TextMuted);
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();

    // Toggle: checkbox chico arriba a la derecha de la tarjeta
    ImGui::SetCursorScreenPos({ p1.x - 34.0f, p0.y + 8.0f });
    ImGui::PushStyleColor(ImGuiCol_CheckMark,      accent);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        CanvaPalette::Surface1);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, CanvaPalette::Surface2);
    ImGui::Checkbox("##enabled", &enabled);
    ImGui::PopStyleColor(3);

    float y = p0.y + 36.0f;

    if (enabled && color4) {
        ImGui::SetCursorScreenPos({ p0.x + 12.0f, y });
        ImGui::SetNextItemWidth(colWidth - 24.0f);
        ImGui::ColorEdit4("##color", color4,
            ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs);
        y += 34.0f;
    }

    if (enabled && intensity) {
        ImGui::SetCursorScreenPos({ p0.x + 12.0f, y });
        ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
        ImGui::TextUnformatted(intensityLabel);
        ImGui::PopStyleColor();
        ImGui::SetCursorScreenPos({ p0.x + 12.0f, y + 16.0f });
        DS::ModernSlider("##intensity", intensity, 0.0f, 1.0f, colWidth - 24.0f, CanvaPalette::ToU32(accent));
    }

    // Los widgets de arriba (Checkbox/ColorEdit4/slider) se dibujaron en
    // posiciones absolutas via SetCursorScreenPos, asi que el cursor
    // "logico" de auto-layout quedo en cualquier lado -- hay que resetearlo
    // a p0 y recien ahi someter un Dummy() del tamaño real de la tarjeta
    // para que ImGui registre el limite correcto (ver aviso de Dear ImGui:
    // "SetCursorPos sin un item despues no hace crecer la ventana/parent").
    ImGui::SetCursorScreenPos(p0);
    ImGui::Dummy({ colWidth, cardH + 8.0f });
    ImGui::PopID();
}

void TabEffects::Render(StyleData& data, float colWidth) {
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    CanvaStyleEditor::Badge("EFECTOS DE TEXTO", CanvaPalette::Accent);
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    CanvaStyleEditor::SectionLabel("Capas dibujadas sobre las letras, de atras hacia adelante.");
    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    auto& fx = data.effects;

    // Las 7 tarjetas usan el mismo acento -- antes cada una tenia su propio
    // hue (Gold/TextMuted/Red/Green/cyan/Pink), look "confeti" que no
    // calzaba con el resto de la app; el titulo ya distingue cada efecto.
    RenderEffectCard("Fondo", CanvaPalette::Accent, colWidth,
                      fx.bgEnabled, fx.bgColor, nullptr, nullptr);

    RenderEffectCard("Borde", CanvaPalette::Accent, colWidth,
                      fx.borderEnabled, fx.borderColor, &fx.borderWidth, "Grosor");

    RenderEffectCard("Sombra", CanvaPalette::Accent, colWidth,
                      fx.shadowEnabled, fx.shadowColor, &fx.shadowIntensity, "Distancia");

    RenderEffectCard("Aberracion cromatica", CanvaPalette::Accent, colWidth,
                      fx.chromaticAberrationEnabled, nullptr,
                      &fx.chromaticAberrationIntensity, "Intensidad");

    RenderEffectCard("Glow (bloom)", CanvaPalette::Accent, colWidth,
                      fx.glowEnabled, fx.glowColor, &fx.glowIntensity, "Intensidad");

    RenderEffectCard("Neon", CanvaPalette::Accent, colWidth,
                      fx.neonEnabled, fx.neonColor, &fx.neonIntensity, "Intensidad");

    RenderEffectCard("Subrayado", CanvaPalette::Accent, colWidth,
                      fx.underlineEnabled, fx.underlineColor, &fx.underlineThickness, "Grosor");
}

} // namespace ProyecThor::UI
