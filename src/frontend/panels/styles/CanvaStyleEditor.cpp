#include "CanvaStyleEditor.h"
#include "styles/TabTypography.h"
#include "styles/TabAlignment.h"
#include "styles/TabMargins.h"
#include "styles/TabEffects.h"
#include "backend/core/PresentationCore.h"
#include "frontend/ui/TextEffectsRenderer.h"
#include "SettingsManager.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <cstring>
#include <cmath>
#include <string>
#include <algorithm>

namespace ProyecThor::UI {

ImU32 CanvaPalette::ToU32(const ImVec4& c) {
    return ImGui::ColorConvertFloat4ToU32(c);
}

static ImVec4 CanvaV(const float* a, float alphaMul = 1.0f) {
    return ImVec4(a[0], a[1], a[2], a[3] * alphaMul);
}

void CanvaPalette::Sync(const ProyecThor::Settings::ThemeSettings& t) {
    Accent       = CanvaV(t.accent);
    AccentHov    = CanvaV(t.accentLight);
    AccentActive = CanvaV(t.accentDim);
    Green        = CanvaV(t.success);
    Red          = CanvaV(t.danger);
    Surface0     = CanvaV(t.surface0);
    Surface1     = CanvaV(t.surface1);
    Surface2     = CanvaV(t.surface2);
    Border       = CanvaV(t.border);
    Text         = CanvaV(t.textPrimary);
    TextMuted    = CanvaV(t.textDim);
}

bool CanvaStyleEditor::PrimaryButton(const char* label, ImVec2 size) {
    ImGui::PushStyleColor(ImGuiCol_Button,        CanvaPalette::Accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CanvaPalette::AccentHov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  CanvaPalette::AccentActive);
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(14.0f, 7.0f));
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    return clicked;
}

bool CanvaStyleEditor::GhostButton(const char* label, ImVec2 size) {
    ImGui::PushStyleColor(ImGuiCol_Button,        CanvaPalette::Surface1);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CanvaPalette::Surface2);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.28f, 0.30f, 0.40f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          CanvaPalette::Text);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(14.0f, 7.0f));
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    return clicked;
}

void CanvaStyleEditor::Badge(const char* label, ImVec4 color) {
    ImVec2 p       = ImGui::GetCursorScreenPos();
    ImVec2 textSz  = ImGui::CalcTextSize(label);
    const float px = 8.0f, py = 3.0f;
    ImVec2 rectMax = ImVec2(p.x + textSz.x + px * 2.0f, p.y + textSz.y + py * 2.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec4 bgColor = ImVec4(color.x * 0.20f, color.y * 0.20f, color.z * 0.20f, 1.0f);
    ImVec4 bdColor = ImVec4(color.x, color.y, color.z, 0.45f);
    dl->AddRectFilled(p, rectMax, CanvaPalette::ToU32(bgColor), 4.0f);
    dl->AddRect(p, rectMax, CanvaPalette::ToU32(bdColor), 4.0f, 0, 1.0f);

    ImGui::SetCursorScreenPos(ImVec2(p.x + px, p.y + py));
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + textSz.y + py * 2.0f + 2.0f));
}

void CanvaStyleEditor::SectionLabel(const char* label) {
    ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
}

void CanvaStyleEditor::SegmentedButtons(const char* prefix,
                                         const char** labels, int count, int* current,
                                         float totalWidth, float height,
                                         const ImVec4& activeColor) {
    const float gap = 4.0f;
    float segW = (totalWidth - gap * (count - 1)) / static_cast<float>(count);

    for (int i = 0; i < count; i++) {
        if (i > 0) ImGui::SameLine(0.0f, gap);

        bool active = (*current == i);
        ImVec4 bgActive = ImVec4(
            activeColor.x * 0.28f,
            activeColor.y * 0.28f,
            activeColor.z * 0.55f,
            1.0f);

        ImGui::PushStyleColor(ImGuiCol_Button,        active ? bgActive : CanvaPalette::Surface1);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CanvaPalette::Surface2);
        ImGui::PushStyleColor(ImGuiCol_Text,          active ? activeColor : CanvaPalette::TextMuted);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));

        std::string id = std::string(labels[i])
                       + "##" + std::string(prefix)
                       + "_" + std::to_string(i);

        if (ImGui::Button(id.c_str(), ImVec2(segW, height)))
            *current = i;

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
    }
}

