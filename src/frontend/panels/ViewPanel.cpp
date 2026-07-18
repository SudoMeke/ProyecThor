#include "ViewPanel.h"
#include "UIManager.h"
#include "DesignSystem.h"
#include "frontend/ui/bin/StyleGeneralApp.h"
#include "backend/core/PresentationCore.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <GLFW/glfw3.h>

namespace ProyecThor::UI {

static constexpr float kQuickActionsRailW = 40.0f;

namespace {

ImVec4 ToVec4(ImU32 col) { return ImGui::ColorConvertU32ToFloat4(col); }
ImVec4 Brighten(const ImVec4& c, float amount)
{
    return ImVec4(
        std::clamp(c.x + amount, 0.0f, 1.0f),
        std::clamp(c.y + amount, 0.0f, 1.0f),
        std::clamp(c.z + amount, 0.0f, 1.0f),
        c.w);
}

// Botón de celda plano — sin esquinas redondeadas, ancho completo del riel y
// separador inferior de 1px: da el efecto de "grilla" tipo hoja de cálculo
// (Excel) / toolbar de Holyrics-ProPresenter en vez de tarjetas vistosas.
bool QuickActionButton(const char* id, const char* iconKey, const char* fallbackGlyph,
                       const char* tooltip, ImVec2 size, ImVec4 bgColor, ImVec4 hoverColor,
                       ImVec4 activeColor, ImVec4 tint, bool toggledOn)
{
    ImVec4 restColor = toggledOn ? activeColor : bgColor;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        restColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  activeColor);
    ImGui::PushStyleColor(ImGuiCol_Text,          tint);

    auto it = StyleGeneralApp::Icons.find(iconKey);
    bool hasIcon = (it != StyleGeneralApp::Icons.end() && it->second.textureID != nullptr);
    std::string label = (hasIcon ? "" : std::string(fallbackGlyph)) + "##" + id;

    bool clicked = ImGui::Button(label.c_str(), size);

    ImVec2 bMin = ImGui::GetItemRectMin();
    ImVec2 bMax = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (hasIcon)
    {
        const float iconSide = std::min(size.x, size.y) * 0.42f;
        const ImVec2 center  = { (bMin.x + bMax.x) * 0.5f, (bMin.y + bMax.y) * 0.5f };
        const ImVec2 pMin    = { center.x - iconSide * 0.5f, center.y - iconSide * 0.5f };
        const ImVec2 pMax    = { center.x + iconSide * 0.5f, center.y + iconSide * 0.5f };

        dl->AddImage(it->second.textureID, pMin, pMax,
            ImVec2(0, 0), ImVec2(1, 1),
            ImGui::ColorConvertFloat4ToU32(tint));
    }

    // Línea fina de "celda" — misma idea que los bordes de una hoja de cálculo.
    dl->AddLine({ bMin.x, bMax.y }, { bMax.x, bMax.y }, IM_COL32(0, 0, 0, 120), 1.0f);

    // Barra izquierda delgada cuando el estado está activo/encendido.
    if (toggledOn)
    {
        ImU32 accent = ImGui::ColorConvertFloat4ToU32(tint);
        dl->AddRectFilled({ bMin.x, bMin.y + 3.0f }, { bMin.x + 2.0f, bMax.y - 3.0f }, accent);
    }

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();

    if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        ImGui::SetTooltip("%s", tooltip);

    return clicked;
}

} // namespace

void ViewPanel::Render()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.04f, 0.04f, 0.06f, 1.0f));

    bool visible = ImGui::Begin("Vista en Vivo");

    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(1);

    if (visible)
    {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        const float railW    = kQuickActionsRailW;
        const float contentW = std::max(0.0f, avail.x - railW);

        // Children con padding cero — el estilo global usa WindowPadding
        // (22,18), que aquí sólo recortaría el video y el riel angosto.
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        if (contentW > 8.0f && avail.y > 8.0f)
        {
            ImGui::BeginChild("##viewVideoArea", ImVec2(contentW, avail.y), false,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            RenderContent(contentW, avail.y);
            ImGui::EndChild();
        }

        ImGui::SameLine(0.0f, 0.0f);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.06f, 0.09f, 1.0f));
        ImGui::BeginChild("##viewQuickActions", ImVec2(railW, avail.y), false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        RenderQuickActions(railW, avail.y);
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::PopStyleVar();
    }

    ImGui::End();
}

