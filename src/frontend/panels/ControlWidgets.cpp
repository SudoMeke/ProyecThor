#include "ControlWidgets.h"
#include "DesignSystem.h"
#include <imgui_internal.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <string>
#include <vector>
#include <cmath>

namespace ProyecThor::UI {

// =============================================================================
//  Utilidades de Color
// =============================================================================
ImVec4 ToVec4(ImU32 col) {
    return ImGui::ColorConvertU32ToFloat4(col);
}
ImU32 ColA(ImU32 col, int a) {
    return (col & 0x00FFFFFFu) | (static_cast<ImU32>(std::clamp(a, 0, 255)) << 24);
}
ImVec4 Brighten(const ImVec4& c, float amount) {
    return ImVec4(
        std::clamp(c.x + amount, 0.0f, 1.0f),
        std::clamp(c.y + amount, 0.0f, 1.0f),
        std::clamp(c.z + amount, 0.0f, 1.0f),
        c.w);
}

// =============================================================================
//  VectorIconButton
// =============================================================================
bool VectorIconButton(const char* id, DrawIconFn drawIcon, const char* tooltip,
                      ImVec2 size, ImVec4 bgColor, ImVec4 hoverColor,
                      ImVec4 activeColor, ImVec4 iconColor, bool toggledOn)
{
    ImVec4 restColor = toggledOn ? activeColor : bgColor;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);
    ImGui::PushStyleColor(ImGuiCol_Button,        restColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  activeColor);

    std::string label = std::string("##") + id;
    bool clicked = ImGui::Button(label.c_str(), size);

    ImVec2 bMin = ImGui::GetItemRectMin();
    ImVec2 bMax = ImGui::GetItemRectMax();
    ImVec2 center = { (bMin.x + bMax.x) * 0.5f, (bMin.y + bMax.y) * 0.5f };
    float  radius = std::min(size.x, size.y) * 0.32f;
    if (drawIcon)
        drawIcon(ImGui::GetWindowDrawList(), center, radius, ImGui::ColorConvertFloat4ToU32(iconColor));

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        ImGui::SetTooltip("%s", tooltip);

    return clicked;
}

bool IconLabelButton(const char* id, const char* label, DrawIconFn icon, ImVec2 size,
                     ImVec4 bgColor, ImVec4 hoverColor, ImVec4 activeColor, ImVec4 textColor)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
    ImGui::PushStyleColor(ImGuiCol_Button,        bgColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  activeColor);
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // el label lo dibujamos nosotros

    std::string bid = std::string("##") + id;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::Button(bid.c_str(), size);

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();

    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImU32       col = ImGui::ColorConvertFloat4ToU32(textColor);
    float       iconBoxSide = size.y;

    if (icon) {
        ImVec2 iconCenter = { pos.x + iconBoxSide * 0.5f, pos.y + size.y * 0.5f };
        icon(dl, iconCenter, size.y * 0.24f, col);
    }

    ImVec2 textSz  = ImGui::CalcTextSize(label);
    ImVec2 textPos = { pos.x + iconBoxSide, pos.y + (size.y - textSz.y) * 0.5f };
    dl->AddText(textPos, col, label);

    return clicked;
}