CanvaStyleEditor::CanvaStyleEditor(std::vector<std::string>* fontList,
                                   OnFontImportedCallback    onFontImported)
    : m_FontList(fontList)
{
    memset(m_Name, 0, sizeof(m_Name));

    m_TabTypography = std::make_unique<TabTypography>(fontList, std::move(onFontImported));
    m_TabAlignment  = std::make_unique<TabAlignment>();
    m_TabMargins    = std::make_unique<TabMargins>();
    m_TabEffects    = std::make_unique<TabEffects>();
}

CanvaStyleEditor::~CanvaStyleEditor() = default;

void CanvaStyleEditor::OpenNew(const StyleData& defaults) {
    m_IsEditingExisting = false;
    m_IsOpen            = true;
    m_ActiveTab         = 0;
    m_LongPreview       = false;
    m_Data              = defaults;
    memset(m_Name, 0, sizeof(m_Name));
}

void CanvaStyleEditor::OpenEdit(const std::string& existingName, const StyleData& existingData) {
    m_IsEditingExisting = true;
    m_IsOpen            = true;
    m_ActiveTab         = 0;
    m_LongPreview       = false;
    m_Data              = existingData;

    size_t len = std::min(existingName.size(), sizeof(m_Name) - 1);
    memcpy(m_Name, existingName.c_str(), len);
    m_Name[len] = '\0';
}

