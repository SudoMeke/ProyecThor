#include "Announcements.h"
#include "backend/core/PresentationCore.h"
#include "frontend/ui/UIStrings.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cstring>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
//  Nota de integración
//
//  1. En PreviewPanel.h agrega:
//       #include "../panels/Announcements.h"
//       bool          m_ShowAnnouncements = true;
//       Announcements m_Announcements;
//
//  2. En PreviewPanel::Render():
//       if (m_ShowAnnouncements) m_Announcements.Render();
//
//  3. En UIManager::RenderAll(), bloque ProjectorLive, antes de ImGui::End():
//       if (previewPanel && previewPanel->m_Announcements.IsLive()) {
//           static auto lastTime = std::chrono::steady_clock::now();
//           auto now = std::chrono::steady_clock::now();
//           float dt = std::chrono::duration<float>(now - lastTime).count();
//           lastTime = now;
//           previewPanel->m_Announcements.RenderOnProjector(
//               ImGui::GetWindowDrawList(),
//               (float)mx, (float)my,
//               (float)mode->width, (float)mode->height, dt);
//       }
//
//  4. En UIManager::BeginDockspace(), menú Vista:
//       ImGui::MenuItem("Anuncios", NULL, &previewPanel->m_ShowAnnouncements);
//
//  5. En UIManager::BeginDockspace(), DockBuilder:
//       ImGui::DockBuilderDockWindow("Anuncios", dock_right_bottom);
//
// ─────────────────────────────────────────────────────────────────────────────

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers locales
// ─────────────────────────────────────────────────────────────────────────────

static ImU32 AnnColU32(float r, float g, float b, float a = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
}

static bool SmallIconButton(const char* label, ImVec2 size,
                            ImVec4 col, ImVec4 colHov, ImVec4 colAct) {
    ImGui::PushStyleColor(ImGuiCol_Button,        col);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colHov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  colAct);
    bool pressed = ImGui::Button(label, size);
    ImGui::PopStyleColor(3);
    return pressed;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor
// ─────────────────────────────────────────────────────────────────────────────

Announcements::Announcements() {
    Message first;
    std::strncpy(first.text, "Bienvenidos al servicio", sizeof(first.text) - 1);
    m_Messages.push_back(first);

    // Cargar lista de fuentes al iniciar
    SyncFontList();
}

// ─────────────────────────────────────────────────────────────────────────────
//  SyncFontList
// ─────────────────────────────────────────────────────────────────────────────

void Announcements::SyncFontList() {
    Core::PresentationCore::Get().SyncFontListFromDisk(m_FontList);
}

// ─────────────────────────────────────────────────────────────────────────────
//  GetCurrentMessage
// ─────────────────────────────────────────────────────────────────────────────

const std::string& Announcements::GetCurrentMessage() const {
    static std::string s_Cache;
    s_Cache.clear();

    if (m_Messages.empty()) return s_Cache;

    int total = (int)m_Messages.size();
    for (int i = 0; i < total; ++i) {
        int idx = (m_ActiveIndex + i) % total;
        if (m_Messages[idx].enabled && m_Messages[idx].text[0] != '\0') {
            s_Cache = m_Messages[idx].text;
            return s_Cache;
        }
    }
    return s_Cache;
}

// ─────────────────────────────────────────────────────────────────────────────
//  TickScroll
// ─────────────────────────────────────────────────────────────────────────────

