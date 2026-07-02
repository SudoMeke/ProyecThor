#include "QuickNotes.h"
#include "backend/core/PresentationCore.h"
#include "frontend/ui/UIStrings.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <cstring>

namespace ProyecThor::UI {

QuickNotes::QuickNotes() : m_IsLive(false) {
    m_TextBuffer.fill('\0');
}

QuickNotes::~QuickNotes() {
    if (m_IsLive)
        Core::PresentationCore::Get().ClearQuickNote();
}

std::string QuickNotes::GetName() const { return "QuickNotes"; }

static ImU32 ColU32(float r, float g, float b, float a = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
}

void QuickNotes::PushToCore() {
    auto& core       = Core::PresentationCore::Get();
    std::string text = std::string(m_TextBuffer.data());
    if (text.empty())
        core.ClearQuickNote();
    else
        core.SetLiveQuickNote(text);  // ahora escribe en currentText con el estilo activo
}

void QuickNotes::Render() {
    const auto& str = ProyecThor::UI::GetUIStrings();
    auto& core      = Core::PresentationCore::Get();

    bool forceUpdate = false;
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        if (ImGui::IsKeyPressed(ImGuiKey_F5) && !m_IsLive) {
            m_IsLive    = true;
            forceUpdate = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) && m_IsLive) {
            m_IsLive = false;
            core.ClearQuickNote();
        }
    }

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07f, 0.08f, 0.10f, 1.0f));
    ImGui::Begin(str.quickNotesTitle);


    if (m_IsLive) {
        ImGui::SameLine();
        float avail = ImGui::GetContentRegionAvail().x;
        float tw    = ImGui::CalcTextSize(str.liveIndicator).x + 18.0f;
        ImGui::SameLine(ImGui::GetWindowWidth() - tw - 16.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
        ImGui::TextUnformatted("● ");
        ImGui::SameLine(0, 2);
        ImGui::TextUnformatted(str.liveIndicator);
        ImGui::PopStyleColor();
        (void)avail;
    }



    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.65f, 0.15f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.78f, 0.20f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.50f, 0.10f, 0.10f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    if (ImGui::Button(str.songClearScreen, ImVec2(-1, 35))) {
        m_TextBuffer.fill('\0');
        m_IsLive = false;
        core.ClearQuickNote();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    ImGui::Separator();
    ImGui::Spacing();

    ImVec4 inputBg = m_IsLive
        ? ImVec4(0.04f, 0.16f, 0.07f, 1.0f)
        : ImVec4(0.10f, 0.10f, 0.13f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        inputBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(inputBg.x + 0.03f, inputBg.y + 0.03f, inputBg.z + 0.05f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  inputBg);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

    float inputH = ImGui::GetTextLineHeight() * 6.0f + ImGui::GetStyle().FramePadding.y * 2.0f;
    bool textChanged = ImGui::InputTextMultiline(
        "##QuickNoteInput",
        m_TextBuffer.data(),
        m_TextBuffer.size(),
        ImVec2(-FLT_MIN, inputH));

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

    if (!m_IsLive) {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.15f, 0.55f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.65f, 0.30f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.10f, 0.45f, 0.18f, 1.0f));
        if (ImGui::Button(str.showOnScreen, ImVec2(-FLT_MIN, 40))) {
            m_IsLive    = true;
            forceUpdate = true;
        }
        ImGui::PopStyleColor(3);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.65f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.78f, 0.20f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.50f, 0.10f, 0.10f, 1.0f));
        if (ImGui::Button(str.hideMessage, ImVec2(-FLT_MIN, 40))) {
            m_IsLive = false;
            core.ClearQuickNote();
        }
        ImGui::PopStyleColor(3);
    }

    ImGui::PopStyleVar();

    if (m_IsLive && (textChanged || forceUpdate))
        PushToCore();

    ImGui::End();
    ImGui::PopStyleColor(); // WindowBg
}

} // namespace ProyecThor::UI