bool CanvaStyleEditor::Render(OnSaveCallback onSave, bool embedded) {
    if (!m_IsOpen) return false;

    bool open           = true;
    bool savedThisFrame = false;

    if (!embedded) {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(980.0f, 720.0f), ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints(ImVec2(860.0f, 600.0f), ImVec2(1200.0f, 900.0f));

        ImGui::PushStyleColor(ImGuiCol_WindowBg, CanvaPalette::Surface0);
        ImGui::PushStyleColor(ImGuiCol_Border,   CanvaPalette::Border);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(0.0f, 0.0f));

        constexpr ImGuiWindowFlags kEditorFlags =
            ImGuiWindowFlags_NoCollapse        |
            ImGuiWindowFlags_NoSavedSettings   |
            ImGuiWindowFlags_NoScrollbar       |
            ImGuiWindowFlags_NoScrollWithMouse;

        bool windowVisible = ImGui::Begin("EditorDeEstilo##canvaWindow", &open, kEditorFlags);

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);

        if (!windowVisible) {
            ImGui::End();
            if (!open) m_IsOpen = false;
            return false;
        }
    }

    {
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2 winPos   = embedded ? ImGui::GetCursorScreenPos() : ImGui::GetWindowPos();
        ImVec2 winSize  = embedded ? ImGui::GetContentRegionAvail() : ImGui::GetWindowSize();

        RenderHeader(dl, winPos, winSize);

        const float kFooterH = 64.0f;
        const float kHeaderH = 60.0f;
        const float kPadH    = 20.0f;
        const float kColLeft = 340.0f;
        const float kColGap  = 20.0f;
        float colRight       = winSize.x - kColLeft - kColGap - kPadH * 2.0f;
        float contentH       = winSize.y - kHeaderH - kFooterH - kPadH * 2.0f;
        const float tabH     = 32.0f;

        ImGui::SetCursorScreenPos(ImVec2(winPos.x + kPadH, winPos.y + kHeaderH + kPadH));
        ImGui::BeginGroup();
        {
            const char* tabLabels[] = { "Tipografia", "Alineacion", "Margenes", "Efectos" };

            ImVec4 bgActive = ImVec4(
                CanvaPalette::Accent.x * 0.20f,
                CanvaPalette::Accent.y * 0.20f,
                CanvaPalette::Accent.z * 0.42f,
                1.0f);

            for (int t = 0; t < 4; t++) {
                if (t > 0) ImGui::SameLine(0.0f, 4.0f);
                bool active = (m_ActiveTab == t);
                float tW    = (kColLeft - 4.0f * 3.0f) / 4.0f;

                ImGui::PushStyleColor(ImGuiCol_Button,        active ? bgActive : CanvaPalette::Surface1);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CanvaPalette::Surface2);
                ImGui::PushStyleColor(ImGuiCol_Text,          active ? CanvaPalette::Accent : CanvaPalette::TextMuted);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));

                std::string tabId = std::string(tabLabels[t]) + "##mainTab" + std::to_string(t);
                if (ImGui::Button(tabId.c_str(), ImVec2(tW, tabH)))
                    m_ActiveTab = t;

                if (active) {
                    ImVec2 btnMin = ImGui::GetItemRectMin();
                    ImVec2 btnMax = ImGui::GetItemRectMax();
                    dl->AddLine(
                        ImVec2(btnMin.x + 6.0f, btnMax.y - 1.0f),
                        ImVec2(btnMax.x - 6.0f, btnMax.y - 1.0f),
                        CanvaPalette::ToU32(CanvaPalette::Accent), 2.0f);
                }

                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(3);
            }

            ImGui::Dummy(ImVec2(0.0f, 10.0f));

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 10.0f));
            ImGui::BeginChild("##tabContent",
                ImVec2(kColLeft, contentH - tabH - 14.0f));

            RenderActiveTab(kColLeft, contentH, tabH, dl);

            ImGui::EndChild();
            ImGui::PopStyleVar();
        }
        ImGui::EndGroup();

        ImGui::SetCursorScreenPos(ImVec2(winPos.x + kPadH + kColLeft + kColGap, winPos.y + kHeaderH + kPadH));
        ImGui::BeginGroup();
        {
            Badge("PREVIEW EN VIVO", CanvaPalette::Accent);
            ImGui::Dummy(ImVec2(0.0f, 6.0f));

            float previewW    = colRight;
            float previewH    = previewW * (9.0f / 16.0f);
            ImVec2 previewPos = ImGui::GetCursorScreenPos();
            ImVec2 previewSz  = ImVec2(previewW, previewH);

            RenderPreview(previewPos, previewSz, dl);
            ImGui::Dummy(previewSz);

            ImGui::Dummy(ImVec2(0.0f, 6.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
            ImGui::Text("Fuente: %s  |  Tamanio: %.0f px",
                m_Data.selectedFont.c_str(), m_Data.textSize);
            ImGui::PopStyleColor();

            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            ImGui::PushStyleColor(ImGuiCol_CheckMark, CanvaPalette::Accent);
            ImGui::Checkbox("Texto largo de prueba", &m_LongPreview);
            ImGui::PopStyleColor();
        }
        ImGui::EndGroup();

        RenderFooter(winPos, winSize, onSave, savedThisFrame);
    }

    if (!embedded) {
        ImGui::End();
        if (!open) m_IsOpen = false;
    }

    return savedThisFrame;
}

void CanvaStyleEditor::RenderActiveTab(float colWidth, float /*contentH*/,
                                        float /*tabH*/, ImDrawList* dl) {
    switch (m_ActiveTab) {
        case 0: m_TabTypography->Render(m_Data, colWidth); break;
        case 1: m_TabAlignment->Render(m_Data, colWidth, dl); break;
        case 2: m_TabMargins->Render(m_Data, colWidth, dl); break;
        case 3: m_TabEffects->Render(m_Data, colWidth); break;
        default: break;
    }
}

