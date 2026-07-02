#include "LibrarySidebar.h"
#include "LibraryIcons.h"
#include "LibraryStyles.h"
#include "LibraryHelpers.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <string>
#include <cmath>

// El enum vive en LibraryPanel.h; aqui lo reproducimos como constantes locales
// para no crear una dependencia circular con el header del panel.
// El orden debe coincidir con LibraryCategory.
static constexpr int kCat_Songs     = 0;
static constexpr int kCat_Videos    = 1;
static constexpr int kCat_Images    = 2;
static constexpr int kCat_Bibles    = 3;
static constexpr int kCat_Documents = 4;
static constexpr int kCat_Audio     = 5;

namespace ProyecThor::Library {

void RenderCategoryButtons(LibraryContext& ctx)
{
    using DrawFn = void(*)(ImDrawList*, ImVec2, float, ImU32);

    struct CatDef {
        int         catInt;
        DrawFn      drawIcon;
        const char* label;
        ImU32 glowCol;
        ImU32 bgActive;
        ImU32 bgInactive;
        ImU32 bgHover;
        ImU32 iconActive;
        ImU32 iconInactive;
        ImU32 accentBar;
    };

    static const CatDef k_Cats[] = {
        {
            kCat_Songs, DrawIcon_Music, "Letra",
            IM_COL32( 60, 110, 255,  70), IM_COL32( 18,  40, 110, 255),
            IM_COL32( 12,  16,  36, 220), IM_COL32( 24,  50, 140, 230),
            IM_COL32(160, 195, 255, 255), IM_COL32( 55,  68, 120, 200),
            IM_COL32( 80, 140, 255, 255),
        },
        {
            kCat_Videos, DrawIcon_Play, "Video",
            IM_COL32(220,  60,  60,  70), IM_COL32(100,  18,  18, 255),
            IM_COL32( 36,  10,  10, 220), IM_COL32(130,  28,  28, 230),
            IM_COL32(255, 170, 160, 255), IM_COL32(110,  40,  40, 200),
            IM_COL32(255,  80,  80, 255),
        },
        {
            kCat_Images, DrawIcon_Image, "Imagen",
            IM_COL32( 40, 200,  90,  70), IM_COL32( 12,  70,  28, 255),
            IM_COL32(  8,  26,  14, 220), IM_COL32( 18, 100,  42, 230),
            IM_COL32(160, 255, 185, 255), IM_COL32( 30,  80,  46, 200),
            IM_COL32( 60, 220, 100, 255),
        },
        {
            kCat_Bibles, DrawIcon_Cross, "Biblia",
            IM_COL32(210, 170,  40,  70), IM_COL32( 76,  55,  10, 255),
            IM_COL32( 28,  20,   6, 220), IM_COL32(105,  76,  16, 230),
            IM_COL32(255, 228, 140, 255), IM_COL32(100,  78,  20, 200),
            IM_COL32(220, 170,  40, 255),
        },
        {
            kCat_Documents, DrawIcon_Document, "Doc",
            IM_COL32(160,  80, 240,  70), IM_COL32( 54,  16,  88, 255),
            IM_COL32( 20,   8,  34, 220), IM_COL32( 78,  26, 128, 230),
            IM_COL32(218, 175, 255, 255), IM_COL32( 76,  34, 118, 200),
            IM_COL32(165,  80, 255, 255),
        },
        {
            kCat_Audio, DrawIcon_Audio, "Audio",
            IM_COL32( 30, 190, 190,  70), IM_COL32(  8,  60,  65, 255),
            IM_COL32(  5,  22,  26, 220), IM_COL32( 12,  90,  95, 230),
            IM_COL32(160, 245, 245, 255), IM_COL32( 20,  80,  84, 200),
            IM_COL32( 40, 210, 210, 255),
        },
    };

    ImDrawList*  dl      = ImGui::GetWindowDrawList();
    const float  sidebarW = ImGui::GetContentRegionAvail().x;
    const float  winH     = ImGui::GetWindowHeight();
    const ImVec2 winPos   = ImGui::GetWindowPos();

    dl->AddRectFilled(winPos, { winPos.x + sidebarW, winPos.y + winH },
                      IM_COL32(11, 11, 20, 255));

    ImGui::Dummy({ sidebarW, 8.0f });

    constexpr float btnGapY  = 2.0f;
    constexpr float rounding = 8.0f;
    const float     btnH     = 64.0f;
    const float     iconSz   = std::floor(btnH * 0.38f);
    (void)winH;

    ImGuiStorage* storage = ImGui::GetStateStorage();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, btnGapY));

    for (const auto& cd : k_Cats)
    {
        const bool active = (ctx.currentCategoryInt == cd.catInt);

        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImVec2 bMin   = cursor;
        ImVec2 bMax   = { cursor.x + sidebarW, cursor.y + btnH };

        ImGuiID hovId = ImGui::GetID(cd.label);
        float*  pT    = storage->GetFloatRef(hovId ^ 0xABCD1234u, 0.0f);
        bool hovered  = ImGui::IsMouseHoveringRect(bMin, bMax, false);
        *pT = Lerp(*pT, hovered ? 1.0f : 0.0f, ImGui::GetIO().DeltaTime * 14.0f);
        float t = *pT;

        // ── Fondo ─────────────────────────────────────────────────────────
        if (active) {
            ImVec4 ac = ImGui::ColorConvertU32ToFloat4(cd.accentBar);
            ac.w = 0.12f;
            dl->AddRectFilled(bMin, bMax,
                              ImGui::ColorConvertFloat4ToU32(ac), rounding);
        } else if (t > 0.01f) {
            dl->AddRectFilled(bMin, bMax,
                              IM_COL32(255, 255, 255, (int)(t * 14.f)), rounding);
        }

        // ── Barra lateral izquierda ────────────────────────────────────────
        {
            float barH     = btnH * 0.60f * (active ? 1.0f : t);
            float barY0    = cursor.y + (btnH - barH) * 0.5f;
            float barAlpha = active ? 1.0f : t * 0.55f;
            ImVec4 ac      = ImGui::ColorConvertU32ToFloat4(cd.accentBar);
            ac.w           = barAlpha;
            dl->AddRectFilled(
                { bMin.x,        barY0 },
                { bMin.x + 3.0f, barY0 + barH },
                ImGui::ColorConvertFloat4ToU32(ac), 2.0f);
        }

        ImGui::SetCursorScreenPos(bMin);
        const std::string btnId = std::string("##cat_") + cd.label;
        bool clicked = ImGui::InvisibleButton(btnId.c_str(), { sidebarW, btnH });

        // ── Icono + label ──────────────────────────────────────────────────
        {
            float iconBright = active ? 1.0f : Lerp(0.32f, 0.72f, t);
            ImVec4 icF = { iconBright, iconBright, iconBright, 1.0f };
            if (active) {
                ImVec4 ac = ImGui::ColorConvertU32ToFloat4(cd.accentBar);
                icF = LerpColor(icF, ac, 0.35f);
                icF.w = 1.0f;
            }

            ImVec2 lblDim       = ImGui::CalcTextSize(cd.label);
            float  totalContent = iconSz + 5.0f + lblDim.y;
            float  startY       = cursor.y + (btnH - totalContent) * 0.5f;
            float  iconX        = cursor.x + (sidebarW - iconSz) * 0.5f;

            cd.drawIcon(dl, { iconX, startY }, iconSz,
                        ImGui::ColorConvertFloat4ToU32(icF));

            float lblBright = active ? 1.0f : Lerp(0.30f, 0.72f, t);
            ImVec4 lblF = { lblBright, lblBright, lblBright, 1.0f };
            if (active) {
                ImVec4 ac = ImGui::ColorConvertU32ToFloat4(cd.accentBar);
                lblF = LerpColor(lblF, ac, 0.25f);
                lblF.w = 1.0f;
            }

            float lblX = cursor.x + (sidebarW - lblDim.x) * 0.5f;
            float lblY = startY + iconSz + 5.0f;
            dl->AddText({ lblX, lblY },
                        ImGui::ColorConvertFloat4ToU32(lblF), cd.label);
        }

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("%s", cd.label);

        if (clicked) {
            ctx.currentCategoryInt = cd.catInt;
            ctx.selectedIndex      = -1;
            ctx.refreshList();
        }
    }

    ImGui::PopStyleVar();
}

} // namespace ProyecThor::Library