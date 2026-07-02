#include "SettingsPanel.h"
#include "SettingsManager.h"
#include "backend/core/PresentationCore.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <vector>
#include <string>
#include <cmath>

namespace ProyecThor::UI::Settings {

// ─────────────────────────────────────────────────────────────────────────────
//  Definición de categorías
// ─────────────────────────────────────────────────────────────────────────────

struct Category {
    const char* tag;
    const char* label;
    const char* description;
};

static const Category k_Categories[] = {
    { "UI",  "Apariencia",      "Colores, fuentes y efectos visuales"     },
    { "GEN", "General",         "Inicio, guardado y carpetas"             },
    { "PRY", "Proyección",      "Monitor, texto y márgenes"               },
    { "SOU", "Audio",           "Volumen, dispositivo y fade"             },
    { "LNG", "Idioma",          "Idioma de la interfaz"                   },
    { "UPD", "Actualizaciones", "Versión instalada y canales"             },
};
static constexpr int k_CategoryCount = 6;

// Paleta "Liquid Glass": Tonos cyan, cobalto y neón con alta saturación
static const ImVec4 k_AccentColors[] = {
    ImVec4(0.00f, 0.75f, 1.00f, 1.0f), // UI    — Cyan Neón
    ImVec4(0.10f, 0.50f, 0.95f, 1.0f), // GEN   — Azul Cobalto
    ImVec4(0.30f, 0.40f, 1.00f, 1.0f), // PRY   — Azul Violeta
    ImVec4(0.00f, 0.85f, 0.85f, 1.0f), // SOU   — Turquesa Brillante
    ImVec4(0.20f, 0.65f, 0.90f, 1.0f), // LNG   — Celeste
    ImVec4(0.40f, 0.20f, 0.90f, 1.0f), // UPD   — Púrpura Profundo
};

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor
// ─────────────────────────────────────────────────────────────────────────────

SettingsPanel::SettingsPanel()
    : m_SelectedCategory(0), m_PrevCategory(-1),
      m_SaveTimer(0.0f), m_CheckingAnim(0.0f) {}

// ─────────────────────────────────────────────────────────────────────────────
//  Render principal
// ─────────────────────────────────────────────────────────────────────────────

void SettingsPanel::Render(bool* isOpen) {
    if (!*isOpen) return;

    const float dt = ImGui::GetIO().DeltaTime;
    m_CheckingAnim += dt * 280.0f; 

    ImGui::SetNextWindowSize(ImVec2(900, 650), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(720, 500), ImVec2(FLT_MAX, FLT_MAX));

    // Estilo Liquid Glass: Fondos azules oscuros semi-transparentes y bordes curvos
ImGui::PushStyleColor(ImGuiCol_WindowBg,      ImVec4(0.05f, 0.05f, 0.06f, 0.95f)); // Fondo gris-plomo casi negro
ImGui::PushStyleColor(ImGuiCol_TitleBg,       ImVec4(0.04f, 0.04f, 0.05f, 0.98f)); // Título más oscuro
ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.06f, 0.06f, 0.07f, 0.98f)); // Activo apenas más claro
ImGui::PushStyleColor(ImGuiCol_Border,        ImVec4(0.15f, 0.15f, 0.18f, 0.60f)); // Borde plomo sutil con leve brillo
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f); // Muy curvo
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);

    bool open = ImGui::Begin("Preferencias", isOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(4);

    if (!open) { ImGui::End(); return; }

    ImVec2 avail    = ImGui::GetContentRegionAvail();
    float  sideW    = 240.0f;
    float  footerH  = 70.0f;
    float  contentW = avail.x - sideW;
    float  contentH = avail.y - footerH;

    // ── Sidebar ───────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.03f, 0.08f, 0.18f, 0.40f)); // Más transparente
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 25.0f));
    ImGui::BeginChild("##sidebar", ImVec2(sideW, contentH), false, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();
    RenderSidebar();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Línea separadora suave (efecto cristal)
    ImDrawList* winDl = ImGui::GetWindowDrawList();
    ImVec2 sep = ImGui::GetCursorScreenPos();
    winDl->AddLine(ImVec2(sep.x, sep.y), ImVec2(sep.x, sep.y + contentH), IM_COL32(20, 60, 120, 100), 1.5f);

    ImGui::SameLine(0, 0);

    // ── Área de contenido ─────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.01f, 0.02f, 0.06f, 0.60f)); // Contraste profundo
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(45.0f, 40.0f));
    ImGui::BeginChild("##content", ImVec2(contentW, contentH),
                      ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PopStyleVar();
    RenderContent();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ── Footer ────────────────────────────────────────────────────────────────
    RenderSaveBar();

    ImGui::End();

    if (m_SaveTimer > 0.0f) {
        m_SaveTimer -= dt;
        if (m_SaveTimer <= 0.0f) m_SaveStatusMsg = "";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Sidebar
// ─────────────────────────────────────────────────────────────────────────────

void SettingsPanel::RenderSidebar() {
    auto& mgr = ProyecThor::Settings::SettingsManager::Get();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::SetCursorPosX(25.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.95f, 1.00f, 1.0f)); // Blanco azulado
    ImGui::SetWindowFontScale(1.2f);
    ImGui::TextUnformatted("ProyecThor");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::SetCursorPosX(25.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.60f, 0.80f, 1.0f));
    ImGui::Text("Versión %s", mgr.GetSettings().updates.currentVersion.c_str());
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 30.0f));

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 6.0f));

    for (int i = 0; i < k_CategoryCount; i++) {
        bool selected = (m_SelectedCategory == i);
        ImVec4 accent = k_AccentColors[i];
        float itemH   = 45.0f;
        float itemW   = ImGui::GetContentRegionAvail().x - 20.0f; // Margen derecho
        ImVec2 p = ImGui::GetCursorScreenPos();
        p.x += 10.0f; // Margen izquierdo

        ImGui::SetCursorPosX(10.0f);
        
        // Quitar el color de fondo estándar de ImGui para dibujarlo nosotros con curvas
        ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImVec4(0,0,0,0));
        
        char selectId[32];
        snprintf(selectId, sizeof(selectId), "##nav%d", i);
        
        bool hovered = false;
        bool clicked = ImGui::Selectable(selectId, selected, ImGuiSelectableFlags_None, ImVec2(itemW, itemH));
        hovered = ImGui::IsItemHovered();
        
        if (clicked) m_SelectedCategory = i;

        // Fondo tipo píldora (Pill shape) para selecciones
        if (selected) {
            // Degradado suave para el fondo seleccionado
            ImU32 colLeft = ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, 0.3f));
            ImU32 colRight = ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, 0.0f));
            dl->AddRectFilledMultiColor(p, ImVec2(p.x + itemW, p.y + itemH), colLeft, colRight, colRight, colLeft);
            
            // Barra indicadora brillante a la izquierda con bordes curvos
            dl->AddRectFilled(ImVec2(p.x, p.y + 8.0f), ImVec2(p.x + 4.0f, p.y + itemH - 8.0f), 
                              ImGui::ColorConvertFloat4ToU32(accent), 2.0f);
        } else if (hovered) {
            dl->AddRectFilled(p, ImVec2(p.x + itemW, p.y + itemH), IM_COL32(40, 80, 150, 40), 8.0f);
        }

        float labelY = p.y + (itemH - ImGui::GetTextLineHeight()) * 0.5f;
        
        // Etiqueta
        ImU32 labelCol = selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(140, 170, 210, 255);
        dl->AddText(ImVec2(p.x + 25.0f, labelY), labelCol, k_Categories[i].label);

        ImGui::PopStyleColor(3);
    }

    ImGui::PopStyleVar();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Área de contenido
