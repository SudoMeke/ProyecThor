#pragma once
#include <imgui.h>
#include "CanvaStyleEditor.h"

namespace ProyecThor::UI {

class TabAlignment {
public:
    TabAlignment()  = default;
    ~TabAlignment() = default;

    void Render(StyleData& data, float colWidth, ImDrawList* dl);

private:
    void RenderAlignSection(const char* sectionTitle,
                             const char* idPrefix,
                             ImVec4 accentColor,
                             int& hAlign, int& vAlign,
                             float colWidth);
};

} // namespace ProyecThor::UI
