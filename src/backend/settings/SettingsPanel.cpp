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
    { "SNG", "Canciones",       "Etiquetas y opciones de canciones"       },
       { "KEY", "Teclas rápidas",  "Atajos de teclado disponibles"           },
    { "LNG", "Idioma",          "Idioma de la interfaz"                   },
    { "UPD", "Actualizaciones", "Versión instalada y canales"             },
    { "INT", "Integraciones",   "APIs externas (ej. banco de imagenes)"   },
};
static constexpr int k_CategoryCount = 9;

// Pequeño helper local: convierte un token de color del tema (float[4]) en
// ImVec4, con un multiplicador opcional de alpha.
static inline ImVec4 ThemeCol(const float* a, float alphaMul = 1.0f) {
    return ImVec4(a[0], a[1], a[2], a[3] * alphaMul);
}

// Suavizado exponencial independiente del framerate (0..1 por segundo de "velocidad")
static inline float SmoothTowards(float current, float target, float dt, float speed) {
    float t = 1.0f - std::exp(-speed * dt);
    return current + (target - current) * t;
}

static inline float EaseOutQuad(float t) { return 1.0f - (1.0f - t) * (1.0f - t); }

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
    if (!*isOpen) { m_WasOpenLastFrame = false; return; }

    auto&       mgr   = ProyecThor::Settings::SettingsManager::Get();
    const auto& theme = mgr.GetSettings().theme;

    const float dt = ImGui::GetIO().DeltaTime;
    m_CheckingAnim += dt * 280.0f;

    // ── Detecta la transición cerrado -> abierto ────────────────────────────
    const bool justOpened = !m_WasOpenLastFrame;
    m_WasOpenLastFrame = true;
    if (justOpened) m_OpenAnim = 0.0f;
    m_OpenAnim = std::min(1.0f, m_OpenAnim + dt * 7.0f); // ~0.14s
    const float eased = EaseOutQuad(m_OpenAnim);

    const ImVec2 baseSize(900.0f, 650.0f);

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 workCenter(vp->WorkPos.x + vp->WorkSize.x * 0.5f,
                       vp->WorkPos.y + vp->WorkSize.y * 0.5f);

    // Mientras la ventana está "apareciendo" forzamos posición y tamaño en
    // cada frame (centrada, con un ligero efecto de "pop-in" de escala).
    // Esto evita depender de cualquier posición/tamaño guardado previamente
    // en el .ini (que es lo que hacía que la ventana apareciera en una
    // esquina, a veces fuera de la pantalla, al reabrir el panel).
    if (m_OpenAnim < 1.0f) {
        float scale = 0.95f + 0.05f * eased;
        ImGui::SetNextWindowPos(workCenter, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(baseSize.x * scale, baseSize.y * scale), ImGuiCond_Always);
    } else if (justOpened) {
        ImGui::SetNextWindowPos(workCenter, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(baseSize, ImGuiCond_Always);
    }

    ImGui::SetNextWindowSizeConstraints(ImVec2(720, 500), ImVec2(FLT_MAX, FLT_MAX));

    // Estilo "Liquid Glass" tomado del tema activo, no de valores fijos.
    ImGui::PushStyleColor(ImGuiCol_WindowBg,      ThemeCol(theme.base));
    ImGui::PushStyleColor(ImGuiCol_TitleBg,       ThemeCol(theme.surface0));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ThemeCol(theme.surface1));
    ImGui::PushStyleColor(ImGuiCol_Border,        ThemeCol(theme.border, 0.85f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,  theme.windowRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, eased); // fade-in general al abrir

    // NoSavedSettings: no persistimos pos/tamaño en el .ini, así el panel
    // siempre vuelve a nacer centrado la próxima vez que se abra, sin
    // arrastrar coordenadas obsoletas de una resolución/monitor distinto.
    bool open = ImGui::Begin("Preferencias", isOpen,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);

    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(4);

    if (!open) { ImGui::End(); return; }

    ImVec2 avail    = ImGui::GetContentRegionAvail();
    float  sideW    = 240.0f;
    float  footerH  = 70.0f;
    float  contentW = avail.x - sideW;
    float  contentH = avail.y - footerH;

    // ── Sidebar ───────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeCol(theme.surface0, 0.55f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 25.0f));
    ImGui::BeginChild("##sidebar", ImVec2(sideW, contentH), false, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();
    RenderSidebar();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Línea separadora suave (efecto cristal)
    ImDrawList* winDl = ImGui::GetWindowDrawList();
    ImVec2 sep = ImGui::GetCursorScreenPos();
    winDl->AddLine(ImVec2(sep.x, sep.y), ImVec2(sep.x, sep.y + contentH),
                   ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.border, 0.9f)), 1.5f);

    ImGui::SameLine(0, 0);

    // ── Área de contenido ─────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeCol(theme.base, 0.85f));
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
    auto&       mgr   = ProyecThor::Settings::SettingsManager::Get();
    const auto& theme = mgr.GetSettings().theme;
    const float dt    = ImGui::GetIO().DeltaTime;
    ImDrawList* dl    = ImGui::GetWindowDrawList();

    ImVec4 accent = ThemeCol(theme.accent);

    ImGui::SetCursorPosX(25.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textPrimary));
    ImGui::SetWindowFontScale(1.2f);
    ImGui::TextUnformatted("ProyecThor");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::SetCursorPosX(25.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textDim));
    ImGui::Text("Versión %s", mgr.GetSettings().updates.currentVersion.c_str());
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 30.0f));

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 6.0f));

    const float itemH = 45.0f;
    const float itemW = ImGui::GetContentRegionAvail().x - 20.0f; // constante para toda la lista

    // Separamos en dos canales de dibujo: 0 = fondo animado (píldora),
    // 1 = hover/texto. Así la píldora que se desliza entre filas nunca
    // tapa el texto de la fila destino, sin importar el orden de dibujo.
    dl->ChannelsSplit(2);

    float targetPillY = -1.0f;

    for (int i = 0; i < k_CategoryCount; i++) {
        bool selected = (m_SelectedCategory == i);
        ImVec2 p = ImGui::GetCursorScreenPos();
        p.x += 10.0f; // Margen izquierdo

        ImGui::SetCursorPosX(10.0f);

        ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImVec4(0,0,0,0));

        char selectId[32];
        snprintf(selectId, sizeof(selectId), "##nav%d", i);

        dl->ChannelsSetCurrent(1);

        bool clicked = ImGui::Selectable(selectId, selected, ImGuiSelectableFlags_None, ImVec2(itemW, itemH));
        bool hovered = ImGui::IsItemHovered();

        if (clicked && m_SelectedCategory != i) {
            m_SelectedCategory = i;
            m_ContentFade = 0.0f; // dispara el fade/slide del contenido nuevo
        }

        if (selected) {
            targetPillY = p.y;
        } else if (hovered) {
            dl->AddRectFilled(p, ImVec2(p.x + itemW, p.y + itemH),
                              ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.surface2, 0.5f)), 8.0f);
        }

        float labelY = p.y + (itemH - ImGui::GetTextLineHeight()) * 0.5f;
        ImU32 labelCol = ImGui::ColorConvertFloat4ToU32(selected ? ThemeCol(theme.textPrimary) : ThemeCol(theme.textDim));
        dl->AddText(ImVec2(p.x + 25.0f, labelY), labelCol, k_Categories[i].label);

        ImGui::PopStyleColor(3);
    }

    // Píldora de selección: se desliza suavemente hacia la fila activa
    // en lugar de saltar instantáneamente.
    if (targetPillY >= 0.0f) {
        if (!m_PillInit) { m_PillY = targetPillY; m_PillInit = true; }
        m_PillY = SmoothTowards(m_PillY, targetPillY, dt, 22.0f);

        dl->ChannelsSetCurrent(0);

        ImVec2 pillP(ImGui::GetWindowPos().x + 10.0f, m_PillY);

        ImU32 colLeft  = ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.accent, 0.30f));
        ImU32 colRight = ImGui::ColorConvertFloat4ToU32(ImVec4(theme.accent[0], theme.accent[1], theme.accent[2], 0.0f));
        dl->AddRectFilledMultiColor(pillP, ImVec2(pillP.x + itemW, pillP.y + itemH), colLeft, colRight, colRight, colLeft);

        dl->AddRectFilled(ImVec2(pillP.x, pillP.y + 8.0f), ImVec2(pillP.x + 4.0f, pillP.y + itemH - 8.0f),
                          ImGui::ColorConvertFloat4ToU32(accent), 2.0f);
    }

    dl->ChannelsMerge();

    ImGui::PopStyleVar();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Área de contenido
