#pragma once
#include <imgui.h>

// ─────────────────────────────────────────────────────────────────────────────
//  LayersTheme — paleta de colores y widgets compartidos por todos los tabs
//  del panel de capas.  Incluir en cada .cpp que necesite dibujar UI.
// ─────────────────────────────────────────────────────────────────────────────

namespace ProyecThor::UI {

// ── Paleta ────────────────────────────────────────────────────────────────────
struct LP {  // "Layers Palette"
    // Surfaces
    static constexpr ImVec4 Base        = {0.07f, 0.08f, 0.10f, 1.0f};
    static constexpr ImVec4 Surface0    = {0.09f, 0.10f, 0.13f, 1.0f};
    static constexpr ImVec4 Surface1    = {0.12f, 0.13f, 0.17f, 1.0f};
    static constexpr ImVec4 Surface2    = {0.16f, 0.17f, 0.22f, 1.0f};
    static constexpr ImVec4 Surface3    = {0.20f, 0.21f, 0.28f, 1.0f};

    // Borders
    static constexpr ImVec4 Border      = {0.22f, 0.24f, 0.32f, 0.6f};
    static constexpr ImVec4 BorderHov   = {0.35f, 0.38f, 0.55f, 0.8f};

    // Accent — electric violet-blue
    static constexpr ImVec4 Accent      = {0.42f, 0.48f, 1.00f, 1.0f};
    static constexpr ImVec4 AccentHov   = {0.52f, 0.58f, 1.00f, 1.0f};
    static constexpr ImVec4 AccentDim   = {0.42f, 0.48f, 1.00f, 0.18f};
    static constexpr ImVec4 AccentActive= {0.32f, 0.38f, 0.90f, 1.0f};

    // Semantic
    static constexpr ImVec4 Gold        = {0.95f, 0.75f, 0.20f, 1.0f};
    static constexpr ImVec4 GoldDim     = {0.95f, 0.75f, 0.20f, 0.15f};
    static constexpr ImVec4 Green       = {0.30f, 0.85f, 0.55f, 1.0f};
    static constexpr ImVec4 GreenDim    = {0.30f, 0.85f, 0.55f, 0.15f};
    static constexpr ImVec4 Red         = {0.95f, 0.35f, 0.35f, 1.0f};
    static constexpr ImVec4 RedDim      = {0.95f, 0.35f, 0.35f, 0.15f};

    // Text
    static constexpr ImVec4 Text        = {0.92f, 0.93f, 0.96f, 1.0f};
    static constexpr ImVec4 TextSub     = {0.65f, 0.67f, 0.75f, 1.0f};
    static constexpr ImVec4 TextMuted   = {0.42f, 0.44f, 0.52f, 1.0f};

    // Tab bar sticky
    static constexpr ImVec4 TabBar      = {0.08f, 0.09f, 0.12f, 1.0f};
    static constexpr ImVec4 TabActive   = {0.12f, 0.13f, 0.17f, 1.0f};
    static constexpr ImVec4 TabInactive = {0.09f, 0.10f, 0.13f, 1.0f};
};

// ── Conversion helpers ─────────────────────────────────────────────────────
inline ImU32 LPU32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }

// ── Shared button widgets ──────────────────────────────────────────────────

inline bool LPPrimaryBtn(const char* label, ImVec2 size = {0, 0}) {
    ImGui::PushStyleColor(ImGuiCol_Button,        LP::Accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, LP::AccentHov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  LP::AccentActive);
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1, 1, 1, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(12.0f, 7.0f));
    bool r = ImGui::Button(label, size);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    return r;
}

inline bool LPGhostBtn(const char* label, ImVec2 size = {0, 0}) {
    ImGui::PushStyleColor(ImGuiCol_Button,        LP::Surface2);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, LP::Surface3);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.28f, 0.30f, 0.40f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          LP::Text);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(12.0f, 7.0f));
    bool r = ImGui::Button(label, size);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    return r;
}

inline bool LPIconToggle(const char* id, bool active, ImVec2 size = {26, 22}) {
    ImVec4 bg  = active ? ImVec4(0.18f, 0.20f, 0.35f, 1.0f) : LP::Surface1;
    ImVec4 col = active ? LP::Accent : LP::TextMuted;
    ImGui::PushStyleColor(ImGuiCol_Button,        bg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, LP::Surface2);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  LP::Surface3);
    ImGui::PushStyleColor(ImGuiCol_Text,          col);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(3.0f, 3.0f));
    bool r = ImGui::Button(id, size);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    return r;
}

// Dibuja el icono de cuadricula dentro del ultimo boton renderizado
inline void DrawGridIcon(bool active) {
    ImVec2 min = ImGui::GetItemRectMin();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 ic = active ? LPU32(LP::Accent) : LPU32(LP::TextMuted);
    float bx = min.x + 5.0f, by = min.y + 4.0f;
    float cs = 4.5f, gap = 2.5f;
    for (int r = 0; r < 2; r++)
        for (int c = 0; c < 2; c++) {
            float rx = bx + c * (cs + gap);
            float ry = by + r * (cs + gap);
            dl->AddRectFilled({rx, ry}, {rx + cs, ry + cs}, ic, 1.5f);
        }
}

// Dibuja el icono de lista dentro del ultimo boton renderizado
inline void DrawListIcon(bool active) {
    ImVec2 min = ImGui::GetItemRectMin();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 ic = active ? LPU32(LP::Accent) : LPU32(LP::TextMuted);
    float bx = min.x + 4.0f, by = min.y + 5.0f;
    for (int i = 0; i < 3; i++) {
        float ry = by + i * 4.2f;
        dl->AddRectFilled({bx, ry}, {bx + 17.0f, ry + 2.3f}, ic, 1.0f);
    }
}

// ── Decoracion: linea separadora degradada ─────────────────────────────────
inline void LPSeparatorLine() {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float  w = ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
        p, {p.x + w, p.y + 1},
        LPU32({0,0,0,0}),
        LPU32(LP::Accent),
        LPU32({LP::Accent.x, LP::Accent.y, LP::Accent.z, 0.3f}),
        LPU32({0,0,0,0}));
    ImGui::Dummy({0, 5});
}

// ── Badge de texto pequeño ─────────────────────────────────────────────────
inline void LPBadge(ImDrawList* dl, ImVec2 pos, const char* text,
                    ImVec4 bgColor, ImVec4 fgColor,
                    float padX = 5.0f, float padY = 2.0f) {
    ImVec2 ts = ImGui::CalcTextSize(text);
    dl->AddRectFilled(
        {pos.x - padX, pos.y - padY},
        {pos.x + ts.x + padX, pos.y + ts.y + padY},
        LPU32(bgColor), 5.0f);
    dl->AddText(pos, LPU32(fgColor), text);
}

} // namespace ProyecThor::UI
