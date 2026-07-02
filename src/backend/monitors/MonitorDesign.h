#pragma once
#include <imgui.h>

namespace ProyecThor::UI::Design {

    // ── Surface ──────────────────────────────────────────────────────────────
    static constexpr ImVec4 k_Bg0           = { 0.055f, 0.055f, 0.060f, 1.00f };
    static constexpr ImVec4 k_Bg1           = { 0.090f, 0.090f, 0.098f, 1.00f };
    static constexpr ImVec4 k_Bg3           = { 0.030f, 0.030f, 0.035f, 1.00f };
    
    // ── Borders ──────────────────────────────────────────────────────────────
    static constexpr ImVec4 k_BorderOuter   = { 0.00f, 0.00f, 0.00f, 0.70f };
    static constexpr ImVec4 k_BorderInner   = { 1.00f, 1.00f, 1.00f, 0.04f };
    static constexpr ImVec4 k_BorderSubtle  = { 0.00f, 0.00f, 0.00f, 0.35f };

    // ── Typography ───────────────────────────────────────────────────────────
    static constexpr ImVec4 k_TextSecondary = { 0.60f, 0.60f, 0.64f, 1.00f };
    static constexpr ImVec4 k_TextDim       = { 0.38f, 0.38f, 0.42f, 1.00f };
    static constexpr ImVec4 k_TextWhite     = { 1.00f, 1.00f, 1.00f, 1.00f };

    // ── Preview (Cyan/Blue) ──────────────────────────────────────────────────
    static constexpr ImVec4 k_PrevAccent    = { 0.20f, 0.60f, 1.00f, 1.00f };
    static constexpr ImVec4 k_PrevAccentDim = { 0.20f, 0.60f, 1.00f, 0.40f };
    static constexpr ImVec4 k_PrevBtn       = { 0.08f, 0.14f, 0.24f, 1.00f };
    static constexpr ImVec4 k_PrevBtnHov    = { 0.12f, 0.22f, 0.38f, 1.00f };
    static constexpr ImVec4 k_PrevBtnAct    = { 0.18f, 0.34f, 0.60f, 1.00f };
    static constexpr ImVec4 k_PrevGrab      = { 0.20f, 0.60f, 1.00f, 1.00f };
    static constexpr ImVec4 k_PrevTrack     = { 0.06f, 0.09f, 0.14f, 1.00f };

    // ── Live / PGM (Red) ─────────────────────────────────────────────────────
    static constexpr ImVec4 k_LiveAccent    = { 1.00f, 0.22f, 0.26f, 1.00f };
    static constexpr ImVec4 k_LiveAccentDim = { 1.00f, 0.22f, 0.26f, 0.40f };
    static constexpr ImVec4 k_LiveBtn       = { 0.20f, 0.06f, 0.08f, 1.00f };
    static constexpr ImVec4 k_LiveBtnHov    = { 0.32f, 0.08f, 0.10f, 1.00f };
    static constexpr ImVec4 k_LiveBtnAct    = { 0.50f, 0.12f, 0.15f, 1.00f };
    static constexpr ImVec4 k_LiveGrab      = { 1.00f, 0.22f, 0.26f, 1.00f };
    static constexpr ImVec4 k_LiveTrack     = { 0.14f, 0.04f, 0.05f, 1.00f };

    // ── Neutral controls ─────────────────────────────────────────────────────
    static constexpr ImVec4 k_NeutBtn       = { 0.16f, 0.16f, 0.18f, 1.00f };
    static constexpr ImVec4 k_NeutBtnHov    = { 0.22f, 0.22f, 0.25f, 1.00f };
    static constexpr ImVec4 k_NeutBtnAct    = { 0.30f, 0.30f, 0.34f, 1.00f };

    // ── Amber / Green / EQ ───────────────────────────────────────────────────
    static constexpr ImVec4 k_AmberAccent   = { 1.00f, 0.72f, 0.10f, 1.00f };
    static constexpr ImVec4 k_AmberBtn      = { 0.18f, 0.13f, 0.02f, 1.00f };
    static constexpr ImVec4 k_AmberBtnHov   = { 0.26f, 0.18f, 0.03f, 1.00f };
    static constexpr ImVec4 k_AmberBtnAct   = { 0.40f, 0.28f, 0.05f, 1.00f };
    static constexpr ImVec4 k_EQ_Green      = { 0.18f, 0.85f, 0.30f, 1.00f };
    static constexpr ImVec4 k_EQ_Yellow     = { 0.98f, 0.82f, 0.00f, 1.00f };
    static constexpr ImVec4 k_EQ_Red        = { 0.94f, 0.18f, 0.14f, 1.00f };

    // ── Layout metrics ───────────────────────────────────────────────────────
    static constexpr float k_R              = 6.0f;
    static constexpr float k_RLg            = 10.0f;
    static constexpr float k_Gap            = 5.0f;
    static constexpr float k_Pad            = 10.0f;
    static constexpr float k_PadLg          = 14.0f;
    static constexpr float k_CenterW        = 106.0f;
    static constexpr float k_MonitorRatio   = 16.0f / 9.0f;
    
    static constexpr float k_TransportH     = 36.0f;
    static constexpr float k_VolumeH        = 30.0f;
    static constexpr float k_Meters_H       = 64.0f;
    static constexpr float k_ControlsH      = 248.0f;
}