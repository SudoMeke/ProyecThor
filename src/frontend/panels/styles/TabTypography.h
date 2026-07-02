#pragma once
#include <imgui.h>
#include <string>
#include <vector>
#include <functional>
#include "CanvaStyleEditor.h"

namespace ProyecThor::UI {

class TabTypography {
public:
    using OnFontImportedCallback = std::function<void(const std::string& fontPath)>;

    explicit TabTypography(std::vector<std::string>* fontList,
                           OnFontImportedCallback onFontImported = nullptr);

    void Render(StyleData& data, float colWidth);

private:
    void RenderFontSelector    (StyleData& data, float colWidth);
    void RenderColorPicker     (StyleData& data, float colWidth);
    void RenderSizeSlider      (StyleData& data, float colWidth);
    void RenderAutoScaleCheckbox(StyleData& data);
    void ImportFont();

    std::vector<std::string>* m_FontList       = nullptr;
    OnFontImportedCallback    m_OnFontImported;
    std::string               m_ImportStatus;
    float                     m_ImportMsgTimer  = 0.0f;
};

} // namespace ProyecThor::UI