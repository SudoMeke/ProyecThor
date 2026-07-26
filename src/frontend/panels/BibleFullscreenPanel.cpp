#include "BibleFullscreenPanel.h"
#include <imgui.h>

namespace ProyecThor::UI {

void BibleFullscreenPanel::Render() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove       |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("##BibleFullscreenRoot", nullptr, flags);
    ImGui::BeginChild("##bibleFullscreenContent", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AlwaysUseWindowPadding);

    m_BibleView.Render();

    ImGui::EndChild();
    ImGui::End();
}

} // namespace ProyecThor::UI