void CanvaStyleEditor::RenderHeader(ImDrawList* dl, ImVec2 winPos, ImVec2 winSize) {
    const float kHeaderH = 60.0f;

    dl->AddRectFilled(
        winPos,
        ImVec2(winPos.x + winSize.x, winPos.y + kHeaderH),
        CanvaPalette::ToU32(CanvaPalette::Surface1));

    float dotY = winPos.y + kHeaderH * 0.5f;
    dl->AddCircleFilled(ImVec2(winPos.x + 28.0f, dotY), 4.0f,
        CanvaPalette::ToU32(CanvaPalette::Accent));

    std::string title = m_IsEditingExisting
        ? (std::string("Editar estilo — ") + m_Name)
        : "Nuevo estilo de texto";

    dl->AddText(ImGui::GetFont(), 15.0f,
        ImVec2(winPos.x + 44.0f, winPos.y + (kHeaderH - 15.0f) * 0.5f),
        CanvaPalette::ToU32(CanvaPalette::Text),
        title.c_str());

    dl->AddLine(
        ImVec2(winPos.x, winPos.y + kHeaderH - 1.0f),
        ImVec2(winPos.x + winSize.x, winPos.y + kHeaderH - 1.0f),
        CanvaPalette::ToU32(ImVec4(CanvaPalette::Accent.x, CanvaPalette::Accent.y, CanvaPalette::Accent.z, 0.35f)), 1.5f);

    ImGui::Dummy(ImVec2(0.0f, kHeaderH));
}

void CanvaStyleEditor::RenderPreview(ImVec2 pos, ImVec2 sz, ImDrawList* dl) {
    dl->AddRectFilled(pos, ImVec2(pos.x + sz.x, pos.y + sz.y),
        CanvaPalette::ToU32(ImVec4(0.04f, 0.04f, 0.06f, 1.0f)), 6.0f);
    dl->AddRect(pos, ImVec2(pos.x + sz.x, pos.y + sz.y),
        CanvaPalette::ToU32(CanvaPalette::Border), 6.0f, 0, 1.0f);

    float sc = sz.x / 1920.0f;
    float mL = m_Data.margins[0] * sc;
    float mT = m_Data.margins[1] * sc;
    float mR = m_Data.margins[2] * sc;
    float mB = m_Data.margins[3] * sc;

    float bx = pos.x + mL;
    float by = pos.y + mT;
    float bw = std::max(10.0f, sz.x - mL - mR);
    float bh = std::max(10.0f, sz.y - mT - mB);

    ImU32 dimColor = IM_COL32(0, 0, 0, 100);
    dl->AddRectFilled(pos, ImVec2(pos.x + sz.x, by), dimColor);
    dl->AddRectFilled(ImVec2(pos.x, by + bh), ImVec2(pos.x + sz.x, pos.y + sz.y), dimColor);
    dl->AddRectFilled(ImVec2(pos.x, by), ImVec2(bx, by + bh), dimColor);
    dl->AddRectFilled(ImVec2(bx + bw, by), ImVec2(pos.x + sz.x, by + bh), dimColor);

    dl->AddRect(ImVec2(bx, by), ImVec2(bx + bw, by + bh),
        CanvaPalette::ToU32(ImVec4(0.39f, 0.44f, 0.97f, 0.50f)), 0.0f, 0, 1.2f);

    const char* shortText =
        "Porque de tal manera amo Dios\nal mundo, que ha dado a\nsu Hijo unigenito.";
    const char* longText =
        "Y el Verbo se hizo carne, y habito\nentre nosotros, lleno de gracia y\n"
        "de verdad; y vimos su gloria, gloria\ncomo del unigenito del Padre.";

    const char* testText = m_LongPreview ? longText : shortText;

    float displaySize = m_Data.textSize * sc;

    ImFont* previewFont = Core::PresentationCore::Get().GetImGuiFont(m_Data.selectedFont, 60.0f);
    if (!previewFont) previewFont = ImGui::GetFont();

    if (m_Data.autoScale) {
        while (displaySize > 6.0f) {
            ImVec2 ts = previewFont->CalcTextSizeA(displaySize, FLT_MAX, bw, testText);
            if (ts.y <= bh && ts.x <= bw) break;
            displaySize -= 0.5f;
        }
    }

    ImVec2 blockSz = previewFont->CalcTextSizeA(displaySize, FLT_MAX, bw, testText);

    float tx = bx;
    if      (m_Data.textAlignment == 1) tx += (bw - blockSz.x) * 0.5f;
    else if (m_Data.textAlignment == 2) tx += bw - blockSz.x;

    float ty = by;
    if      (m_Data.vAlignment == 1) ty += (bh - blockSz.y) * 0.5f;
    else if (m_Data.vAlignment == 2) ty += bh - blockSz.y;

    ImU32 textCol = IM_COL32(
        static_cast<int>(m_Data.textColor[0] * 255),
        static_cast<int>(m_Data.textColor[1] * 255),
        static_cast<int>(m_Data.textColor[2] * 255),
        static_cast<int>(m_Data.textColor[3] * 255));

    DrawStyledText(dl, previewFont, displaySize, ImVec2(tx, ty), textCol,
                   testText, bw, sc, m_Data.effects);

    dl->AddText(ImGui::GetFont(), 10.0f,
        ImVec2(pos.x + 6.0f, pos.y + sz.y - 14.0f),
        CanvaPalette::ToU32(CanvaPalette::TextMuted),
        "1920 x 1080");
}

