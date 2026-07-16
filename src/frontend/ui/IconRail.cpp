#include "IconRail.h"
#include <imgui_internal.h>
#include <cmath>
#include <string>

namespace ProyecThor::UI {

static float Lerp(float a, float b, float t) { return a + (b - a) * t; }

static void RenderVertical(const IconRailItem* items, int count, int& currentIndex,
                            const float (*categoryColor)[4])
{
    ImDrawList*  dl       = ImGui::GetWindowDrawList();
    const float  railW    = ImGui::GetContentRegionAvail().x;
    const float  winH     = ImGui::GetWindowHeight();
    const ImVec2 winPos   = ImGui::GetWindowPos();

    dl->AddRectFilled(winPos, { winPos.x + railW, winPos.y + winH },
                      IM_COL32(11, 11, 20, 255));

    ImGui::Dummy({ railW, 8.0f });

    constexpr float btnGapY  = 2.0f;
    constexpr float rounding = 8.0f;
    const float     btnH    = 64.0f;
    const float     iconSz  = std::floor(btnH * 0.38f);

    ImGuiStorage* storage = ImGui::GetStateStorage();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, btnGapY));

    for (int i = 0; i < count; i++)
    {
        const auto& item   = items[i];
        const bool  active = (currentIndex == item.index);
        const float* cc    = categoryColor[i];
        const ImU32 accent = ImGui::ColorConvertFloat4ToU32(ImVec4(cc[0], cc[1], cc[2], cc[3]));

        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImVec2 bMin   = cursor;
        ImVec2 bMax   = { cursor.x + railW, cursor.y + btnH };

        ImGuiID hovId = ImGui::GetID(item.label);
        float*  pT    = storage->GetFloatRef(hovId ^ 0xABCD1234u, 0.0f);
        bool hovered  = ImGui::IsMouseHoveringRect(bMin, bMax, false);
        *pT = Lerp(*pT, hovered ? 1.0f : 0.0f, ImGui::GetIO().DeltaTime * 14.0f);
        float t = *pT;

        if (active) {
            ImVec4 ac = ImGui::ColorConvertU32ToFloat4(accent);
            ac.w = 0.12f;
            dl->AddRectFilled(bMin, bMax, ImGui::ColorConvertFloat4ToU32(ac), rounding);
        } else if (t > 0.01f) {
            dl->AddRectFilled(bMin, bMax, IM_COL32(255, 255, 255, (int)(t * 14.f)), rounding);
        }

        // Barra lateral izquierda (indicador de seleccion)
        {
            float barH     = btnH * 0.60f * (active ? 1.0f : t);
            float barY0    = cursor.y + (btnH - barH) * 0.5f;
            float barAlpha = active ? 1.0f : t * 0.55f;
            ImVec4 ac      = ImGui::ColorConvertU32ToFloat4(accent);
            ac.w           = barAlpha;
            dl->AddRectFilled({ bMin.x, barY0 }, { bMin.x + 3.0f, barY0 + barH },
                              ImGui::ColorConvertFloat4ToU32(ac), 2.0f);
        }

        ImGui::SetCursorScreenPos(bMin);
        const std::string btnId = std::string("##rail_") + item.label;
        bool clicked = ImGui::InvisibleButton(btnId.c_str(), { railW, btnH });

        {
            float iconBright = active ? 1.0f : Lerp(0.32f, 0.72f, t);
            ImVec4 icF = { iconBright, iconBright, iconBright, 1.0f };
            if (active) {
                ImVec4 ac = ImGui::ColorConvertU32ToFloat4(accent);
                icF.x = Lerp(icF.x, ac.x, 0.35f);
                icF.y = Lerp(icF.y, ac.y, 0.35f);
                icF.z = Lerp(icF.z, ac.z, 0.35f);
                icF.w = 1.0f;
            }

            ImVec2 lblDim       = ImGui::CalcTextSize(item.label);
            float  totalContent = iconSz + 5.0f + lblDim.y;
            float  startY       = cursor.y + (btnH - totalContent) * 0.5f;
            float  iconX        = cursor.x + (railW - iconSz) * 0.5f;

            item.drawIcon(dl, { iconX, startY }, iconSz, ImGui::ColorConvertFloat4ToU32(icF));

            float lblBright = active ? 1.0f : Lerp(0.30f, 0.72f, t);
            ImVec4 lblF = { lblBright, lblBright, lblBright, 1.0f };
            if (active) {
                ImVec4 ac = ImGui::ColorConvertU32ToFloat4(accent);
                lblF.x = Lerp(lblF.x, ac.x, 0.25f);
                lblF.y = Lerp(lblF.y, ac.y, 0.25f);
                lblF.z = Lerp(lblF.z, ac.z, 0.25f);
                lblF.w = 1.0f;
            }

            float lblX = cursor.x + (railW - lblDim.x) * 0.5f;
            float lblY = startY + iconSz + 5.0f;
            dl->AddText({ lblX, lblY }, ImGui::ColorConvertFloat4ToU32(lblF), item.label);
        }

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("%s", item.label);

        if (clicked) currentIndex = item.index;
    }

    ImGui::PopStyleVar();
}