// =============================================================================
//  Iconos
// =============================================================================
namespace ControlIcons {

void DrawPlay(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddTriangleFilled(
        { c.x - r * 0.55f, c.y - r * 0.75f },
        { c.x - r * 0.55f, c.y + r * 0.75f },
        { c.x + r * 0.80f, c.y },
        col);
}

void DrawStop(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    dl->AddRectFilled({ c.x - r * 0.6f, c.y - r * 0.6f }, { c.x + r * 0.6f, c.y + r * 0.6f },
                       col, r * 0.18f);
}

void DrawChevronLeft(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    float th = std::max(1.4f, r * 0.26f);
    dl->PathLineTo({ c.x + r * 0.35f, c.y - r * 0.6f });
    dl->PathLineTo({ c.x - r * 0.4f,  c.y });
    dl->PathLineTo({ c.x + r * 0.35f, c.y + r * 0.6f });
    dl->PathStroke(col, ImDrawFlags_None, th);
}

void DrawChevronRight(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    float th = std::max(1.4f, r * 0.26f);
    dl->PathLineTo({ c.x - r * 0.35f, c.y - r * 0.6f });
    dl->PathLineTo({ c.x + r * 0.4f,  c.y });
    dl->PathLineTo({ c.x - r * 0.35f, c.y + r * 0.6f });
    dl->PathStroke(col, ImDrawFlags_None, th);
}

void DrawBroadcast(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    // Antena tipo "wifi/broadcast": punto + dos arcos abriendo hacia arriba.
    ImVec2 base = { c.x, c.y + r * 0.55f };
    float th = std::max(1.2f, r * 0.16f);
    dl->AddCircleFilled(base, r * 0.14f, col);
    dl->PathArcTo(base, r * 0.55f, IM_PI * 1.22f, IM_PI * 1.78f, 14);
    dl->PathStroke(col, ImDrawFlags_None, th);
    dl->PathArcTo(base, r * 0.95f, IM_PI * 1.22f, IM_PI * 1.78f, 16);
    dl->PathStroke(col, ImDrawFlags_None, th);
}

void DrawScreenCast(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    // Pantalla publica + ondas de transmision en la esquina.
    float w = r * 1.5f, h = r * 1.0f;
    float th = std::max(1.2f, r * 0.16f);
    ImVec2 tl = { c.x - w * 0.5f, c.y - h * 0.55f };
    ImVec2 br = { c.x + w * 0.5f, c.y + h * 0.15f };
    dl->AddRect(tl, br, col, r * 0.10f, ImDrawFlags_RoundCornersAll, th);
    dl->AddLine({ c.x, br.y }, { c.x, br.y + r * 0.28f }, col, th);
    dl->AddLine({ c.x - w * 0.28f, br.y + r * 0.30f }, { c.x + w * 0.28f, br.y + r * 0.30f }, col, th);

    ImVec2 waveOrigin = { br.x - r * 0.05f, tl.y + r * 0.05f };
    dl->PathArcTo(waveOrigin, r * 0.30f, IM_PI * 1.5f, IM_PI * 2.0f, 10);
    dl->PathStroke(col, ImDrawFlags_None, th * 0.85f);
    dl->PathArcTo(waveOrigin, r * 0.55f, IM_PI * 1.5f, IM_PI * 2.0f, 12);
    dl->PathStroke(col, ImDrawFlags_None, th * 0.85f);
}

void DrawRoute(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    // Flecha entrando a una pantalla chica — "enrutamiento de pantallas".
    float th = std::max(1.2f, r * 0.18f);
    ImVec2 scrTl = { c.x + r * 0.05f, c.y - r * 0.45f };
    ImVec2 scrBr = { c.x + r * 0.85f, c.y + r * 0.30f };
    dl->AddRect(scrTl, scrBr, col, r * 0.08f, ImDrawFlags_RoundCornersAll, th);

    float arrowY = c.y - r * 0.08f;
    dl->AddLine({ c.x - r * 0.85f, arrowY }, { c.x - r * 0.05f, arrowY }, col, th);
    dl->AddTriangleFilled(
        { c.x - r * 0.25f, arrowY - r * 0.28f },
        { c.x - r * 0.25f, arrowY + r * 0.28f },
        { c.x + r * 0.05f, arrowY },
        col);
}

void DrawQuality(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    // Tres sliders verticales (calidad/mezcla).
    float th = std::max(1.2f, r * 0.16f);
    const float xs[3]    = { -0.5f, 0.0f, 0.5f };
    const float knobY[3] = { 0.18f, -0.28f, 0.05f };
    for (int i = 0; i < 3; i++) {
        float x = c.x + xs[i] * r;
        dl->AddLine({ x, c.y - r * 0.75f }, { x, c.y + r * 0.75f }, col, th);
        dl->AddCircleFilled({ x, c.y + knobY[i] * r }, r * 0.16f, col, 12);
    }
}

void DrawStageMonitor(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    // Monitor de confianza con una "grilla" adentro (celdas de contenido).
    float w = r * 1.5f, h = r * 1.05f;
    float th = std::max(1.2f, r * 0.14f);
    ImVec2 tl = { c.x - w * 0.5f, c.y - h * 0.55f };
    ImVec2 br = { c.x + w * 0.5f, c.y + h * 0.15f };
    dl->AddRect(tl, br, col, r * 0.08f, ImDrawFlags_RoundCornersAll, th);
    dl->AddLine({ c.x, tl.y }, { c.x, br.y }, col, th * 0.8f);
    dl->AddLine({ tl.x, c.y - h * 0.02f }, { br.x, c.y - h * 0.02f }, col, th * 0.8f);
    dl->AddLine({ c.x, br.y }, { c.x, br.y + r * 0.28f }, col, th);
    dl->AddLine({ c.x - w * 0.28f, br.y + r * 0.30f }, { c.x + w * 0.28f, br.y + r * 0.30f }, col, th);
}

} // namespace ControlIcons

// =============================================================================
//  Tarjetas
// =============================================================================
bool BeginCard(const char* id, float minHeight)
{
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    float  w  = ImGui::GetContentRegionAvail().x;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, DS::RadiusMedium);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.f, 14.f));

    bool open = ImGui::BeginChild(id,
                             minHeight > 0.0f ? ImVec2(0.f, minHeight) : ImVec2(0.f, 0.f),
                             true,
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImVec2 p1 = ImVec2(p0.x + w, p0.y + ImGui::GetWindowSize().y);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilledMultiColor(p0, p1, DS::GlassFillTop, DS::GlassFillTop, DS::GlassFillBot, DS::GlassFillBot);
    dl->AddRect(p0, p1, DS::GlassBorder, DS::RadiusMedium, 0, 1.0f);

    float hx0 = p0.x + DS::RadiusMedium, hx1 = p1.x - DS::RadiusMedium;
    if (hx1 > hx0) {
        dl->AddLine(ImVec2(hx0, p0.y + 1.0f), ImVec2(hx1, p0.y + 1.0f), DS::GlassHighlight, 1.0f);
        dl->AddLine(ImVec2(hx0, p1.y - 1.0f), ImVec2(hx1, p1.y - 1.0f), IM_COL32(0, 0, 0, 255), 1.0f);
    }

    return open;
}

