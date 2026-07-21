#include "ViewToolsPanel.h"
#include "UIManager.h"
#include "DesignSystem.h"
#include "frontend/ui/IconRail.h"
#include "frontend/ui/AppIcons.h"
#include "frontend/panels/home/HomeIcons.h"
#include "frontend/ui/bin/StyleGeneralApp.h"
#include "backend/core/PresentationCore.h"
#include "backend/settings/SettingsManager.h"
#include "MonitorTheme.h"
#include "MonitorUIHelpers.h"
#include <imgui.h>
#include <string>
#include <vector>

namespace ProyecThor::UI {

namespace MT = MonitorTheme;
using namespace Components;

ViewToolsPanel::ViewToolsPanel(UIManager* uiManager)
    : m_UIManager(uiManager)
{
    // Se registra a si mismo (direccion de su propio miembro) para que
    // ViewPanel pueda pedir "Limpiar reloj" sin depender de ViewToolsPanel
    // directamente — ver PresentationCore::SetOClockRef.
    Core::PresentationCore::Get().SetOClockRef(&m_OClock);
}

namespace {

// Misma copia chica que ya existia en MonitorView/ViewPanel — ver el
// comentario original en ViewPanel.cpp: no se justifica extraerla a un
// helper compartido todavia por lo simple que es.
bool DrawIconButton(const char* iconName, float size,
                    ImVec4 bgCol, ImVec4 hov, ImVec4 act,
                    ImVec2 btnSize, bool isActiveState = false)
{
    ImTextureID tex = (ImTextureID)0;
    auto it = StyleGeneralApp::Icons.find(iconName);
    if (it != StyleGeneralApp::Icons.end() && it->second.textureID)
        tex = (ImTextureID)(intptr_t)it->second.textureID;

    ImVec4 finalBg = isActiveState ? act : bgCol;

    ImGui::PushStyleColor(ImGuiCol_Button,        finalBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  act);

    bool pressed = ImGui::Button("", btnSize);
    bool isHeld  = ImGui::IsItemActive();

    ImVec2 p = ImGui::GetItemRectMin();
    ImVec2 s = ImGui::GetItemRectSize();

    float offsetY = isHeld ? 2.0f : 0.0f;
    ImU32 tintCol = isHeld
        ? ImGui::GetColorU32(ImVec4(0.8f, 0.8f, 0.8f, 1.0f))
        : ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

    ImGui::GetWindowDrawList()->AddImage(
        tex,
        { p.x + (s.x - size) * 0.5f, p.y + (s.y - size) * 0.5f + offsetY },
        { p.x + (s.x + size) * 0.5f, p.y + (s.y + size) * 0.5f + offsetY },
        ImVec2(0, 0), ImVec2(1, 1), tintCol);

    ImGui::PopStyleColor(3);
    return pressed;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
//  RenderControlOverlays — movido tal cual desde ViewPanel.cpp (mismo
//  comportamiento, ver comentario original alli): elegir/arrancar un macro,
//  y en modo manual avanzar/retroceder cue por cue con transicion.
// ─────────────────────────────────────────────────────────────────────────────
void ViewToolsPanel::RenderControlOverlays(float w, float h)
{
    auto& core = Core::PresentationCore::Get();
    bool  playing = core.IsMacroPlaying();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { MT::k_PadLg, MT::k_Pad });
    ImGui::PushStyleColor(ImGuiCol_ChildBg, MT::k_Bg1);
    ImGui::PushStyleColor(ImGuiCol_Border,  MT::k_BorderSubtle);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   MT::k_R);

    ImGui::BeginChild("##viewControlOverlays", { w, h }, true, ImGuiWindowFlags_NoScrollbar);

    const float innerW = w - MT::k_PadLg * 2.0f;

    // ── Cabecera ──────────────────────────────────────────────────────────────
    {
        ImVec2 headerPos = ImGui::GetCursorScreenPos();
        DrawStatusDot(
            ImGui::GetWindowDrawList(),
            { headerPos.x + 7.0f, headerPos.y + 9.0f },
            4.5f, MT::k_PrevAccent, playing);

        ImGui::SetCursorPosX(MT::k_PadLg + 20.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, playing ? MT::k_PrevAccent : MT::k_TextSecondary);
        ImGui::TextUnformatted(playing ? "CONTROL OVERLAYS  —  REPRODUCIENDO" : "CONTROL OVERLAYS");
        ImGui::PopStyleColor();
    }