static void RenderHorizontal(const IconRailItem* items, int count, int& currentIndex,
                              const float (*categoryColor)[4])
{
    ImDrawList*  dl     = ImGui::GetWindowDrawList();
    const float  railH  = ImGui::GetContentRegionAvail().y;
    const float  winW   = ImGui::GetWindowWidth();
    const ImVec2 winPos = ImGui::GetWindowPos();

    dl->AddRectFilled(winPos, { winPos.x + winW, winPos.y + railH },
                      IM_COL32(11, 11, 20, 255));

    ImGui::Dummy({ 8.0f, railH });
    ImGui::SameLine(0.0f, 0.0f);

    constexpr float btnGapX  = 2.0f;
    constexpr float rounding = 8.0f;
    const float     btnW    = kIconRailHorizontalItemW;
    const float      iconSz  = std::floor(railH * 0.38f);

    ImGuiStorage* storage = ImGui::GetStateStorage();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(btnGapX, 0.f));

    for (int i = 0; i < count; i++)
    {
        const auto& item   = items[i];
        const bool  active = (currentIndex == item.index);
        const float* cc    = categoryColor[i];
        const ImU32 accent = ImGui::ColorConvertFloat4ToU32(ImVec4(cc[0], cc[1], cc[2], cc[3]));

        if (i > 0) ImGui::SameLine();
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImVec2 bMin   = cursor;
        ImVec2 bMax   = { cursor.x + btnW, cursor.y + railH };

        ImGuiID hovId = ImGui::GetID(item.label);
        float*  pT    = storage->GetFloatRef(hovId ^ 0xABCD1234u, 0.0f);
        bool hovered  = ImGui::IsMouseHoveringRect(bMin, bMax, false);
        *pT = Lerp(*pT, hovered ? 1.0f : 0.0f, ImGui::GetIO().DeltaTime * 14.0f);
        float t = *pT;

        if (active) {
            ImVec4 ac = ImGui::ColorConvertU32ToFloat4(accent);
            ac.w = 0.12f;
            dl->AddRectFilled(bMin, bMax, ImGui::ColorConvertFloat4ToU32(ac), rounding);
        } else if (t > 0.01f) {
            dl->AddRectFilled(bMin, bMax, IM_COL32(255, 255, 255, (int)(t * 14.f)), rounding);
        }

        // Barra inferior (indicador de seleccion, equivalente horizontal de
        // la barra lateral del rail vertical)
        {
            float barW     = btnW * 0.60f * (active ? 1.0f : t);
            float barX0    = cursor.x + (btnW - barW) * 0.5f;
            float barAlpha = active ? 1.0f : t * 0.55f;
            ImVec4 ac      = ImGui::ColorConvertU32ToFloat4(accent);
            ac.w           = barAlpha;
            dl->AddRectFilled({ barX0, bMax.y - 3.0f }, { barX0 + barW, bMax.y },
                              ImGui::ColorConvertFloat4ToU32(ac), 2.0f);
        }

        ImGui::SetCursorScreenPos(bMin);
        const std::string btnId = std::string("##rail_") + item.label;
        bool clicked = ImGui::InvisibleButton(btnId.c_str(), { btnW, railH });

        {
            float iconBright = active ? 1.0f : Lerp(0.32f, 0.72f, t);
            ImVec4 icF = { iconBright, iconBright, iconBright, 1.0f };
            if (active) {
                ImVec4 ac = ImGui::ColorConvertU32ToFloat4(accent);
                icF.x = Lerp(icF.x, ac.x, 0.35f);
                icF.y = Lerp(icF.y, ac.y, 0.35f);
                icF.z = Lerp(icF.z, ac.z, 0.35f);
                icF.w = 1.0f;
            }

            ImVec2 lblDim       = ImGui::CalcTextSize(item.label);
            float  totalContent = iconSz + 5.0f + lblDim.y;
            float  startY       = cursor.y + (railH - totalContent) * 0.5f;
            float  iconX        = cursor.x + (btnW - iconSz) * 0.5f;

            item.drawIcon(dl, { iconX, startY }, iconSz, ImGui::ColorConvertFloat4ToU32(icF));

            float lblBright = active ? 1.0f : Lerp(0.30f, 0.72f, t);
            ImVec4 lblF = { lblBright, lblBright, lblBright, 1.0f };
            if (active) {
                ImVec4 ac = ImGui::ColorConvertU32ToFloat4(accent);
                lblF.x = Lerp(lblF.x, ac.x, 0.25f);
                lblF.y = Lerp(lblF.y, ac.y, 0.25f);
                lblF.z = Lerp(lblF.z, ac.z, 0.25f);
                lblF.w = 1.0f;
            }

            float lblX = cursor.x + (btnW - lblDim.x) * 0.5f;
            float lblY = startY + iconSz + 5.0f;
            dl->AddText({ lblX, lblY }, ImGui::ColorConvertFloat4ToU32(lblF), item.label);
        }

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("%s", item.label);

        if (clicked) currentIndex = item.index;
    }

    ImGui::PopStyleVar();
}

void RenderIconRail(const IconRailItem* items, int count, int& currentIndex,
                     IconRailOrientation orientation, const float (*categoryColor)[4])
{
    if (orientation == IconRailOrientation::Horizontal)
        RenderHorizontal(items, count, currentIndex, categoryColor);
    else
        RenderVertical(items, count, currentIndex, categoryColor);
}

} // namespace ProyecThor::UI
