#include "ControlTheme.h"
#include "SettingsManager.h"

namespace ProyecThor::UI::ControlTheme {

static ImVec4 V(const float* c, float a = 1.0f) {
    return ImVec4(c[0], c[1], c[2], c[3] * a);
}

void Sync(const ProyecThor::Settings::ThemeSettings& t) {
    PanelBgTop    = V(t.base, 1.0f);
    PanelBgBottom = V(t.base, 0.85f);
    Divider       = V(t.textPrimary, 0.06f);

    TextPrimary = V(t.textPrimary);
    TextDim     = V(t.textDim);

    NoMonitorDot  = V(t.danger);
    NoMonitorText = V(t.danger, 0.90f);

    LiveDot = V(t.danger);
    IdleDot = V(t.success);

    ComboBg      = V(t.surface1);
    ComboBgHover = V(t.surface2);
    ComboPopupBg = V(t.surface1, 0.98f);

    ProjectHalo      = V(t.danger);
    ProjectBtnLive   = V(t.danger, 0.85f);
    ProjectBtnIdle   = V(t.accentDim);
    ProjectBtnOff    = V(t.surface1);
    ProjectIconOn    = V(t.textPrimary);
    ProjectIconOff   = V(t.textFaint);
    ProjectLabelLive = V(t.danger, 0.85f);
    ProjectLabelIdle = V(t.accentLight);
    ProjectLabelOff  = V(t.textFaint);

    StatusBarBgLive = V(t.danger, 0.14f);
    StatusBarBgIdle = V(t.surface0);
    StatusTextLive  = V(t.danger, 0.90f);
    StatusTextIdle  = V(t.textDim);
    StatusDotIdle   = V(t.textFaint, 1.0f);

    ActionBtnBase       = V(t.surface1);
    ActionBtnHoverClear = V(t.accentDim);
    ActionBtnHoverStop  = V(t.danger, 0.35f);
    ActionBtnHoverMon   = V(t.success, 0.30f);
    ActionLabel         = V(t.textDim);
}

} // namespace ProyecThor::UI::ControlTheme