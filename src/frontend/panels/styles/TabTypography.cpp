#include "TabTypography.h"
#include "backend/core/AppPaths.h"
#include <imgui.h>
#include <windows.h>
#include <commdlg.h>
#include <filesystem>
#include <cstring>
#include <algorithm>

namespace fs = std::filesystem;

namespace ProyecThor::UI {

TabTypography::TabTypography(std::vector<std::string>* fontList,
                             OnFontImportedCallback onFontImported)
    : m_FontList(fontList)
    , m_OnFontImported(std::move(onFontImported))
{
}

void TabTypography::Render(StyleData& data, float colWidth) {
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    CanvaStyleEditor::Badge("TIPOGRAFIA", CanvaPalette::Accent);
    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    RenderFontSelector(data, colWidth);
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    RenderColorPicker(data, colWidth);
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    RenderSizeSlider(data, colWidth);
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    RenderAutoScaleCheckbox(data);
}

void TabTypography::RenderFontSelector(StyleData& data, float colWidth) {
    CanvaStyleEditor::SectionLabel("Fuente");

    const float importBtnW = 90.0f;
    const float gap        = 6.0f;
    float comboW           = colWidth - importBtnW - gap;

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        CanvaPalette::Surface1);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, CanvaPalette::Surface2);
    ImGui::PushStyleColor(ImGuiCol_PopupBg,        ImVec4(0.10f, 0.11f, 0.14f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Header,         ImVec4(0.20f, 0.22f, 0.38f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,  CanvaPalette::Surface2);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::SetNextItemWidth(comboW);

    if (ImGui::BeginCombo("##editFont", data.selectedFont.c_str())) {
        if (m_FontList) {
            for (const auto& f : *m_FontList) {
                bool sel = (data.selectedFont == f);
                if (ImGui::Selectable(f.c_str(), sel))
                    data.selectedFont = f;
                if (sel) ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(5);

    ImGui::SameLine(0.0f, gap);

    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.18f, 0.20f, 0.32f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.28f, 0.46f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.14f, 0.16f, 0.26f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          CanvaPalette::Accent);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(8.0f, 6.0f));

    if (ImGui::Button("+ Fuente", ImVec2(importBtnW, 0.0f)))
        ImportFont();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);

    if (m_ImportMsgTimer > 0.0f) {
        m_ImportMsgTimer -= ImGui::GetIO().DeltaTime;
        bool isOk = (m_ImportStatus == "ok");
        ImGui::PushStyleColor(ImGuiCol_Text,
            isOk ? CanvaPalette::Green : ImVec4(0.93f, 0.26f, 0.36f, 1.0f));
        ImGui::TextUnformatted(isOk
            ? "Fuente importada correctamente"
            : m_ImportStatus.c_str());
        ImGui::PopStyleColor();
    } else {
        ImGui::Dummy(ImVec2(0.0f, ImGui::GetTextLineHeight()));
    }
}

void TabTypography::RenderColorPicker(StyleData& data, float colWidth) {
    CanvaStyleEditor::SectionLabel("Color del texto");
    ImGui::SetNextItemWidth(colWidth);
    ImGui::ColorEdit4("##editColor", data.textColor,
        ImGuiColorEditFlags_AlphaBar |
        ImGuiColorEditFlags_PickerHueWheel |
        ImGuiColorEditFlags_DisplayRGB);
}

void TabTypography::RenderSizeSlider(StyleData& data, float colWidth) {
    // ── Tamanio principal ─────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
    ImGui::Text("Tamanio inicial   %.0f px", data.textSize);
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_FrameBg,         CanvaPalette::Surface1);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,       CanvaPalette::Accent);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, CanvaPalette::AccentHov);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
    ImGui::SetNextItemWidth(colWidth);
    ImGui::SliderFloat("##editSize", &data.textSize, 20.0f, 300.0f, "");
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    // ── Tamanio de la referencia biblica ──────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
    ImGui::Text("Referencia (nombre + version)   %.0f px", data.refTextSize);
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_FrameBg,         CanvaPalette::Surface1);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,       CanvaPalette::Accent);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, CanvaPalette::AccentHov);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
    ImGui::SetNextItemWidth(colWidth);
    ImGui::SliderFloat("##editRefSize", &data.refTextSize, 10.0f, 200.0f, "");
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    // ── Tamanio del cuerpo del versiculo ──────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
    ImGui::Text("Versiculo (cuerpo del texto)   %.0f px", data.verseTextSize);
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_FrameBg,         CanvaPalette::Surface1);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,       CanvaPalette::Accent);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, CanvaPalette::AccentHov);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
    ImGui::SetNextItemWidth(colWidth);
    ImGui::SliderFloat("##editVerseSize", &data.verseTextSize, 10.0f, 300.0f, "");
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
}

void TabTypography::RenderAutoScaleCheckbox(StyleData& data) {
    ImGui::PushStyleColor(ImGuiCol_CheckMark,      CanvaPalette::Accent);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        CanvaPalette::Surface1);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, CanvaPalette::Surface2);
    ImGui::Checkbox("Auto-reducir si el texto no cabe", &data.autoScale);
    ImGui::PopStyleColor(3);

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
    ImGui::TextWrapped(
        "Cuando el texto supera la zona segura, el tamanio se reduce automaticamente "
        "hasta que entre. Util para presentaciones con contenido variable.");
    ImGui::PopStyleColor();
}

void TabTypography::ImportFont() {
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn       = {};
    ofn.lStructSize         = sizeof(ofn);
    ofn.hwndOwner           = NULL;
    ofn.lpstrFilter         = "Fuentes\0*.ttf;*.otf;*.ttc\0Todos los archivos\0*.*\0";
    ofn.lpstrFile           = filename;
    ofn.nMaxFile            = MAX_PATH;
    ofn.Flags               = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameA(&ofn)) return;

    try {
        std::filesystem::path fontsDir =
            std::filesystem::path(GetAssetsPath()) / "fonts";

        std::filesystem::create_directories(fontsDir);

        std::filesystem::path src(filename);
        std::filesystem::path dst = fontsDir / src.filename();

        std::filesystem::copy(src, dst, std::filesystem::copy_options::overwrite_existing);

        std::string fontName = src.stem().string();

        if (m_FontList) {
            bool exists = std::any_of(
                m_FontList->begin(), m_FontList->end(),
                [&fontName](const std::string& f) { return f == fontName; });
            if (!exists)
                m_FontList->push_back(fontName);
        }

        if (m_OnFontImported)
            m_OnFontImported(dst.string());

        m_ImportStatus   = "ok";
        m_ImportMsgTimer = 3.0f;

    } catch (const std::exception& e) {
        m_ImportStatus   = std::string("Error: ") + e.what();
        m_ImportMsgTimer = 4.0f;
    }
}

} // namespace ProyecThor::UI