void CanvaStyleEditor::RenderFooter(ImVec2 winPos, ImVec2 winSize,
                                     OnSaveCallback& onSave, bool& savedThisFrame) {
    const float kFooterH = 64.0f;
    float footerY        = winSize.y - kFooterH;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddLine(
        ImVec2(winPos.x, winPos.y + footerY),
        ImVec2(winPos.x + winSize.x, winPos.y + footerY),
        CanvaPalette::ToU32(CanvaPalette::Border), 1.0f);

    dl->AddRectFilled(
        ImVec2(winPos.x, winPos.y + footerY),
        ImVec2(winPos.x + winSize.x, winPos.y + winSize.y),
        CanvaPalette::ToU32(ImVec4(0.08f, 0.08f, 0.10f, 1.0f)),
        0.0f, ImDrawFlags_RoundCornersBottom);

    float rowY = winPos.y + footerY + (kFooterH - 36.0f) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(winPos.x + 20.0f, rowY));

    ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Nombre del estilo:");
    ImGui::PopStyleColor();
    ImGui::SameLine(0.0f, 8.0f);

    bool nameEmpty = (strlen(m_Name) == 0);
    if (nameEmpty) {
        ImGui::PushStyleColor(ImGuiCol_Border,
            CanvaPalette::ToU32(ImVec4(0.90f, 0.30f, 0.30f, 0.70f)));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    }
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        CanvaPalette::Surface1);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, CanvaPalette::Surface2);
    ImGui::PushStyleColor(ImGuiCol_Text,           CanvaPalette::Text);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::SetNextItemWidth(240.0f);
    ImGui::InputText("##styleNameInput", m_Name, sizeof(m_Name));
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    if (nameEmpty) {
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    if (nameEmpty) {
        ImGui::SameLine(0.0f, 8.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.40f, 0.40f, 1.0f));
        ImGui::AlignTextToFramePadding();
        ImGui::Text("El nombre es obligatorio");
        ImGui::PopStyleColor();
    }

    float btnGroupW = 110.0f + 8.0f + 160.0f;
    ImGui::SetCursorScreenPos(ImVec2(winPos.x + winSize.x - btnGroupW - 20.0f, rowY));

    if (GhostButton("  Cancelar  ", ImVec2(110.0f, 36.0f)))
        m_IsOpen = false;

    ImGui::SameLine(0.0f, 8.0f);

    bool canSave = !nameEmpty;
    if (!canSave)
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);

    if (PrimaryButton("  Guardar estilo  ", ImVec2(160.0f, 36.0f)) && canSave) {
        if (onSave)
            onSave(std::string(m_Name), m_Data);
        savedThisFrame = true;
        m_IsOpen       = false;
    }

    if (!canSave)
        ImGui::PopStyleVar();
}

} // namespace ProyecThor::UI
