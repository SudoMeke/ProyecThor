#include "ViewPanel.h"
#include "backend/core/PresentationCore.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <GLFW/glfw3.h>

namespace ProyecThor::UI {

void ViewPanel::Render()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.04f, 0.04f, 0.06f, 1.0f));

    bool visible = ImGui::Begin("Vista en Vivo");

    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(1);

    if (visible)
    {
        ImVec2 contentSize = ImGui::GetContentRegionAvail();
        if (contentSize.x > 8.0f && contentSize.y > 8.0f)
            RenderContent(contentSize.x, contentSize.y);
    }

    ImGui::End();
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