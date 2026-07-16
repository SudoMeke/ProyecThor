#include "StageDisplayPanel.h"
#include "backend/settings/SettingsManager.h"
#include "backend/settings/StageLayoutTemplates.h"
#include <imgui.h>
#include <algorithm>
#include <string>

namespace ProyecThor::UI {

using namespace ProyecThor::Settings;

static constexpr ImVec4 kAccent    = { 0.30f, 0.55f, 0.95f, 1.0f };
static constexpr ImVec4 kSurface   = { 0.10f, 0.11f, 0.15f, 1.0f };
static constexpr ImVec4 kSurface2  = { 0.15f, 0.16f, 0.21f, 1.0f };
static constexpr ImVec4 kGrayText  = { 0.65f, 0.68f, 0.75f, 1.0f };

static ImU32 Col(ImVec4 v) { return ImGui::ColorConvertFloat4ToU32(v); }

// Boton simple de plantilla (mismo espiritu que el selector de calidad de
// CategoryProjection.cpp, duplicado liviano ya que viven en modulos
// distintos y el widget es de solo 6 lineas).
static bool TemplateButton(const char* id, const char* label, bool active, float width) {
    ImVec4 base   = active ? ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.35f) : ImVec4(1,1,1,0.05f);
    ImVec4 hover  = active ? ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.45f) : ImVec4(1,1,1,0.10f);
    ImVec4 border = active ? kAccent : ImVec4(1,1,1,0.12f);

    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_Button, base);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, hover);
    ImGui::PushStyleColor(ImGuiCol_Border, border);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, active ? 2.0f : 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

    bool clicked = ImGui::Button(label, ImVec2(width, 36.0f));

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    ImGui::PopID();
    return clicked;
}

void StageDisplayPanel::RenderContent() {
    ImGui::TextDisabled("Configura que ve el equipo en el escenario a traves del Monitor de Control.");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    RenderTemplateSelector();
    ImGui::Spacing();
    RenderCellPreview();
    ImGui::Spacing();
    RenderCellAssignments();
}

void StageDisplayPanel::RenderTemplateSelector() {
    auto& sd = ProyecThor::Settings::SettingsManager::Get().GetSettings().stageDisplay;

    ImGui::TextUnformatted("Distribucion");
    ImGui::Spacing();

    float w = ImGui::GetContentRegionAvail().x;
    float gap = 6.0f;
    float btnW = (w - gap * (kStageLayoutTemplateCount - 1)) / kStageLayoutTemplateCount;

    for (int i = 0; i < kStageLayoutTemplateCount; i++) {
        if (i > 0) ImGui::SameLine(0.0f, gap);
        std::string id = "tmpl" + std::to_string(i);
        if (TemplateButton(id.c_str(), kStageLayoutTemplates[i].label,
                            sd.layoutTemplateIndex == i, btnW)) {
            sd.layoutTemplateIndex = i;
            ProyecThor::Settings::SettingsManager::Get().Save();
        }
    }
}

void StageDisplayPanel::RenderCellPreview() {
    auto& sd = ProyecThor::Settings::SettingsManager::Get().GetSettings().stageDisplay;
    int idx = std::clamp(sd.layoutTemplateIndex, 0, kStageLayoutTemplateCount - 1);
    const auto& tmpl = kStageLayoutTemplates[idx];

    float w = ImGui::GetContentRegionAvail().x;
    float h = w * 9.0f / 16.0f; // vista previa en proporcion 16:9

    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1 = ImVec2(p0.x + w, p0.y + h);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(p0, p1, Col(kSurface), 8.0f);
    dl->AddRect(p0, p1, Col(ImVec4(1,1,1,0.15f)), 8.0f);

    for (int i = 0; i < tmpl.cellCount; i++) {
        const float* r = tmpl.rect[i];
        ImVec2 c0 = ImVec2(p0.x + r[0] * w, p0.y + r[1] * h);
        ImVec2 c1 = ImVec2(c0.x + r[2] * w, c0.y + r[3] * h);

        dl->AddRectFilled(ImVec2(c0.x + 2, c0.y + 2), ImVec2(c1.x - 2, c1.y - 2), Col(kSurface2), 6.0f);
        dl->AddRect(ImVec2(c0.x + 2, c0.y + 2), ImVec2(c1.x - 2, c1.y - 2), Col(ImVec4(1,1,1,0.10f)), 6.0f);

        auto widget = static_cast<StageWidgetType>(sd.cellWidget[i]);
        const char* label = StageWidgetTypeName(widget);
        ImVec2 ts = ImGui::CalcTextSize(label);
        ImVec2 center = ImVec2((c0.x + c1.x) * 0.5f - ts.x * 0.5f, (c0.y + c1.y) * 0.5f - ts.y * 0.5f);
        dl->AddText(center, Col(kGrayText), label);
    }

    ImGui::Dummy(ImVec2(w, h));
}

void StageDisplayPanel::RenderCellAssignments() {
    auto& sd = ProyecThor::Settings::SettingsManager::Get().GetSettings().stageDisplay;
    int idx = std::clamp(sd.layoutTemplateIndex, 0, kStageLayoutTemplateCount - 1);
    const auto& tmpl = kStageLayoutTemplates[idx];

    ImGui::TextUnformatted("Contenido por celda");
    ImGui::Spacing();

    static const char* kWidgetNames[] = { "Vacio", "Reloj", "Texto en vivo", "Proxima linea" };

    for (int i = 0; i < tmpl.cellCount; i++) {
        std::string label = "Celda " + std::to_string(i + 1);
        int current = std::clamp(sd.cellWidget[i], 0, 3);

        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::Combo(label.c_str(), &current, kWidgetNames, 4)) {
            sd.cellWidget[i] = current;
            ProyecThor::Settings::SettingsManager::Get().Save();
        }
    }
}

} // namespace ProyecThor::UI