// ─────────────────────────────────────────────────────────────────────────────

void SettingsPanel::RenderContent() {
    auto&       mgr   = ProyecThor::Settings::SettingsManager::Get();
    const auto& theme = mgr.GetSettings().theme;
    const float dt    = ImGui::GetIO().DeltaTime;
    ImDrawList* dl    = ImGui::GetWindowDrawList();
    ImVec4      accent = ThemeCol(theme.accent);

    // Avanza el fade/slide de "entrada" del contenido cuando se cambia de
    // categoría (se reinicia a 0 desde RenderSidebar al hacer clic).
    m_ContentFade = std::min(1.0f, m_ContentFade + dt * 9.0f);
    float fadeT       = EaseOutQuad(m_ContentFade);
    float slideOffset = (1.0f - fadeT) * 10.0f;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + slideOffset);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * (0.25f + 0.75f * fadeT));

    // Título de la sección
    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textPrimary));
    ImGui::SetWindowFontScale(1.6f);
    ImGui::TextUnformatted(k_Categories[m_SelectedCategory].label);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 2.0f));

    // Subtítulo
    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textDim));
    ImGui::TextUnformatted(k_Categories[m_SelectedCategory].description);
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 20.0f));

    // Separador líquido (degradado que se desvanece), color = acento del tema
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    ImU32 sepAccent = ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, 0.6f));
    ImU32 sepFade   = ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, 0.0f));
    dl->AddRectFilledMultiColor(ImVec2(p.x, p.y), ImVec2(p.x + w, p.y + 1.5f), sepAccent, sepFade, sepFade, sepAccent);

    ImGui::Dummy(ImVec2(0.0f, 25.0f));

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(15.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, theme.frameRounding);

    switch (m_SelectedCategory) {
        case 0: RenderCategoryTheme();      break;
        case 1: RenderCategoryGeneral();    break;
        case 2: RenderCategoryProjection(); break;
        case 3: RenderCategoryAudio();      break;
        case 4: RenderCategorySongs();      break;
         case 5: RenderCategoryShortcuts();  break;
        case 6: RenderCategoryLanguage();   break;
        case 7: RenderCategoryUpdates();    break;
        case 8: RenderCategoryIntegrations(); break;
        default: ImGui::TextDisabled("Categoría no implementada."); break;
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleVar(); // alpha del fade de contenido
}