void EndCard()
{
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

// =============================================================================
//  DetectCurrentMonitorIndex
// =============================================================================
int DetectCurrentMonitorIndex()
{
    GLFWwindow* win = glfwGetCurrentContext();
    if (!win) return -1;

    int wx, wy, ww, wh;
    glfwGetWindowPos(win, &wx, &wy);
    glfwGetWindowSize(win, &ww, &wh);
    const int cx = wx + ww / 2;
    const int cy = wy + wh / 2;

    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

    for (int i = 0; i < monitorCount; i++) {
        int mx, my;
        glfwGetMonitorPos(monitors[i], &mx, &my);
        if (const GLFWvidmode* vm = glfwGetVideoMode(monitors[i])) {
            if (cx >= mx && cx < mx + vm->width && cy >= my && cy < my + vm->height)
                return i;
        }
    }
    return -1;
}

// =============================================================================
//  MonitorSelector
// =============================================================================
static constexpr float kArrowBtnSize = 24.0f;

void MonitorSelector(const char* idPrefix, int selected, int monitorCountOverride,
                     bool includeLAN, int currentAppMonitor,
                     std::function<void(int)> onCycle,
                     std::function<void(int)> onPick)
{
    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    if (monitorCountOverride >= 0) monitorCount = std::min(monitorCount, monitorCountOverride);

    float rowW = ImGui::GetContentRegionAvail().x;
    const float comboW = rowW - 2.0f * (kArrowBtnSize + 6.0f);

    std::string prevId = std::string(idPrefix) + "Prev";
    std::string nextId = std::string(idPrefix) + "Next";
    std::string comboId = std::string("##") + idPrefix + "sel";

    ImVec4 arrowBg   = ToVec4(DS::BtnDefaultFill);
    ImVec4 arrowText = ToVec4(DS::TextSecondary);

    if (VectorIconButton(prevId.c_str(), ControlIcons::DrawChevronLeft, "Opcion anterior",
                        ImVec2(kArrowBtnSize, kArrowBtnSize),
                        arrowBg, Brighten(arrowBg, 0.05f), Brighten(arrowBg, 0.1f), arrowText))
    {
        onCycle(-1);
    }

    ImGui::SameLine(0.0f, 6.0f);

    static thread_local std::vector<std::string> labels;
    static thread_local std::vector<const char*> ptrs;
    labels.clear(); ptrs.clear();
    for (int i = 0; i < monitorCount; i++) {
        std::string l = "Pantalla " + std::to_string(i + 1) + ": " + glfwGetMonitorName(monitors[i]);
        if (i == currentAppMonitor) l += " (este monitor)";
        labels.push_back(std::move(l));
    }
    if (includeLAN)
        labels.push_back("Red (LAN) - navegador/celular");
    for (const auto& l : labels) ptrs.push_back(l.c_str());

    const int totalItems = (int)ptrs.size();
    int sel = std::clamp(selected, 0, std::max(0, totalItems - 1));

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ToVec4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ToVec4(DS::BtnHoverFill));
    ImGui::PushStyleColor(ImGuiCol_PopupBg,        ToVec4(DS::GlassFillTop));
    ImGui::PushStyleColor(ImGuiCol_Border,         ToVec4(DS::GlassBorder));

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(10.0f, 6.0f));

    ImGui::SetNextItemWidth(comboW);
    if (ImGui::Combo(comboId.c_str(), &sel, ptrs.data(), totalItems))
        onPick(sel);

    if (ImGui::IsItemHovered()) {
        if (sel < monitorCount) {
            if (const GLFWvidmode* vm = glfwGetVideoMode(monitors[sel]))
                ImGui::SetTooltip("%dx%d", vm->width, vm->height);
        } else if (includeLAN) {
            ImGui::SetTooltip("Cualquier dispositivo en la misma red WiFi podra verlo desde su navegador.");
        }
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);

    ImGui::SameLine(0.0f, 6.0f);

    if (VectorIconButton(nextId.c_str(), ControlIcons::DrawChevronRight, "Opcion siguiente",
                        ImVec2(kArrowBtnSize, kArrowBtnSize),
                        arrowBg, Brighten(arrowBg, 0.05f), Brighten(arrowBg, 0.1f), arrowText))
    {
        onCycle(1);
    }
}

} // namespace ProyecThor::UI