void Announcements::TickScroll(float deltaTime, float contentWidth, float screenW) {
    if (m_Paused || m_Messages.empty()) return;

    float speed = (m_Direction == Direction::RightToLeft)
                  ? -m_SpeedPxPerSec
                  :  m_SpeedPxPerSec;

    m_ScrollOffset += speed * deltaTime;

    if (m_Direction == Direction::RightToLeft) {
        if (m_ScrollOffset < -(contentWidth + m_GapWidth)) {
            m_ScrollOffset = screenW;

            int total = (int)m_Messages.size();
            for (int i = 1; i <= total; ++i) {
                int next = (m_ActiveIndex + i) % total;
                if (m_Messages[next].enabled && m_Messages[next].text[0] != '\0') {
                    m_ActiveIndex = next;
                    break;
                }
            }
        }
    } else {
        if (m_ScrollOffset > screenW + m_GapWidth) {
            m_ScrollOffset = -(contentWidth);

            int total = (int)m_Messages.size();
            for (int i = 1; i <= total; ++i) {
                int next = (m_ActiveIndex + i) % total;
                if (m_Messages[next].enabled && m_Messages[next].text[0] != '\0') {
                    m_ActiveIndex = next;
                    break;
                }
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderOnProjector
// ─────────────────────────────────────────────────────────────────────────────

void Announcements::RenderOnProjector(void* drawListPtr,
                                      float screenX, float screenY,
                                      float screenW, float screenH,
                                      float deltaTime) {
    if (!m_IsLive || m_Messages.empty()) return;

    ImDrawList* drawList = static_cast<ImDrawList*>(drawListPtr);
    auto& core           = Core::PresentationCore::Get();

    const std::string& msg = GetCurrentMessage();
    if (msg.empty()) return;

    float screenScale = screenW / 1920.0f;

    // ── Resolver fuente, tamaño y color según el modo activo ──────────────
    float   fontSize = m_FontSize * screenScale;
    ImFont* font     = nullptr;
    ImU32   textCol  = ImGui::ColorConvertFloat4ToU32(
        ImVec4(m_TextColor[0], m_TextColor[1], m_TextColor[2], m_TextColor[3]));

    if (m_StyleMode == 0 && m_AssignedStyleName[0] != '\0') {
        // Modo: estilo guardado — usa todos los campos del SavedStyle
        Core::SavedStyle resolvedStyle;
        if (core.GetSavedStyle(std::string(m_AssignedStyleName), resolvedStyle)) {
            fontSize = resolvedStyle.size * screenScale;
            font     = core.GetImGuiFont(resolvedStyle.fontName, fontSize);
            textCol  = ImGui::ColorConvertFloat4ToU32(
                ImVec4(resolvedStyle.color[0], resolvedStyle.color[1],
                       resolvedStyle.color[2], resolvedStyle.color[3]));
        }
    } else if (m_StyleMode == 1) {
        // Modo: estilo inline — fuente por nombre desde m_FontList
        if (m_SelectedFontIndex >= 0 && m_SelectedFontIndex < (int)m_FontList.size()) {
            font = core.GetImGuiFont(m_FontList[m_SelectedFontIndex], fontSize);
        }
    }

    if (!font) font = ImGui::GetFont();

    // ── Medir texto ────────────────────────────────────────────────────────
    ImVec2 textSize     = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, msg.c_str());
    float  contentWidth = textSize.x;

    TickScroll(deltaTime, contentWidth, screenW);

    // ── Posición vertical del banner ───────────────────────────────────────
    float bannerH = screenH * m_BannerHeightPct;
    float bannerY = screenY;

    switch (m_VPosition) {
        case 0: bannerY = screenY;                               break;
        case 1: bannerY = screenY + (screenH - bannerH) * 0.5f; break;
        case 2: bannerY = screenY + screenH - bannerH;           break;
        default: bannerY = screenY + screenH - bannerH;          break;
    }

    float bannerX  = screenX;
    float bannerX2 = screenX + screenW;
    float bannerY2 = bannerY + bannerH;

    // ── Fondo ──────────────────────────────────────────────────────────────
    if (m_ShowBg) {
        drawList->AddRectFilled(
            ImVec2(bannerX, bannerY), ImVec2(bannerX2, bannerY2),
            AnnColU32(m_BgR, m_BgG, m_BgB, m_BgA));

        float borderThickness = 2.0f * screenScale;
        ImU32 borderCol       = AnnColU32(0.25f, 0.30f, 0.55f, 0.80f);

        if (m_VPosition == 2 || m_VPosition == 1) {
            drawList->AddLine(ImVec2(bannerX, bannerY),  ImVec2(bannerX2, bannerY),
                borderCol, borderThickness);
        }
        if (m_VPosition == 0 || m_VPosition == 1) {
            drawList->AddLine(ImVec2(bannerX, bannerY2), ImVec2(bannerX2, bannerY2),
                borderCol, borderThickness);
        }
    }

    drawList->PushClipRect(ImVec2(bannerX, bannerY), ImVec2(bannerX2, bannerY2), true);

    float textX   = screenX + m_ScrollOffset;
    float textY   = bannerY + (bannerH - textSize.y) * 0.5f;
    float shadowOff = 2.0f * screenScale;

    drawList->AddText(font, fontSize,
        ImVec2(textX + shadowOff, textY + shadowOff),
        IM_COL32(0, 0, 0, 200), msg.c_str());

    drawList->AddText(font, fontSize,
        ImVec2(textX, textY),
        textCol, msg.c_str());

    drawList->PopClipRect();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Render  —  panel de control (ImGui)
// ─────────────────────────────────────────────────────────────────────────────

void Announcements::Render() {
    auto& core = Core::PresentationCore::Get();

    // ── Delta time interno ─────────────────────────────────────────────────
    float deltaTime = 0.016f;
    if (!m_FirstFrame) {
        auto now  = std::chrono::steady_clock::now();
        deltaTime = std::chrono::duration<float>(now - m_LastFrameTime).count();
        deltaTime = std::min(deltaTime, 0.1f);
    }
    m_FirstFrame    = false;
    m_LastFrameTime = std::chrono::steady_clock::now();

    // ── Ventana ────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07f, 0.08f, 0.10f, 1.0f));
    ImGui::Begin("Anuncios");

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.60f, 1.0f, 1.0f));
    ImGui::TextUnformatted("Anuncios");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

    // ─────────────────────────────────────────────────────────────────────
    //  SECCIÓN: Preview del letrero
    // ─────────────────────────────────────────────────────────────────────
    {
        float panelW   = ImGui::GetContentRegionAvail().x;
        float previewH = 50.0f;

        ImVec2      previewPos = ImGui::GetCursorScreenPos();
        ImDrawList* dl         = ImGui::GetWindowDrawList();

        dl->AddRectFilled(
            previewPos,
            ImVec2(previewPos.x + panelW, previewPos.y + previewH),
            AnnColU32(m_BgR, m_BgG, m_BgB, m_IsLive ? m_BgA : 0.5f),
            6.0f);

        dl->AddRect(
            previewPos,
            ImVec2(previewPos.x + panelW, previewPos.y + previewH),
            m_IsLive
                ? AnnColU32(0.35f, 0.50f, 1.0f, 0.7f)
                : AnnColU32(0.20f, 0.22f, 0.30f, 0.5f),
            6.0f, 0, 1.5f);

        dl->PushClipRect(
            previewPos,
            ImVec2(previewPos.x + panelW, previewPos.y + previewH),
            true);

        const std::string& msg = GetCurrentMessage();
        if (!msg.empty()) {
            ImFont* previewFont     = ImGui::GetFont();
            float   previewFontSize = ImGui::GetFontSize();

            if (m_StyleMode == 1) {
                // Escalar m_FontSize al ancho del panel
                float scale     = panelW / 1920.0f;
                previewFontSize = std::max(8.0f, m_FontSize * scale);
                if (m_SelectedFontIndex >= 0 && m_SelectedFontIndex < (int)m_FontList.size()) {
                    ImFont* f = core.GetImGuiFont(m_FontList[m_SelectedFontIndex], previewFontSize);
                    if (f) previewFont = f;
                }
            } else if (m_StyleMode == 0 && m_AssignedStyleName[0] != '\0') {
                Core::SavedStyle resolvedStyle;
                if (core.GetSavedStyle(std::string(m_AssignedStyleName), resolvedStyle)) {
                    float scale     = panelW / 1920.0f;
                    previewFontSize = std::max(8.0f, resolvedStyle.size * scale);
                    ImFont* f = core.GetImGuiFont(resolvedStyle.fontName, previewFontSize);
                    if (f) previewFont = f;
                }
            }

            ImVec2 textSize  = previewFont->CalcTextSizeA(previewFontSize, FLT_MAX, 0.0f, msg.c_str());
            float scrollFrac = m_ScrollOffset / 1920.0f;
            float textX      = previewPos.x + scrollFrac * panelW;
            float textY      = previewPos.y + (previewH - textSize.y) * 0.5f;

            ImU32 previewTextCol = m_IsLive
                ? IM_COL32(200, 220, 255, 255)
                : IM_COL32(130, 140, 160, 255);

            if (m_StyleMode == 1) {
                previewTextCol = ImGui::ColorConvertFloat4ToU32(
                    ImVec4(m_TextColor[0], m_TextColor[1],
                           m_TextColor[2], m_TextColor[3]));
            }

            dl->AddText(previewFont, previewFontSize,
                ImVec2(textX + 1.0f, textY + 1.0f), IM_COL32(0, 0, 0, 180), msg.c_str());
            dl->AddText(previewFont, previewFontSize,
                ImVec2(textX, textY), previewTextCol, msg.c_str());
        } else {
            ImVec2 phSize = ImGui::CalcTextSize("Sin mensajes habilitados");
            dl->AddText(
                ImVec2(previewPos.x + (panelW - phSize.x) * 0.5f,
                       previewPos.y + (previewH - phSize.y) * 0.5f),
                IM_COL32(80, 85, 100, 200),
                "Sin mensajes habilitados");
        }

        dl->PopClipRect();
        ImGui::Dummy(ImVec2(panelW, previewH));
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ─────────────────────────────────────────────────────────────────────
    //  SECCIÓN: Lista de mensajes
    // ─────────────────────────────────────────────────────────────────────

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.58f, 0.70f, 1.0f));
    ImGui::TextUnformatted("Mensajes");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    float listH = std::min(160.0f, (float)m_Messages.size() * 36.0f + 8.0f);
    listH       = std::max(listH, 44.0f);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.065f, 0.070f, 0.095f, 1.0f));
    ImGui::BeginChild("##ann_list", ImVec2(0, listH), true, ImGuiWindowFlags_None);

    int toDelete = -1;
    int toMoveUp = -1;

    for (int i = 0; i < (int)m_Messages.size(); ++i) {
        ImGui::PushID(i);

        bool isActive = (i == m_ActiveIndex);
        if (isActive && m_IsLive) {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.16f, 0.28f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f, 0.11f, 0.15f, 1.0f));
        }

        ImGui::Checkbox("##en", &m_Messages[i].enabled);
        ImGui::SameLine(0, 6);

        float fieldW = ImGui::GetContentRegionAvail().x - 60.0f;
        ImGui::SetNextItemWidth(fieldW);
        ImGui::InputText("##msg", m_Messages[i].text, sizeof(m_Messages[i].text));

        ImGui::PopStyleColor();
        ImGui::SameLine(0, 6);

        if (i > 0) {
            if (SmallIconButton("^", ImVec2(22, 22),
                ImVec4(0.14f, 0.15f, 0.22f, 1.0f),
                ImVec4(0.20f, 0.22f, 0.32f, 1.0f),
                ImVec4(0.28f, 0.32f, 0.50f, 1.0f))) {
                toMoveUp = i;
            }
        } else {
            ImGui::Dummy(ImVec2(22, 22));
        }

        ImGui::SameLine(0, 4);

        if (SmallIconButton("x", ImVec2(22, 22),
            ImVec4(0.30f, 0.10f, 0.10f, 1.0f),
            ImVec4(0.50f, 0.15f, 0.15f, 1.0f),
            ImVec4(0.65f, 0.10f, 0.10f, 1.0f))) {
            toDelete = i;
        }

        ImGui::PopID();
    }

    if (toMoveUp > 0) {
        std::swap(m_Messages[toMoveUp], m_Messages[toMoveUp - 1]);
        if (m_ActiveIndex == toMoveUp)          m_ActiveIndex = toMoveUp - 1;
        else if (m_ActiveIndex == toMoveUp - 1) m_ActiveIndex = toMoveUp;
    }

    if (toDelete >= 0) {
        m_Messages.erase(m_Messages.begin() + toDelete);
        if (m_ActiveIndex >= (int)m_Messages.size())
            m_ActiveIndex = std::max(0, (int)m_Messages.size() - 1);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f, 0.16f, 0.28f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.22f, 0.38f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.25f, 0.32f, 0.55f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);

    if (ImGui::Button("+ Agregar mensaje", ImVec2(ImGui::GetContentRegionAvail().x, 28.0f))) {
        Message nm;
        std::strncpy(nm.text, "Nuevo anuncio", sizeof(nm.text) - 1);
        m_Messages.push_back(nm);
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ─────────────────────────────────────────────────────────────────────
    //  SECCIÓN: Animación
    // ─────────────────────────────────────────────────────────────────────

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.58f, 0.70f, 1.0f));
    ImGui::TextUnformatted("Animacion");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.10f, 0.11f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.14f, 0.15f, 0.21f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

    int dir = (int)m_Direction;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::Combo("##dir", &dir, "Derecha a izquierda\0Izquierda a derecha\0")) {
        m_Direction    = (Direction)dir;
        m_ScrollOffset = (m_Direction == Direction::RightToLeft) ? 1920.0f : -400.0f;
    }

    ImGui::Spacing();

    float panelW = ImGui::GetContentRegionAvail().x;
    float halfW  = (panelW - 8.0f) * 0.5f;

    ImGui::SetNextItemWidth(halfW);
    ImGui::SliderFloat("##spd", &m_SpeedPxPerSec, 20.0f, 600.0f, "Vel: %.0f px/s");
    ImGui::SameLine(0, 8);
    ImGui::SetNextItemWidth(halfW);
    ImGui::SliderFloat("##gap", &m_GapWidth, 50.0f, 600.0f, "Gap: %.0f px");

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.45f, 0.60f, 1.0f, 1.0f));
    ImGui::Checkbox("Pausar animacion", &m_Paused);
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ─────────────────────────────────────────────────────────────────────
    //  SECCIÓN: Posición y apariencia del banner
    // ─────────────────────────────────────────────────────────────────────

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.58f, 0.70f, 1.0f));
    ImGui::TextUnformatted("Posicion y apariencia");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.10f, 0.11f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.14f, 0.15f, 0.21f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::Combo("##vpos", &m_VPosition, "Arriba\0Centro\0Abajo\0");

    ImGui::Spacing();

    float bannerPct = m_BannerHeightPct * 100.0f;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::SliderFloat("##bh", &bannerPct, 3.0f, 20.0f, "Alto: %.1f%%")) {
        m_BannerHeightPct = bannerPct / 100.0f;
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.45f, 0.60f, 1.0f, 1.0f));
    ImGui::Checkbox("Mostrar fondo", &m_ShowBg);
    ImGui::PopStyleColor();

    if (m_ShowBg) {
        ImGui::SameLine(0, 12);
        float bgColor[4] = { m_BgR, m_BgG, m_BgB, m_BgA };
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        if (ImGui::ColorEdit4("##bgcol", bgColor,
            ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar |
            ImGuiColorEditFlags_NoInputs)) {
            m_BgR = bgColor[0]; m_BgG = bgColor[1];
            m_BgB = bgColor[2]; m_BgA = bgColor[3];
        }
        ImGui::PopStyleVar();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Color de fondo del banner");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ─────────────────────────────────────────────────────────────────────
    //  SECCIÓN: Estilo de texto
    // ─────────────────────────────────────────────────────────────────────

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.58f, 0.70f, 1.0f));
    ImGui::TextUnformatted("Estilo de texto");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // ── Selector de modo ──────────────────────────────────────────────────
    {
        float modeW = (ImGui::GetContentRegionAvail().x - 4.0f) * 0.5f;
        float modeH = 26.0f;

        auto ModeButton = [&](const char* label, int modeValue) {
            bool   active = (m_StyleMode == modeValue);
            ImVec4 bg     = active ? ImVec4(0.18f, 0.24f, 0.48f, 1.0f)
                                   : ImVec4(0.10f, 0.11f, 0.15f, 1.0f);
            ImVec4 bgH    = active ? ImVec4(0.22f, 0.30f, 0.55f, 1.0f)
                                   : ImVec4(0.14f, 0.15f, 0.21f, 1.0f);
            ImVec4 textC  = active ? ImVec4(0.55f, 0.75f, 1.0f, 1.0f)
                                   : ImVec4(0.50f, 0.52f, 0.62f, 1.0f);

            ImGui::PushStyleColor(ImGuiCol_Button,        bg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bgH);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.28f, 0.36f, 0.65f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,          textC);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);

            if (ImGui::Button(label, ImVec2(modeW, modeH)))
                m_StyleMode = modeValue;

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
        };

        ModeButton("Estilo guardado##mode0", 0);
        ImGui::SameLine(0, 4);
        ModeButton("Editar estilo##mode1",   1);
    }

    ImGui::Spacing();

    // ─────────────────────────────────────────────────────────────────────
    //  Modo 0: elegir estilo guardado
    // ─────────────────────────────────────────────────────────────────────
    if (m_StyleMode == 0) {
        std::vector<std::string> styleNames = core.GetSavedStyleNames();

        if (styleNames.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.40f, 0.40f, 1.0f));
            ImGui::TextWrapped("No hay estilos guardados. Usa 'Editar estilo' para crear uno.");
            ImGui::PopStyleColor();
        } else {
            int currentIdx = 0;
            for (int i = 0; i < (int)styleNames.size(); ++i) {
                if (styleNames[i] == std::string(m_AssignedStyleName)) {
                    currentIdx = i;
                    break;
                }
            }

            std::string comboItems;
            for (const auto& name : styleNames) {
                comboItems += name;
                comboItems += '\0';
            }
            comboItems += '\0';

            ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.10f, 0.11f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.14f, 0.15f, 0.21f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

            if (ImGui::Combo("##stylesel", &currentIdx, comboItems.c_str())) {
                if (currentIdx >= 0 && currentIdx < (int)styleNames.size()) {
                    std::strncpy(m_AssignedStyleName,
                                 styleNames[currentIdx].c_str(),
                                 sizeof(m_AssignedStyleName) - 1);
                    m_AssignedStyleName[sizeof(m_AssignedStyleName) - 1] = '\0';

                    // Precargar los valores en los campos inline para coherencia
                    Core::SavedStyle loaded;
                    if (core.GetSavedStyle(styleNames[currentIdx], loaded)) {
                        m_FontSize     = loaded.size;
                        m_TextColor[0] = loaded.color[0];
                        m_TextColor[1] = loaded.color[1];
                        m_TextColor[2] = loaded.color[2];
                        m_TextColor[3] = loaded.color[3];
                    }
                }
            }

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);

            // Resumen del estilo activo
            if (m_AssignedStyleName[0] != '\0') {
                Core::SavedStyle preview;
                if (core.GetSavedStyle(std::string(m_AssignedStyleName), preview)) {
                    ImGui::Spacing();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.60f, 1.0f, 0.8f));
                    ImGui::Text("Estilo: %s  |  %.0f px  |  %s",
                        m_AssignedStyleName, preview.size, preview.fontName.c_str());
                    ImGui::PopStyleColor();
                }
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    //  Modo 1: editar fuente, tamaño y color inline + guardar
    // ─────────────────────────────────────────────────────────────────────
    else {
        // ── Botón recargar fuentes ────────────────────────────────────────
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.10f, 0.11f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.14f, 0.15f, 0.21f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.20f, 0.22f, 0.30f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        if (ImGui::Button("Recargar fuentes##ann", ImVec2(ImGui::GetContentRegionAvail().x, 24.0f))) {
            SyncFontList();
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        ImGui::Spacing();

        // ── Fuente ────────────────────────────────────────────────────────
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.48f, 0.58f, 1.0f));
        ImGui::TextUnformatted("Fuente");
        ImGui::PopStyleColor();

        if (!m_FontList.empty()) {
            m_SelectedFontIndex = std::max(0, std::min(m_SelectedFontIndex, (int)m_FontList.size() - 1));

            std::string fontComboItems;
            for (const auto& fn : m_FontList) {
                fontComboItems += fn;
                fontComboItems += '\0';
            }
            fontComboItems += '\0';

            ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.10f, 0.11f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.14f, 0.15f, 0.21f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::Combo("##fontsel", &m_SelectedFontIndex, fontComboItems.c_str());
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.40f, 0.40f, 1.0f));
            ImGui::TextUnformatted("No hay fuentes cargadas. Presiona 'Recargar fuentes'.");
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();

        // ── Tamaño de fuente ──────────────────────────────────────────────
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.48f, 0.58f, 1.0f));
        ImGui::TextUnformatted("Tamanio (px a 1920px de ancho)");
        ImGui::PopStyleColor();

        {
            float availW  = ImGui::GetContentRegionAvail().x;
            float sliderW = availW * 0.70f - 4.0f;
            float inputW  = availW * 0.30f - 4.0f;

            ImGui::PushStyleColor(ImGuiCol_FrameBg,          ImVec4(0.10f, 0.11f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,   ImVec4(0.14f, 0.15f, 0.21f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_SliderGrab,       ImVec4(0.35f, 0.50f, 1.0f, 0.90f));
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.45f, 0.60f, 1.0f, 1.00f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

            ImGui::SetNextItemWidth(sliderW);
            ImGui::SliderFloat("##fontSize", &m_FontSize, 12.0f, 300.0f, "%.0f px");
            ImGui::SameLine(0, 8);
            ImGui::SetNextItemWidth(inputW);
            ImGui::InputFloat("##fontSizeInput", &m_FontSize, 0.0f, 0.0f, "%.0f");
            m_FontSize = std::max(12.0f, std::min(m_FontSize, 300.0f));

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
        }

        ImGui::Spacing();

        // ── Color del texto ───────────────────────────────────────────────
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.48f, 0.58f, 1.0f));
        ImGui::TextUnformatted("Color del texto");
        ImGui::PopStyleColor();

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        ImGui::ColorEdit4("##textcol", m_TextColor,
            ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs);
        ImGui::PopStyleVar();

        ImGui::Spacing();

        // ── Botón guardar este estilo con nombre ──────────────────────────
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f, 0.22f, 0.40f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.28f, 0.52f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.10f, 0.18f, 0.32f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);

        if (ImGui::Button("Guardar como estilo...##annSave",
                          ImVec2(ImGui::GetContentRegionAvail().x, 28.0f))) {
            if (m_SaveStyleName[0] == '\0') {
                std::strncpy(m_SaveStyleName, "Mi estilo anuncio",
                             sizeof(m_SaveStyleName) - 1);
            }
            ImGui::OpenPopup("##ann_save_popup");
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        // ── Popup de guardado ──────────────────────────────────────────────
        ImGui::SetNextWindowSize(ImVec2(320.0f, 0.0f), ImGuiCond_Always);
        if (ImGui::BeginPopup("##ann_save_popup")) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.58f, 0.70f, 1.0f));
            ImGui::TextUnformatted("Nombre del estilo:");
            ImGui::PopStyleColor();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.10f, 0.11f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.14f, 0.15f, 0.21f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            ImGui::SetNextItemWidth(300.0f);
            ImGui::InputText("##annSaveName", m_SaveStyleName, sizeof(m_SaveStyleName));
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);

            ImGui::Spacing();

            bool nameOk = (m_SaveStyleName[0] != '\0');

            if (!nameOk)
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);

            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.15f, 0.22f, 0.55f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.30f, 0.70f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.10f, 0.16f, 0.42f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);

            if (ImGui::Button("Guardar##annSaveBtn", ImVec2(144.0f, 28.0f)) && nameOk) {
                // Construir el SavedStyle con los valores inline actuales.
                // SaveStyle() toma el nombre desde el campo style.name.
                Core::SavedStyle newStyle;
                newStyle.name      = std::string(m_SaveStyleName);
                newStyle.size      = m_FontSize;
                newStyle.color[0]  = m_TextColor[0];
                newStyle.color[1]  = m_TextColor[1];
                newStyle.color[2]  = m_TextColor[2];
                newStyle.color[3]  = m_TextColor[3];

                if (m_SelectedFontIndex >= 0 && m_SelectedFontIndex < (int)m_FontList.size()) {
                    newStyle.fontName = m_FontList[m_SelectedFontIndex];
                } else {
                    newStyle.fontName = "Predeterminada";
                }

                // Defaults razonables para los campos que Announcements no edita
                newStyle.hAlign    = 1;
                newStyle.vAlign    = 1;
                newStyle.autoScale = false;
                for (int k = 0; k < 4; ++k) newStyle.margins[k] = 50.0f;

                core.SaveStyle(newStyle);

                // Seleccionar el estilo recién guardado para que quede activo
                std::strncpy(m_AssignedStyleName, m_SaveStyleName,
                             sizeof(m_AssignedStyleName) - 1);
                m_AssignedStyleName[sizeof(m_AssignedStyleName) - 1] = '\0';

                ImGui::CloseCurrentPopup();
            }

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);

            if (!nameOk) ImGui::PopStyleVar();

            ImGui::SameLine(0, 8);

            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.14f, 0.15f, 0.20f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.22f, 0.30f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.10f, 0.11f, 0.16f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);

            if (ImGui::Button("Cancelar##annCancelBtn", ImVec2(144.0f, 28.0f)))
                ImGui::CloseCurrentPopup();

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);

            ImGui::EndPopup();
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ─────────────────────────────────────────────────────────────────────
    //  SECCIÓN: Transmisión en vivo
    // ─────────────────────────────────────────────────────────────────────

    float btnW = ImGui::GetContentRegionAvail().x;
    float btnH = 38.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

    if (!m_IsLive) {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.15f, 0.22f, 0.55f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.30f, 0.70f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.10f, 0.16f, 0.42f, 1.0f));

        if (ImGui::Button("Transmitir anuncios", ImVec2(btnW, btnH))) {
            m_IsLive       = true;
            m_Paused       = false;
            m_ScrollOffset = (m_Direction == Direction::RightToLeft) ? 1920.0f : -400.0f;
            m_ActiveIndex  = 0;
            for (int i = 0; i < (int)m_Messages.size(); ++i) {
                if (m_Messages[i].enabled && m_Messages[i].text[0] != '\0') {
                    m_ActiveIndex = i;
                    break;
                }
            }
        }

        ImGui::PopStyleColor(3);
    } else {
        float halfBtn = (btnW - 8.0f) * 0.5f;

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.55f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.20f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.42f, 0.10f, 0.10f, 1.0f));

        if (ImGui::Button("Detener", ImVec2(halfBtn, btnH))) {
            m_IsLive = false;
            m_Paused = false;
        }

        ImGui::PopStyleColor(3);
        ImGui::SameLine(0, 8);

        if (!m_Paused) {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.50f, 0.38f, 0.08f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.50f, 0.10f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.38f, 0.28f, 0.05f, 1.0f));
            if (ImGui::Button("Pausar", ImVec2(halfBtn, btnH))) m_Paused = true;
            ImGui::PopStyleColor(3);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f, 0.35f, 0.18f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.48f, 0.24f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.08f, 0.26f, 0.12f, 1.0f));
            if (ImGui::Button("Reanudar", ImVec2(halfBtn, btnH))) m_Paused = false;
            ImGui::PopStyleColor(3);
        }

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.30f, 0.30f, 1.0f));
        ImGui::TextUnformatted("●");
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 4);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.88f, 0.95f, 1.0f));
        ImGui::TextUnformatted(m_Paused ? "EN PAUSA" : "EN VIVO");
        ImGui::PopStyleColor();

        const std::string& currentMsg = GetCurrentMessage();
        if (!currentMsg.empty()) {
            ImGui::SameLine(0, 12);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.50f, 0.65f, 1.0f));
            std::string display = currentMsg.size() > 28
                                  ? currentMsg.substr(0, 25) + "..."
                                  : currentMsg;
            ImGui::TextUnformatted(display.c_str());
            ImGui::PopStyleColor();
        }
    }

    ImGui::PopStyleVar();

    ImGui::End();
    ImGui::PopStyleColor();
}

} // namespace ProyecThor::UI