void ViewPanel::RenderQuickActions(float railW, float railH)
{
    (void)railH;
    auto& core = Core::PresentationCore::Get();
    bool stretchOn = core.GetStretchToFill();
    bool isMuted   = core.GetLiveMute();

    ImVec4 baseFill     = ToVec4(DS::BtnDefaultFill);
    ImVec4 hoverClear   = ToVec4(DS::AccentColorDim);
    ImVec4 hoverStop    = ToVec4((DS::DangerColor & 0x00FFFFFFu) | (89u << 24));
    ImVec4 activeStretch= ToVec4((DS::AccentColor & 0x00FFFFFFu) | (140u << 24));
    ImVec4 hoverMute    = ToVec4(DS::DangerColor);
    ImVec4 activeMute   = Brighten(ToVec4(DS::DangerColor), 0.12f);
    ImVec4 textPrimary  = ToVec4(DS::TextPrimary);
    ImVec4 textDanger   = ToVec4(DS::DangerColor);

    struct ActionDef {
        const char* id;
        const char* icon;
        const char* fallbackGlyph;
        const char* tooltip;
        ImVec4      hoverColor;
        ImVec4      activeColor;
        bool        toggledOn;
        ImVec4      tint;
    };

    ActionDef actions[5] = {
        { "vaClearText", "cleaning_services", "Lim", "Limpiar texto",
          hoverClear, baseFill, false, textPrimary },
        { "vaClearBg",   "delete",            "BG",  "Quitar fondo",
          hoverStop,  baseFill, false, textPrimary },
        { "vaStretch",   stretchOn ? "original_screen" : "fit_screen", stretchOn ? "1:1" : "Fit",
          "Alternar proporción", hoverClear, activeStretch, stretchOn, textPrimary },
        { "vaMute",      isMuted ? "volume_off" : "volume_up", isMuted ? "Mute" : "Vol",
          "Mutear / Desmutear audio vivo", isMuted ? hoverMute : hoverClear, activeMute, isMuted,
          isMuted ? textDanger : textPrimary },
        { "vaPrefs",     "settings", "Cfg", "Abrir preferencias",
          hoverClear, baseFill, false, textPrimary },
    };

    // Celdas de ancho completo, pegadas unas a otras (separadas solo por la
    // línea de 1px que dibuja QuickActionButton) — look de toolbar plano,
    // no de tarjetas sueltas.
    const ImVec2 cellSize(railW, 34.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::Dummy(ImVec2(railW, 1.0f));

    for (int i = 0; i < 5; i++)
    {
        if (i == 4) ImGui::Dummy(ImVec2(railW, 10.0f)); // separa "Ajustes" del resto

        if (QuickActionButton(actions[i].id, actions[i].icon, actions[i].fallbackGlyph, actions[i].tooltip,
                               cellSize, baseFill, actions[i].hoverColor, actions[i].activeColor,
                               actions[i].tint, actions[i].toggledOn))
        {
            if (i == 0)      core.ClearLayer2();
            else if (i == 1) core.StopBackgroundMedia();
            else if (i == 2) core.SetStretchToFill(!stretchOn);
            else if (i == 3) core.SetLiveMute(!isMuted);
            else if (i == 4 && m_UIManager) m_UIManager->RequestSettings();
        }
    }

    ImGui::PopStyleVar();
}

void ViewPanel::RenderContent(float panelW, float panelH)
{
    auto& core  = ProyecThor::Core::PresentationCore::Get();
    auto  state = core.GetState();

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // ── 1. Resolución de referencia del proyector ────────────────────────
    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

    float srcW = 1920.0f;
    float srcH = 1080.0f;

    if (monitors && monitorCount > 0 && state.targetMonitorIndex >= 0 &&
        state.targetMonitorIndex < monitorCount)
    {
        const GLFWvidmode* mode = glfwGetVideoMode(monitors[state.targetMonitorIndex]);
        if (mode && mode->width > 0 && mode->height > 0)
        {
            srcW = (float)mode->width;
            srcH = (float)mode->height;
        }
    }

    // ── 2. Calcular "Lo justo y necesario" ────────────────────────────────
    float srcRatio = srcW / srcH;
    float drawW = panelW;
    float drawH = panelW / srcRatio;

    // Si el alto calculado supera el alto disponible, ajustamos en base al alto
    if (drawH > panelH)
    {
        drawH = panelH;
        drawW = panelH * srcRatio;
    }

    // Centrar horizontal y verticalmente desplazando el cursor interno de ImGui
    float offsetX = (panelW - drawW) * 0.5f;
    float offsetY = (panelH - drawH) * 0.5f;

    if (offsetX > 0.0f) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
    }
    if (offsetY > 0.0f) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offsetY);
    }

    // Puntos exactos del área de dibujo
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1 = ImVec2(p0.x + drawW, p0.y + drawH);

    // ── 3. Fondo de video / Estado Inactivo ───────────────────────────────
    if (!state.isProjecting)
    {
        dl->AddRectFilled(p0, p1, IM_COL32(8, 9, 16, 255));

        const char* msg     = "Sin proyeccion activa";
        ImVec2      msgSize = ImGui::CalcTextSize(msg);
        dl->AddText(
            ImVec2(p0.x + (drawW - msgSize.x) * 0.5f,
                   p0.y + (drawH - msgSize.y) * 0.5f),
            IM_COL32(60, 65, 90, 255),
            msg);

        dl->AddRect(p0, p1, IM_COL32(40, 44, 64, 255), 0.0f, 0, 1.0f);

        // REGISTRAMOS SOLO EL ESPACIO QUE USAMOS
        ImGui::Dummy(ImVec2(drawW, drawH));
        return;
    }

    // Si está proyectando
    if (state.bgType == Core::PresentationState::BackgroundType::Video)
    {
        void* texID = core.GetProcessedBackgroundTexture((int)drawW, (int)drawH);
        if (texID)
            dl->AddImage(texID, p0, p1, ImVec2(0, 0), ImVec2(1, 1));
        else
            dl->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, 255));
    }
    else {
         // Fondo base si proyecta algo que no es video (como imágenes o color sólido)
         dl->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, 255));
    }

    // ── 4. Texto proyectado ───────────────────────────────────────────────
    if (state.showText && !state.currentText.empty())
    {
        float scale = drawW / srcW;

        float marginL = state.margins[0] * scale;
        float marginT = state.margins[1] * scale;
        float marginR = state.margins[2] * scale;
        float marginB = state.margins[3] * scale;

        float boxW = std::max(10.0f, drawW - marginL - marginR);
        float boxH = std::max(10.0f, drawH - marginT - marginB);
        
        // Usamos p0.x y p0.y en lugar del drawX/drawY antiguo
        float boxX = p0.x + marginL;
        float boxY = p0.y + marginT;

        float fontSize = state.textSize * scale;

        std::string fontName = core.GetActiveFontName();
        ImFont* font = core.GetImGuiFont(fontName, fontSize);
        if (!font) font = ImGui::GetFont();

        if (state.autoScale)
        {
            while (fontSize > 4.0f)
            {
                ImVec2 ts = font->CalcTextSizeA(
                    fontSize, FLT_MAX, boxW, state.currentText.c_str());
                if (ts.y <= boxH) break;
                fontSize -= 1.0f;
            }
        }

        ImVec2 textBlock = font->CalcTextSizeA(
            fontSize, FLT_MAX, boxW, state.currentText.c_str());

        float textX = boxX;
        if (state.textAlignment == 1)
            textX += (boxW - textBlock.x) * 0.5f;
        else if (state.textAlignment == 2)
            textX += (boxW - textBlock.x);

        float textY = boxY;
        if (state.vAlignment == 1)
            textY += (boxH - textBlock.y) * 0.5f;
        else if (state.vAlignment == 2)
            textY += (boxH - textBlock.y);

        dl->PushClipRect(p0, p1, true);

        ImU32 shadowCol = IM_COL32(0, 0, 0, 180);
        ImU32 textCol   = ImGui::ColorConvertFloat4ToU32(
            ImVec4(state.textColor[0], state.textColor[1],
                   state.textColor[2], state.textColor[3]));

        bool isSong = (core.PeekSelection().type == Core::ItemType::Song);
        if (isSong && state.textAlignment == 1)
        {
            float lineH = font->CalcTextSizeA(fontSize, FLT_MAX, boxW, "A").y;

            float startY = boxY;
            if (state.vAlignment == 1)
                startY += (boxH - textBlock.y) * 0.5f;
            else if (state.vAlignment == 2)
                startY += (boxH - textBlock.y);

            float  curY     = startY;
            size_t startPos = 0;
            size_t endPos   = state.currentText.find('\n');

            while (startPos != std::string::npos)
            {
                std::string line =
                    state.currentText.substr(startPos, endPos - startPos);
                if (!line.empty() && line.back() == '\r') line.pop_back();

                if (!line.empty())
                {
                    ImVec2 lSize =
                        font->CalcTextSizeA(fontSize, FLT_MAX, boxW, line.c_str());
                    float lx = boxX + (boxW - lSize.x) * 0.5f;

                    dl->AddText(font, fontSize,
                        ImVec2(lx + 2.0f * scale, curY + 2.0f * scale),
                        shadowCol, line.c_str());
                    dl->AddText(font, fontSize,
                        ImVec2(lx, curY), textCol, line.c_str());
                }

                curY += lineH;
                if (endPos == std::string::npos) break;
                startPos = endPos + 1;
                endPos   = state.currentText.find('\n', startPos);
            }
        }
        else
        {
            dl->AddText(font, fontSize,
                ImVec2(textX + 2.0f * scale, textY + 2.0f * scale),
                shadowCol, state.currentText.c_str(), nullptr, boxW);
            dl->AddText(font, fontSize,
                ImVec2(textX, textY), textCol,
                state.currentText.c_str(), nullptr, boxW);
        }

        dl->PopClipRect();
    }

    // ── 5. Borde y UI adicional ───────────────────────────────────────────
    dl->AddRect(p0, p1, IM_COL32(50, 55, 80, 180), 0.0f, 0, 1.0f);

    // ── 6. Indicador de Red (Solo cuando transmite) ───────────────────────
    if (state.isProjecting && state.isStreamingNet)
    {
        // Puedes cambiar "WIFI" por un icono de FontAwesome si tu proyecto lo soporta (ej. u8"\uf1eb")
        const char* wifiStr = "online"; 
        ImVec2 wifiSize = ImGui::CalcTextSize(wifiStr);
        
        // Posicionado en la esquina superior derecha del área de proyección
        ImVec2 wifiPos = ImVec2(p1.x - wifiSize.x - 12.0f, p0.y + 8.0f);

        // Fondo oscuro semitransparente para que contraste con cualquier video/imagen de fondo
        dl->AddRectFilled(
            ImVec2(wifiPos.x - 6.0f, wifiPos.y - 4.0f),
            ImVec2(wifiPos.x + wifiSize.x + 6.0f, wifiPos.y + wifiSize.y + 4.0f),
            IM_COL32(0, 0, 0, 160), 4.0f);

        // Dibujar el icono
        dl->AddText(wifiPos, IM_COL32(0, 255, 100, 255), wifiStr);
    }

    // Registramos que solo consumimos el tamaño de la pantalla
    ImGui::Dummy(ImVec2(drawW, drawH));
}

} // namespace ProyecThor::UI