// ─────────────────────────────────────────────────────────────────────────────
//  Footer / barra de guardado
// ─────────────────────────────────────────────────────────────────────────────

void SettingsPanel::RenderSaveBar() {
    auto&       mgr   = ProyecThor::Settings::SettingsManager::Get();
    const auto& theme = mgr.GetSettings().theme;

    ImVec2 winPos  = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();
    float  barH    = 70.0f;
    float  barY    = winPos.y + winSize.y - barH;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(
        ImVec2(winPos.x, barY),
        ImVec2(winPos.x + winSize.x, winPos.y + winSize.y),
        ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.surface0, 0.98f)),
        theme.windowRounding,
        ImDrawFlags_RoundCornersBottom);

    dl->AddLine(
        ImVec2(winPos.x, barY),
        ImVec2(winPos.x + winSize.x, barY),
        ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.border, 0.9f)),
        1.5f);

    float btnY   = winSize.y - barH + (barH - 36.0f) * 0.5f;
    float rightX = winSize.x - 30.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 18.0f);

    // Botón Guardar — usa el color de acento del tema activo
    ImGui::SetCursorPos(ImVec2(rightX - 160.0f, btnY));
    ImGui::PushStyleColor(ImGuiCol_Button,        ThemeCol(theme.accent, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ThemeCol(theme.accentLight));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ThemeCol(theme.accentDim));
    ImGui::PushStyleColor(ImGuiCol_Text,          ThemeCol(theme.textPrimary));

    if (ImGui::Button("Guardar ajustes", ImVec2(160.0f, 36.0f))) {
        mgr.SaveSettings();
        mgr.ApplyTheme();
        mgr.ApplyProjection();
        m_SaveStatusMsg = "Cambios guardados";
        m_SaveTimer     = 3.0f;
    }
    ImGui::PopStyleColor(4);

    // Botón Restablecer
    ImGui::SetCursorPos(ImVec2(rightX - 160.0f - 15.0f - 130.0f, btnY));
    ImGui::PushStyleColor(ImGuiCol_Button,        ThemeCol(theme.surface2, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ThemeCol(theme.surface3, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ThemeCol(theme.surface1, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ThemeCol(theme.textDim));

    if (ImGui::Button("Restablecer", ImVec2(130.0f, 36.0f))) {
        mgr.ResetToDefaults();
        m_SaveStatusMsg = "Ajustes restablecidos";
        m_SaveTimer     = 3.0f;
    }
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();

    if (!m_SaveStatusMsg.empty()) {
        float alpha = std::min(1.0f, m_SaveTimer);
        // Pequeño "pop" de entrada: aparece deslizándose levemente desde la izquierda
        float pop   = std::min(1.0f, (3.0f - m_SaveTimer) * 6.0f);
        float slide = (1.0f - EaseOutQuad(std::max(0.0f, pop))) * 8.0f;

        float textY = winSize.y - barH + (barH - ImGui::GetTextLineHeight()) * 0.5f;
        const float sideW = 240.0f;

        ImGui::SetCursorPos(ImVec2(sideW + 45.0f + slide, textY));
        ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.success, alpha));
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
//  Helper: SectionTitle (usa acento del tema)
// ─────────────────────────────────────────────────────────────────────────────
void SettingsPanel::SectionTitle(const char* label) {
    auto&       mgr   = ProyecThor::Settings::SettingsManager::Get();
    const auto& theme = mgr.GetSettings().theme;

    ImGui::Dummy(ImVec2(0.0f, 15.0f));

    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.accentLight));
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;

    ImU32 colLeft  = ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.accent, 0.55f));
    ImU32 colRight = ImGui::ColorConvertFloat4ToU32(ImVec4(theme.accent[0], theme.accent[1], theme.accent[2], 0.0f));
    dl->AddRectFilledMultiColor(ImVec2(p.x, p.y), ImVec2(p.x + w, p.y + 1.5f), colLeft, colRight, colRight, colLeft);

    ImGui::Dummy(ImVec2(0.0f, 12.0f));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helper: HelpTooltip (tooltip de cristal, usa colores del tema)
// ─────────────────────────────────────────────────────────────────────────────
void SettingsPanel::HelpTooltip(const char* desc) {
    auto&       mgr   = ProyecThor::Settings::SettingsManager::Get();
    const auto& theme = mgr.GetSettings().theme;

    ImGui::SameLine(0, 8);
    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.accentDim));
    ImGui::TextDisabled("(?)");
    ImGui::PopStyleColor();

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ThemeCol(theme.surface1, 0.98f));
        ImGui::PushStyleColor(ImGuiCol_Border,  ThemeCol(theme.accent, 0.55f));
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, theme.frameRounding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));

        ImGui::BeginTooltip();
        ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textPrimary));
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