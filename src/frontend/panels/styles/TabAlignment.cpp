#include "TabAlignment.h"
#include <imgui.h>
#include <string>

namespace ProyecThor::UI {

void TabAlignment::Render(StyleData& data, float colWidth, ImDrawList* /*dl*/) {
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    CanvaStyleEditor::Badge("ALINEACION", CanvaPalette::Green);
    ImGui::Dummy(ImVec2(0.0f, 12.0f));

    RenderAlignSection("Base / Preview", "base",
                        CanvaPalette::Accent,
                        data.textAlignment, data.vAlignment, colWidth);

    ImGui::Dummy(ImVec2(0.0f, 14.0f));

    RenderAlignSection("Canciones", "song",
                        CanvaPalette::Green,
                        data.songTextAlignment, data.songVAlignment, colWidth);

    ImGui::Dummy(ImVec2(0.0f, 14.0f));

    RenderAlignSection("Biblia", "bible",
                        CanvaPalette::Gold,
                        data.bibleTextAlignment, data.bibleVAlignment, colWidth);
}

void TabAlignment::RenderAlignSection(const char* sectionTitle,
                                       const char* idPrefix,
                                       ImVec4 accentColor,
                                       int& hAlign, int& vAlign,
                                       float colWidth) {
    ImGui::PushStyleColor(ImGuiCol_Text, accentColor);
    ImGui::TextUnformatted(sectionTitle);
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    // FIXED: Use only the prefix (no visible label text) so IDs are unique
    // across sections. SegmentedButtons appends "##index" internally.
    std::string hPrefix = std::string(idPrefix) + "_h";
    std::string vPrefix = std::string(idPrefix) + "_v";

    CanvaStyleEditor::SectionLabel("Horizontal");
    const char* hLabels[] = { "Izq", "Centro", "Der" };
    CanvaStyleEditor::SegmentedButtons(hPrefix.c_str(), hLabels, 3, &hAlign,
                                        colWidth, 30.0f, accentColor);

    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    CanvaStyleEditor::SectionLabel("Vertical");
    const char* vLabels[] = { "Arr", "Cen", "Aba" };
    CanvaStyleEditor::SegmentedButtons(vPrefix.c_str(), vLabels, 3, &vAlign,
                                        colWidth, 30.0f, accentColor);
}

} // namespace ProyecThor::UI
