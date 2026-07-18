#include "LibrarySidebar.h"
#include "LibraryIcons.h"
#include "LibraryStyles.h"
#include "LibraryHelpers.h"
#include "backend/settings/SettingsManager.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <string>
#include <cmath>
#include <algorithm>

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

// Progreso animado (0..1) de "mostrar titulo" — misma idea que IconRail.cpp,
// para que este sidebar (implementacion propia, no comparte RenderIconRail)
// se comporte igual que los otros 3 rails ante Vista > Titulos en barras.
static float RailLabelProgress()
{
    bool wantLabels = ProyecThor::Settings::SettingsManager::Get().GetSettings().general.showRailLabels;
    ImGuiStorage* storage = ImGui::GetStateStorage();
    ImGuiID id = ImGui::GetID("##librarySidebarLabelT");
    float* cur = storage->GetFloatRef(id, wantLabels ? 1.0f : 0.0f);
    float target = wantLabels ? 1.0f : 0.0f;
    *cur += (target - *cur) * std::min(1.0f, ImGui::GetIO().DeltaTime * 10.0f);
    return *cur;
}

void RenderCategoryButtons(LibraryContext& ctx)
{
    using DrawFn = void(*)(ImDrawList*, ImVec2, float, ImU32);

    // El color de identidad de cada categoria (accentBar) es configurable
    // desde Ajustes > Apariencia (SettingsManager: librarySidebar.categoryColor,
    // en el mismo orden que este array). El resto del look de cada boton
    // (fondo activo, barra lateral, tinte de icono/label) se deriva de ese
    // unico color mas abajo, no hace falta guardar variantes aparte.
    struct CatDef {
        int         catInt;
        DrawFn      drawIcon;
        const char* label;
    };

    static const CatDef k_Cats[] = {
        { kCat_Songs,     DrawIcon_Music,    "Letra"  },
        { kCat_Videos,    DrawIcon_Play,     "Video"  },
        { kCat_Images,    DrawIcon_Image,    "Imagen" },
        { kCat_Bibles,    DrawIcon_Cross,    "Biblia" },
        { kCat_Documents, DrawIcon_Document, "Doc"    },
        { kCat_Audio,     DrawIcon_Audio,    "Audio"  },
    };

    const auto& sidebarSettings = ProyecThor::Settings::SettingsManager::Get().GetSettings().librarySidebar;

    ImDrawList*  dl      = ImGui::GetWindowDrawList();
    const float  sidebarW = ImGui::GetContentRegionAvail().x;
    const float  winH     = ImGui::GetWindowHeight();
    const ImVec2 winPos   = ImGui::GetWindowPos();

    dl->AddRectFilled(winPos, { winPos.x + sidebarW, winPos.y + winH },
                      IM_COL32(11, 11, 20, 255));

    ImGui::Dummy({ sidebarW, 4.0f });

    constexpr float btnGapY  = 1.0f;
    constexpr float rounding = 5.0f;
    const float     btnH     = 46.0f;
    const float     iconSz   = std::floor(btnH * 0.38f);
    (void)winH;

    const float lt = RailLabelProgress();

    ImGuiStorage* storage = ImGui::GetStateStorage();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, btnGapY));

    for (int catIdx = 0; catIdx < (int)(sizeof(k_Cats) / sizeof(k_Cats[0])); catIdx++)
    {
        const auto& cd = k_Cats[catIdx];
        const bool  active = (ctx.currentCategoryInt == cd.catInt);
        const float* cc = sidebarSettings.categoryColor[catIdx];
        const ImU32 accentBar = ImGui::ColorConvertFloat4ToU32(
            ImVec4(cc[0], cc[1], cc[2], cc[3]));

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
            ImVec4 ac = ImGui::ColorConvertU32ToFloat4(accentBar);
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
            ImVec4 ac      = ImGui::ColorConvertU32ToFloat4(accentBar);
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
                ImVec4 ac = ImGui::ColorConvertU32ToFloat4(accentBar);
                icF = LerpColor(icF, ac, 0.35f);
                icF.w = 1.0f;
            }

            ImVec2 lblDim       = ImGui::CalcTextSize(cd.label);
            float  totalContent = iconSz + lt * (5.0f + lblDim.y);
            float  startY       = cursor.y + (btnH - totalContent) * 0.5f;
            float  iconX        = cursor.x + (sidebarW - iconSz) * 0.5f;

            cd.drawIcon(dl, { iconX, startY }, iconSz,
                        ImGui::ColorConvertFloat4ToU32(icF));

            if (lt > 0.01f) {
                float lblBright = active ? 1.0f : Lerp(0.30f, 0.72f, t);
                ImVec4 lblF = { lblBright, lblBright, lblBright, lt };
                if (active) {
                    ImVec4 ac = ImGui::ColorConvertU32ToFloat4(accentBar);
                    lblF = LerpColor(lblF, ac, 0.25f);
                    lblF.w = lt;
                }

                float lblX = cursor.x + (sidebarW - lblDim.x) * 0.5f;
                float lblY = startY + iconSz + 5.0f;
                dl->AddText({ lblX, lblY },
                            ImGui::ColorConvertFloat4ToU32(lblF), cd.label);
            }
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