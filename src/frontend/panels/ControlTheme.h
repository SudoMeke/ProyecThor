#pragma once
#include <imgui.h>

namespace ProyecThor::Settings { struct ThemeSettings; }

namespace ProyecThor::UI::ControlTheme {

// Paleta del ControlPanel. No son constexpr: se recalculan en Sync()
// a partir del tema activo, igual que DS::/MonitorTheme::/HubTheme::.

inline ImVec4 PanelBgTop     = { 0.06f, 0.06f, 0.08f, 1.00f };
inline ImVec4 PanelBgBottom  = { 0.03f, 0.03f, 0.04f, 1.00f };
inline ImVec4 Divider        = { 1.00f, 1.00f, 1.00f, 0.04f };

inline ImVec4 TextPrimary    = { 0.85f, 0.85f, 0.90f, 1.00f };
inline ImVec4 TextDim        = { 0.50f, 0.50f, 0.60f, 1.00f };

inline ImVec4 NoMonitorDot   = { 1.00f, 0.40f, 0.40f, 1.00f };
inline ImVec4 NoMonitorText  = { 1.00f, 0.50f, 0.50f, 0.90f };

inline ImVec4 LiveDot        = { 0.95f, 0.35f, 0.45f, 1.00f };
inline ImVec4 IdleDot        = { 0.20f, 0.80f, 0.60f, 1.00f };

inline ImVec4 ComboBg        = { 0.09f, 0.09f, 0.12f, 1.00f };
inline ImVec4 ComboBgHover   = { 0.12f, 0.12f, 0.16f, 1.00f };
inline ImVec4 ComboPopupBg   = { 0.07f, 0.07f, 0.09f, 1.00f };

// Botón grande de proyectar
inline ImVec4 ProjectHalo      = { 0.85f, 0.25f, 0.35f, 1.00f };
inline ImVec4 ProjectBtnLive   = { 0.75f, 0.20f, 0.30f, 1.00f };
inline ImVec4 ProjectBtnIdle   = { 0.20f, 0.25f, 0.45f, 1.00f };
inline ImVec4 ProjectBtnOff    = { 0.12f, 0.12f, 0.15f, 1.00f };
inline ImVec4 ProjectIconOn    = { 0.95f, 0.95f, 0.98f, 1.00f };
inline ImVec4 ProjectIconOff   = { 0.40f, 0.40f, 0.45f, 1.00f };
inline ImVec4 ProjectLabelLive = { 0.85f, 0.40f, 0.50f, 1.00f };
inline ImVec4 ProjectLabelIdle = { 0.50f, 0.60f, 0.85f, 1.00f };
inline ImVec4 ProjectLabelOff  = { 0.35f, 0.35f, 0.40f, 1.00f };

// Barra de estado
inline ImVec4 StatusBarBgLive  = { 0.15f, 0.05f, 0.08f, 1.00f };
inline ImVec4 StatusBarBgIdle  = { 0.07f, 0.07f, 0.09f, 1.00f };
inline ImVec4 StatusTextLive   = { 0.95f, 0.55f, 0.60f, 1.00f };
inline ImVec4 StatusTextIdle   = { 0.45f, 0.45f, 0.55f, 1.00f };
inline ImVec4 StatusDotIdle    = { 0.35f, 0.35f, 0.45f, 1.00f };

// Fila de acciones (botones circulares)
inline ImVec4 ActionBtnBase       = { 0.10f, 0.10f, 0.13f, 1.00f };
inline ImVec4 ActionBtnHoverClear = { 0.20f, 0.25f, 0.45f, 1.00f };
inline ImVec4 ActionBtnHoverStop  = { 0.35f, 0.15f, 0.18f, 1.00f };
inline ImVec4 ActionBtnHoverMon   = { 0.15f, 0.30f, 0.28f, 1.00f };
inline ImVec4 ActionLabel         = { 0.50f, 0.50f, 0.60f, 1.00f };

void Sync(const ProyecThor::Settings::ThemeSettings& theme);

} // namespace ProyecThor::UI::ControlTheme