// ─────────────────────────────────────────────────────────────────────────────

void SettingsPanel::RenderContent() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec4 accent = k_AccentColors[m_SelectedCategory];

    // Título de la sección
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.95f, 1.0f, 1.0f));
    ImGui::SetWindowFontScale(1.6f);
    ImGui::TextUnformatted(k_Categories[m_SelectedCategory].label);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 2.0f));

    // Subtítulo con azul claro apagado
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.65f, 0.85f, 1.0f));
    ImGui::TextUnformatted(k_Categories[m_SelectedCategory].description);
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 20.0f));

    // Separador líquido (Degradado que se desvanece)
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    ImU32 sepAccent = ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, 0.6f));
    ImU32 sepFade = ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, 0.0f));
    dl->AddRectFilledMultiColor(ImVec2(p.x, p.y), ImVec2(p.x + w, p.y + 1.5f), sepAccent, sepFade, sepFade, sepAccent);
    
    ImGui::Dummy(ImVec2(0.0f, 25.0f));

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(15.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f); // Campos de texto suaves

    switch (m_SelectedCategory) {
        case 0: RenderCategoryTheme();      break;
        case 1: RenderCategoryGeneral();    break;
        case 2: RenderCategoryProjection(); break;
        case 3: RenderCategoryAudio();      break;
        case 4: RenderCategoryLanguage();   break;
        case 5: RenderCategoryUpdates();    break;
        default: ImGui::TextDisabled("Categoría no implementada."); break;
    }

    ImGui::PopStyleVar(2);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Footer / barra de guardado