    DrawAccentLine(innerW, MT::k_PrevAccentDim, 1.0f);
    ImGui::Spacing();

    if (!playing)
    {
        // ── Sin macro activo: elegir uno guardado y arrancarlo ──────────────
        static std::vector<std::string> s_names;
        static std::string              s_selName;

        s_names = Core::ListMacroNames();
        int s_selIdx = -1;
        for (int i = 0; i < (int)s_names.size(); i++)
            if (s_names[i] == s_selName) { s_selIdx = i; break; }
        if (s_selIdx < 0) s_selName.clear();

        ImGui::SetCursorPosX(MT::k_PadLg);
        ImGui::SetNextItemWidth(innerW - 100.0f);
        const char* preview = (s_selIdx >= 0 && s_selIdx < (int)s_names.size())
            ? s_names[s_selIdx].c_str() : "Elegi un macro guardado...";
        if (ImGui::BeginCombo("##ctrlOvMacroSel", preview)) {
            for (int i = 0; i < (int)s_names.size(); i++) {
                bool sel = (i == s_selIdx);
                if (ImGui::Selectable(s_names[i].c_str(), sel)) s_selName = s_names[i];
                if (sel) ImGui::SetItemDefaultFocus();
            }
            if (s_names.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, MT::k_TextDim);
                ImGui::TextUnformatted("Sin macros guardados (Diseño > Overlays > Macros)");
                ImGui::PopStyleColor();
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        bool canPlay = (s_selIdx >= 0 && s_selIdx < (int)s_names.size());
        ImGui::BeginDisabled(!canPlay);
        ImGui::PushID("ctrlov_play");
        if (DrawIconButton("play", 14.0f, MT::k_PrevBtn, MT::k_PrevBtnHov, MT::k_PrevBtnAct, {90.0f, 26.0f}))
            core.PlayMacro(s_names[s_selIdx], core.GetMacroAutoAdvance());
        ImGui::PopID();
        ImGui::EndDisabled();
    }
    else
    {
        // ── Macro activo: nombre + transporte de cues ───────────────────────
        ImGui::SetCursorPosX(MT::k_PadLg);
        ImGui::PushStyleColor(ImGuiCol_Text, MT::k_TextPrimary);
        std::string nameLine = core.GetActiveMacroName() + "   —   cue " +
            std::to_string(core.GetMacroCueIndex() + 1) + "/" + std::to_string(core.GetMacroCueCount());
        ImGui::TextUnformatted(nameLine.c_str());
        ImGui::PopStyleColor();

        ImGui::SetCursorPosX(MT::k_PadLg);

        const float navBtnW = 40.0f;
        const float btnH    = MT::k_TransportH;

        ImGui::PushID("ctrlov_prev");
        if (DrawIconButton("arrow_back", 15.0f, MT::k_NeutBtn, MT::k_NeutBtnHov, MT::k_NeutBtnAct, {navBtnW, btnH}))
            core.PrevMacroCue();
        ImGui::PopID();
        ImGui::SameLine(0.0f, MT::k_Gap);

        ImGui::PushID("ctrlov_next");
        if (DrawIconButton("arrow_forward", 15.0f, MT::k_PrevBtn, MT::k_PrevBtnHov, MT::k_PrevBtnAct, {navBtnW, btnH}))
            core.NextMacroCue();
        ImGui::PopID();
        ImGui::SameLine(0.0f, MT::k_Gap * 2.0f);

        bool autoAdv = core.GetMacroAutoAdvance();
        ImGui::PushID("ctrlov_auto");
        if (DrawIconButton("repeat", 14.0f, autoAdv ? MT::k_PrevBtn : MT::k_NeutBtn,
                           MT::k_PrevBtnHov, MT::k_PrevBtnAct, {navBtnW, btnH}, autoAdv))
            core.SetMacroAutoAdvance(true);
        ImGui::PopID();
        ImGui::SameLine(0.0f, MT::k_Gap);
        ImGui::PushID("ctrlov_manual");
        if (DrawIconButton("motion_play", 14.0f, !autoAdv ? MT::k_PrevBtn : MT::k_NeutBtn,
                           MT::k_PrevBtnHov, MT::k_PrevBtnAct, {navBtnW, btnH}, !autoAdv))
            core.SetMacroAutoAdvance(false);
        ImGui::PopID();
        ImGui::SameLine(0.0f, MT::k_Gap * 2.0f);

        ImGui::PushID("ctrlov_stop");
        if (DrawIconButton("stop", 15.0f, MT::k_LiveBtn, MT::k_LiveBtnHov, MT::k_LiveBtnAct, {navBtnW, btnH}))
            core.StopMacro();
        ImGui::PopID();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

void ViewToolsPanel::Render()
{
    // ── Pump incondicional ──────────────────────────────────────────────────
    // Mismo motivo que antes en HomePanel: OClock/StreamingPanel deben
    // seguir corriendo aunque el operador este mirando otra pestaña de este
    // hub (Reloj alimenta LAN/pantalla, Streaming alimenta la transmision).
    m_OClock.Update();
    m_StreamingPanel.Update();
    m_TeamChatPanel.Update();

    bool visible = m_UIManager
        ? DS::BeginGlassPanel(GetName().c_str(), m_UIManager->GetGlassRenderer(),
                              nullptr, 0, ImVec2(0.0f, 0.0f))
        : ImGui::Begin(GetName().c_str());

    if (!visible) {
        if (m_UIManager) DS::EndGlassPanel(); else ImGui::End();
        return;
    }

    const float railH  = IconRailThickness(false);
    const float totalW = ImGui::GetContentRegionAvail().x;

    // ── Rail de iconos arriba ────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::BeginChild("##viewToolsRail", ImVec2(totalW, railH), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    {
        static const IconRailItem kItems[] = {
            { (int)ViewToolsSection::ControlOverlays, AppIcons::DrawIcon_Mixer,     "Overlays" },
            { (int)ViewToolsSection::Streaming,       HomeIcons::DrawIcon_Broadcast,"Red"      },
            { (int)ViewToolsSection::QuickNotes,      HomeIcons::DrawIcon_Notepad,  "Notas"    },
            { (int)ViewToolsSection::Clock,           HomeIcons::DrawIcon_Clock,    "Reloj"    },
            { (int)ViewToolsSection::Chat,             HomeIcons::DrawIcon_Chat,      "Chat"    },
        };
        const auto& hubSettings = ProyecThor::Settings::SettingsManager::Get().GetSettings().viewTools;
        int currentIndex = (int)m_CurrentSection;
        RenderIconRail(kItems, 5, currentIndex, IconRailOrientation::Horizontal, hubSettings.categoryColor);
        m_CurrentSection = (ViewToolsSection)currentIndex;
    }

    ImGui::EndChild();

    // ── Divisor horizontal con gradiente (mismo estilo que StylesHubPanel) ──
    {
        ImVec2      p  = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 colLeft  = IM_COL32(60, 80, 160,  0);
        ImU32 colMid   = IM_COL32(60, 80, 160, 80);
        ImU32 colRight = IM_COL32(60, 80, 160,  0);
        float midX     = p.x + totalW * 0.5f;
        dl->AddRectFilledMultiColor(p, { midX, p.y + 1.f }, colLeft, colMid, colMid, colLeft);
        dl->AddRectFilledMultiColor({ midX, p.y }, { p.x + totalW, p.y + 1.f }, colMid, colRight, colRight, colMid);
        ImGui::Dummy(ImVec2(totalW, 1.0f));
    }

    // ── Contenido de la seccion activa ───────────────────────────────────────
    constexpr float kContentMarginX = 18.0f;
    constexpr float kContentMarginY = 16.0f;
    const float     contentH = ImGui::GetContentRegionAvail().y;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kContentMarginX, kContentMarginY));
    ImGui::BeginChild("##viewToolsContent", ImVec2(0.f, contentH),
                      ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();

    switch (m_CurrentSection)
    {
        case ViewToolsSection::ControlOverlays:
            RenderControlOverlays(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);
            break;
        case ViewToolsSection::Streaming:  m_StreamingPanel.RenderContent();          break;
        case ViewToolsSection::QuickNotes: m_QuickNotes.Render();                     break;
        case ViewToolsSection::Clock:
            if (m_UIManager) m_OClock.Render(m_UIManager->GetGlassRenderer());
            break;
        case ViewToolsSection::Chat:       m_TeamChatPanel.RenderContent();           break;
    }

    ImGui::EndChild();

    if (m_UIManager) DS::EndGlassPanel(); else ImGui::End();
}

} // namespace ProyecThor::UI
