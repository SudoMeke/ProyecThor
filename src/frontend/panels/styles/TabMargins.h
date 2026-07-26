#pragma once
#include <imgui.h>
#include "CanvaStyleEditor.h"

namespace ProyecThor::UI {

// TabMargins — tab "Margenes" del editor de estilos.
class TabMargins {
public:
    TabMargins() = default;

    void Render(StyleData& data, float colWidth, ImDrawList* dl);

private:
    void RenderMarginInputs(StyleData& data, float colWidth);
    void RenderMarginDiagram(StyleData& data, float colWidth, ImDrawList* dl);
    void RenderAutoScaleCheckbox(StyleData& data);
};

} // namespace ProyecThor::UI
