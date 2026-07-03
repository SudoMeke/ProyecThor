#pragma once
#include <imgui.h>

namespace ProyecThor::Settings { struct ThemeSettings; }

namespace ProyecThor::UI::HubTheme {

// Paleta del Hub de inicio. No son constexpr: se recalculan en Sync()
// a partir del tema activo, igual que DS:: y MonitorTheme::.
inline ImU32 BgSidebar  = IM_COL32(22, 22, 26, 255);
inline ImU32 BgMain     = IM_COL32(16, 16, 19, 255);
inline ImU32 AccentBlue = IM_COL32(35, 116, 225, 255);
inline ImU32 TextPri    = IM_COL32(230, 230, 230, 255);
inline ImU32 TextMuted  = IM_COL32(120, 120, 126, 255);
inline ImU32 Divider    = IM_COL32(45, 45, 52, 255);

void Sync(const ProyecThor::Settings::ThemeSettings& theme);

} // namespace ProyecThor::UI::HubTheme