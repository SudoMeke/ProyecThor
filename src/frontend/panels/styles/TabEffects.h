#pragma once
#include <imgui.h>
#include "CanvaStyleEditor.h"

namespace ProyecThor::UI {

// TabEffects — tab "Efectos" del editor de estilos.
class TabEffects {
public:
    TabEffects() = default;

    void Render(StyleData& data, float colWidth);

private:
    void RenderEffectCard(const char* title, const ImVec4& accent, float colWidth,
                           bool& enabled, float* color4,
                           float* intensity, const char* intensityLabel);
};

} // namespace ProyecThor::UI