// ─────────────────────────────────────────────────────────────────────────────

void SettingsPanel::RenderSaveBar() {
    ImVec2 winPos  = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();
    float  barH    = 70.0f;
    float  barY    = winPos.y + winSize.y - barH;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(
        ImVec2(winPos.x, barY),
        ImVec2(winPos.x + winSize.x, winPos.y + winSize.y),
        IM_COL32(5, 15, 35, 230),
        14.0f,
        ImDrawFlags_RoundCornersBottom);

    dl->AddLine(
        ImVec2(winPos.x, barY),
        ImVec2(winPos.x + winSize.x, barY),
        IM_COL32(30, 80, 160, 150),
        1.5f);

    float btnY   = winSize.y - barH + (barH - 36.0f) * 0.5f;
    float rightX = winSize.x - 30.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 18.0f);

    // Boton Guardar
    ImGui::SetCursorPos(ImVec2(rightX - 160.0f, btnY));
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.00f, 0.45f, 0.90f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.10f, 0.55f, 1.00f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.00f, 0.35f, 0.80f, 0.9f));

    if (ImGui::Button("Guardar ajustes", ImVec2(160.0f, 36.0f))) {
        auto& mgr = ProyecThor::Settings::SettingsManager::Get();
        mgr.SaveSettings();
        mgr.ApplyTheme();
        mgr.ApplyProjection();
        m_SaveStatusMsg = "Cambios guardados";
        m_SaveTimer     = 3.0f;
    }

    ImGui::PopStyleColor(3);

    // Boton Restablecer
    ImGui::SetCursorPos(ImVec2(rightX - 160.0f - 15.0f - 130.0f, btnY));
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.10f, 0.15f, 0.25f, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.25f, 0.40f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.05f, 0.10f, 0.20f, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.60f, 0.75f, 0.90f, 1.0f));

    if (ImGui::Button("Restablecer", ImVec2(130.0f, 36.0f))) {
        ProyecThor::Settings::SettingsManager::Get().ResetToDefaults();
        m_SaveStatusMsg = "Ajustes restablecidos";
        m_SaveTimer     = 3.0f;
    }

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();

    if (!m_SaveStatusMsg.empty()) {
        float alpha = std::min(1.0f, m_SaveTimer);
        float textY = winSize.y - barH + (barH - ImGui::GetTextLineHeight()) * 0.5f;
        const float sideW = 240.0f;

        ImGui::SetCursorPos(ImVec2(sideW + 45.0f, textY));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.85f, 0.5f, alpha));
        ImGui::TextUnformatted(m_SaveStatusMsg.c_str());
        ImGui::PopStyleColor();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers Animados y Visuales (Glassy effect)
// ─────────────────────────────────────────────────────────────────────────────

