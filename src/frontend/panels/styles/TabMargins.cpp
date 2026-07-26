#include "TabMargins.h"
#include <imgui.h>
#include <string>
#include <algorithm>
#include <cmath>

namespace ProyecThor::UI {

void TabMargins::Render(StyleData& data, float colWidth, ImDrawList* dl) {
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    CanvaStyleEditor::Badge("MARGENES", CanvaPalette::Accent);
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    CanvaStyleEditor::SectionLabel("En pixeles, referencia a resolucion 1920x1080");
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    RenderMarginInputs(data, colWidth);

    ImGui::Dummy(ImVec2(0.0f, 16.0f));

    RenderMarginDiagram(data, colWidth, dl);

    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    RenderAutoScaleCheckbox(data);
}

void TabMargins::RenderMarginInputs(StyleData& data, float colWidth) {
    const char* marginNames[] = { "Izquierda", "Arriba", "Derecha", "Abajo" };
    float halfW = (colWidth - 8.0f) * 0.5f;

    for (int m = 0; m < 4; m++) {
        if (m % 2 != 0) ImGui::SameLine(0.0f, 8.0f);

        ImGui::BeginGroup();

        ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
        ImGui::Text("%s", marginNames[m]);
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_FrameBg,        CanvaPalette::Surface1);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, CanvaPalette::Surface2);
        ImGui::PushStyleColor(ImGuiCol_Border,         CanvaPalette::Border);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,  5.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.5f);
        ImGui::SetNextItemWidth(halfW);

        std::string dragId = "##mrg" + std::to_string(m);
        ImGui::DragFloat(dragId.c_str(), &data.margins[m], 2.0f, 0.0f, 900.0f, "%.0f px");

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
        ImGui::EndGroup();
    }
}

void TabMargins::RenderMarginDiagram(StyleData& data, float colWidth, ImDrawList* dl) {
    ImVec2 p    = ImGui::GetCursorScreenPos();
    float diagW = colWidth;
    float diagH = 100.0f;
    float scale = diagW / 1920.0f;

    float mL = data.margins[0] * scale;
    float mT = data.margins[1] * scale;
    float mR = data.margins[2] * scale;
    float mB = data.margins[3] * scale;

    dl->AddRectFilled(p, ImVec2(p.x + diagW, p.y + diagH),
        CanvaPalette::ToU32(CanvaPalette::Surface1), 4.0f);

    float sx = p.x + mL;
    float sy = p.y + mT;
    float sw = std::max(4.0f, diagW - mL - mR);
    float sh = std::max(4.0f, diagH - mT - mB);

    ImU32 accentFill = CanvaPalette::ToU32(ImVec4(
        CanvaPalette::Accent.x, CanvaPalette::Accent.y, CanvaPalette::Accent.z, 0.12f));
    ImU32 accentLine = CanvaPalette::ToU32(ImVec4(
        CanvaPalette::Accent.x, CanvaPalette::Accent.y, CanvaPalette::Accent.z, 0.65f));

    dl->AddRectFilled(ImVec2(sx, sy), ImVec2(sx + sw, sy + sh), accentFill);
    dl->AddRect(ImVec2(sx, sy), ImVec2(sx + sw, sy + sh), accentLine, 3.0f, 0, 1.2f);

    const char* marginSigns[] = { "L", "T", "R", "B" };
    ImU32       labelCol      = CanvaPalette::ToU32(CanvaPalette::TextMuted);

    if (mL > 8.0f) {
        dl->AddText(ImGui::GetFont(), 10.0f,
            ImVec2(p.x + mL * 0.5f - 3.0f, p.y + diagH * 0.5f - 5.0f),
            labelCol, marginSigns[0]);
    }
    if (mT > 8.0f) {
        dl->AddText(ImGui::GetFont(), 10.0f,
            ImVec2(p.x + diagW * 0.5f, p.y + mT * 0.3f),
            labelCol, marginSigns[1]);
    }
    if (mR > 8.0f) {
        dl->AddText(ImGui::GetFont(), 10.0f,
            ImVec2(p.x + diagW - mR * 0.5f - 3.0f, p.y + diagH * 0.5f - 5.0f),
            labelCol, marginSigns[2]);
    }
    if (mB > 8.0f) {
        dl->AddText(ImGui::GetFont(), 10.0f,
            ImVec2(p.x + diagW * 0.5f, p.y + diagH - mB * 0.6f),
            labelCol, marginSigns[3]);
    }

    ImGui::Dummy(ImVec2(0.0f, diagH + 6.0f));
}

void TabMargins::RenderAutoScaleCheckbox(StyleData& data) {
    ImGui::PushStyleColor(ImGuiCol_CheckMark,      CanvaPalette::Accent);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        CanvaPalette::Surface1);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, CanvaPalette::Surface2);
    ImGui::Checkbox("Auto-reducir si el texto no cabe", &data.autoScale);
    ImGui::PopStyleColor(3);

    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
    ImGui::TextWrapped(
        "Reduce el tamanio automaticamente hasta que el bloque de texto entre "
        "dentro de la zona segura.");
    ImGui::PopStyleColor();
}

} // namespace ProyecThor::UI
