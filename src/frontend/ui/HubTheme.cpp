#include "HubTheme.h"
#include "SettingsManager.h"

namespace ProyecThor::UI::HubTheme {

static ImU32 U32(const float* c, float a = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3] * a));
}

void Sync(const ProyecThor::Settings::ThemeSettings& t) {
    BgSidebar  = U32(t.surface0);
    BgMain     = U32(t.base);
    AccentBlue = U32(t.accent);
    TextPri    = U32(t.textPrimary);
    TextMuted  = U32(t.textDim);
    Divider    = U32(t.border);
}

} // namespace ProyecThor::UI::HubTheme