void SettingsPanel::AnimatedProgressBar(float fraction, ImVec2 size, ImVec4 col) {
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetCursorScreenPos();
    float       t   = (float)ImGui::GetTime();

    // Fondo barra tipo tubo de cristal
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(10, 20, 40, 200), size.y * 0.5f);
    dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(40, 90, 160, 100), size.y * 0.5f, 0, 1.5f);

    if (fraction > 0.0f) {
        float fillW = fraction * size.x;
        if (fillW < size.y) fillW = size.y; // Evitar artefactos gráficos

        // Degradado vertical para simular un cilindro líquido
        ImU32 colTop = ImGui::ColorConvertFloat4ToU32(ImVec4(col.x + 0.2f, col.y + 0.2f, col.z + 0.2f, 0.9f));
        ImU32 colBot = ImGui::ColorConvertFloat4ToU32(ImVec4(col.x - 0.1f, col.y - 0.1f, col.z - 0.1f, 0.9f));
        
        ImVec2 pMin = pos;
        ImVec2 pMax = ImVec2(pos.x + fillW, pos.y + size.y);
        
        // ClipRect para asegurar que el relleno respete las curvas
        dl->PushClipRect(pMin, pMax, true);
        dl->AddRectFilledMultiColor(pMin, pMax, colTop, colTop, colBot, colBot);
        
        // Brillo ondulante (Líquido)
        float wave = sinf(t * 3.0f + pos.x) * 0.5f + 0.5f;
        dl->AddRectFilled(pMin, ImVec2(pMax.x, pMin.y + size.y * 0.3f), IM_COL32(255, 255, 255, (int)(40 + 30 * wave)), size.y * 0.5f);
        dl->PopClipRect();
    }

    char pctText[8];
    snprintf(pctText, sizeof(pctText), "%d%%", (int)(fraction * 100.0f));
    ImVec2 textSize = ImGui::CalcTextSize(pctText);
    ImGui::SetCursorScreenPos(ImVec2(pos.x + size.x + 15.0f, pos.y + (size.y - textSize.y) * 0.5f));
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s", pctText);

    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + size.y + 12.0f));
    ImGui::Dummy(ImVec2(0, 0));
}

void SettingsPanel::SpinnerWidget(float radius, float thickness, const ImVec4& color) {
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetCursorScreenPos();
    float       cx  = pos.x + radius;
    float       cy  = pos.y + radius;
    float       t   = (float)ImGui::GetTime();

    // Aro de fondo cristalino oscuro
    dl->AddCircle(ImVec2(cx, cy), radius, IM_COL32(20, 40, 80, 150), 32, thickness);

    // Segmento rotatorio con resplandor (Neon effect)
    const int   numSegments = 32;
    const float startAngle  = t * 5.0f;
    const float arcSpan     = IM_PI * 1.2f;

    dl->PathClear();
    for (int i = 0; i <= numSegments; i++) {
        float angle = startAngle + arcSpan * ((float)i / numSegments);
        dl->PathLineTo(ImVec2(cx + cosf(angle) * radius, cy + sinf(angle) * radius));
    }
    
    // Dibujamos dos veces, una más gruesa y transparente para simular el brillo exterior (Glow)
    ImU32 glowCol = ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, 0.4f));
    dl->PathStroke(glowCol, false, thickness + 3.0f);
    
    // Y el núcleo brillante
    for (int i = 0; i <= numSegments; i++) {
        float angle = startAngle + arcSpan * ((float)i / numSegments);
        dl->PathLineTo(ImVec2(cx + cosf(angle) * radius, cy + sinf(angle) * radius));
    }
    dl->PathStroke(ImGui::ColorConvertFloat4ToU32(color), false, thickness);

    ImGui::Dummy(ImVec2(radius * 2.0f, radius * 2.0f));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helper: SectionTitle (Cristalino y brillante)
// ─────────────────────────────────────────────────────────────────────────────
void SettingsPanel::SectionTitle(const char* label) {
    ImGui::Dummy(ImVec2(0.0f, 15.0f));

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.70f, 1.0f, 1.0f)); // Azul brillante
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;

    // Línea de sección que se desvanece
    ImU32 colLeft = IM_COL32(30, 90, 180, 180);
    ImU32 colRight = IM_COL32(30, 90, 180, 0);
    dl->AddRectFilledMultiColor(ImVec2(p.x, p.y), ImVec2(p.x + w, p.y + 1.5f), colLeft, colRight, colRight, colLeft);

    ImGui::Dummy(ImVec2(0.0f, 12.0f));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helper: HelpTooltip (Tooltips redondeados y semi-transparentes)
// ─────────────────────────────────────────────────────────────────────────────
void SettingsPanel::HelpTooltip(const char* desc) {
    ImGui::SameLine(0, 8);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.25f, 0.55f, 0.85f, 1.0f));
    ImGui::TextDisabled("(?)");
    ImGui::PopStyleColor();

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        // Estilo tooltip de cristal oscuro
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.02f, 0.05f, 0.15f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.10f, 0.40f, 0.80f, 0.6f));
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));

        ImGui::BeginTooltip();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.85f, 1.0f, 1.0f));
        ImGui::PushTextWrapPos(280.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        ImGui::EndTooltip();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }
}

void SettingsPanel::InitializeTheme() {}
} // namespace ProyecThor::UI::Settings