#include "CanvaStyleEditor.h"
#include "styles/TabTypography.h"
#include "styles/TabAlignment.h"
#include "styles/TabMargins.h"
#include "backend/core/PresentationCore.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <cstring>
#include <cmath>
#include <string>
#include <algorithm>

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  CanvaPalette — definiciones
// ─────────────────────────────────────────────────────────────────────────────

const ImVec4 CanvaPalette::Accent       = ImVec4(0.39f, 0.44f, 0.97f, 1.0f);
const ImVec4 CanvaPalette::AccentHov    = ImVec4(0.49f, 0.54f, 1.00f, 1.0f);
const ImVec4 CanvaPalette::AccentActive = ImVec4(0.30f, 0.35f, 0.90f, 1.0f);
const ImVec4 CanvaPalette::Green        = ImVec4(0.10f, 0.79f, 0.55f, 1.0f);
const ImVec4 CanvaPalette::Red          = ImVec4(0.93f, 0.26f, 0.36f, 1.0f);
const ImVec4 CanvaPalette::Surface0     = ImVec4(0.09f, 0.09f, 0.11f, 1.0f);
const ImVec4 CanvaPalette::Surface1     = ImVec4(0.12f, 0.13f, 0.16f, 1.0f);
const ImVec4 CanvaPalette::Surface2     = ImVec4(0.16f, 0.17f, 0.22f, 1.0f);
const ImVec4 CanvaPalette::Border       = ImVec4(0.22f, 0.23f, 0.30f, 1.0f);
const ImVec4 CanvaPalette::Text         = ImVec4(0.92f, 0.92f, 0.94f, 1.0f);
const ImVec4 CanvaPalette::TextMuted    = ImVec4(0.50f, 0.52f, 0.60f, 1.0f);
const ImVec4 CanvaPalette::Gold         = ImVec4(0.95f, 0.72f, 0.20f, 1.0f);
const ImVec4 CanvaPalette::Pink         = ImVec4(0.93f, 0.40f, 0.70f, 1.0f);

ImU32 CanvaPalette::ToU32(const ImVec4& c) {
    return ImGui::ColorConvertFloat4ToU32(c);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers de widgets estilizados
// ─────────────────────────────────────────────────────────────────────────────

bool CanvaStyleEditor::PrimaryButton(const char* label, ImVec2 size) {
    ImGui::PushStyleColor(ImGuiCol_Button,        CanvaPalette::Accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CanvaPalette::AccentHov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  CanvaPalette::AccentActive);
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(14.0f, 8.0f));
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
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(14.0f, 8.0f));
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
    dl->AddRectFilled(p, rectMax, CanvaPalette::ToU32(bgColor), 6.0f);
    dl->AddRect(p, rectMax, CanvaPalette::ToU32(bdColor), 6.0f, 0, 1.2f);

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

// FIXED: IDs are built as "prefix##index" — never include visible label text
// in the ID string. This prevents collisions when multiple sections share
// labels like "Centro" or "Arriba".
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
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));

        // FIXED: ID = visible label + "##" + prefix + index
        // The "##" separator hides everything after it from the display,
        // but the full string (including prefix+index) is used for hashing.
        // This guarantees uniqueness across sections even if labels are identical.
        std::string id = std::string(labels[i])
                       + "##" + std::string(prefix)
                       + "_" + std::to_string(i);

        if (ImGui::Button(id.c_str(), ImVec2(segW, height)))
            *current = i;

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────

CanvaStyleEditor::CanvaStyleEditor(std::vector<std::string>* fontList,
                                   OnFontImportedCallback    onFontImported)
    : m_FontList(fontList)
{
    memset(m_Name, 0, sizeof(m_Name));

    m_TabTypography = std::make_unique<TabTypography>(fontList, std::move(onFontImported));
    m_TabAlignment  = std::make_unique<TabAlignment>();
    m_TabMargins    = std::make_unique<TabMargins>();
}

CanvaStyleEditor::~CanvaStyleEditor() = default;

// ─────────────────────────────────────────────────────────────────────────────
//  Apertura del editor
// ─────────────────────────────────────────────────────────────────────────────

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

// ─────────────────────────────────────────────────────────────────────────────
//  Render principal
// ─────────────────────────────────────────────────────────────────────────────

bool CanvaStyleEditor::Render(OnSaveCallback onSave) {
    if (!m_IsOpen) return false;

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(980.0f, 720.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(860.0f, 600.0f), ImVec2(1200.0f, 900.0f));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, CanvaPalette::Surface0);
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.22f, 0.23f, 0.32f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(0.0f, 0.0f));

    bool open           = true;
    bool savedThisFrame = false;

    constexpr ImGuiWindowFlags kEditorFlags =
        ImGuiWindowFlags_NoCollapse        |
        ImGuiWindowFlags_NoSavedSettings   |
        ImGuiWindowFlags_NoScrollbar       |
        ImGuiWindowFlags_NoScrollWithMouse;

    bool windowVisible = ImGui::Begin("EditorDeEstilo##canvaWindow", &open, kEditorFlags);

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    if (windowVisible)
    {
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2 winPos   = ImGui::GetWindowPos();
        ImVec2 winSize  = ImGui::GetWindowSize();

        RenderHeader(dl, winPos, winSize);

        const float kFooterH = 64.0f;
        const float kHeaderH = 60.0f;
        const float kPadH    = 20.0f;
        const float kColLeft = 340.0f;
        const float kColGap  = 20.0f;
        float colRight       = winSize.x - kColLeft - kColGap - kPadH * 2.0f;
        float contentH       = winSize.y - kHeaderH - kFooterH - kPadH * 2.0f;
        const float tabH     = 32.0f;

        // ── Columna izquierda ────────────────────────────────────────────
        ImGui::SetCursorPos(ImVec2(kPadH, kHeaderH + kPadH));
        ImGui::BeginGroup();
        {
            const char* tabLabels[] = { "Tipografia", "Alineacion", "Margenes" };
            const ImVec4 tabColors[] = {
                CanvaPalette::Accent,
                CanvaPalette::Green,
                CanvaPalette::Gold
            };

            for (int t = 0; t < 3; t++) {
                if (t > 0) ImGui::SameLine(0.0f, 4.0f);
                bool active = (m_ActiveTab == t);
                float tW    = (kColLeft - 8.0f) / 3.0f;

                ImVec4 bgActive = ImVec4(
                    tabColors[t].x * 0.20f,
                    tabColors[t].y * 0.20f,
                    tabColors[t].z * 0.42f,
                    1.0f);

                ImGui::PushStyleColor(ImGuiCol_Button,        active ? bgActive : CanvaPalette::Surface1);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CanvaPalette::Surface2);
                ImGui::PushStyleColor(ImGuiCol_Text,          active ? tabColors[t] : CanvaPalette::TextMuted);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));

                // FIXED: Tab IDs use index suffix to guarantee uniqueness
                std::string tabId = std::string(tabLabels[t]) + "##mainTab" + std::to_string(t);
                if (ImGui::Button(tabId.c_str(), ImVec2(tW, tabH)))
                    m_ActiveTab = t;

                if (active) {
                    ImVec2 btnMin = ImGui::GetItemRectMin();
                    ImVec2 btnMax = ImGui::GetItemRectMax();
                    dl->AddLine(
                        ImVec2(btnMin.x + 6.0f, btnMax.y - 1.0f),
                        ImVec2(btnMax.x - 6.0f, btnMax.y - 1.0f),
                        CanvaPalette::ToU32(tabColors[t]), 2.0f);
                }

                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(3);
            }

            ImGui::Dummy(ImVec2(0.0f, 10.0f));

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 10.0f));
            ImGui::BeginChild("##tabContent",
                ImVec2(kColLeft, contentH - tabH - 14.0f),
                false, ImGuiWindowFlags_NoScrollbar);

            RenderActiveTab(kColLeft, contentH, tabH, dl);

            ImGui::EndChild();
            ImGui::PopStyleVar();
        }
        ImGui::EndGroup();

        // ── Columna derecha: preview ─────────────────────────────────────
        ImGui::SetCursorPos(ImVec2(kPadH + kColLeft + kColGap, kHeaderH + kPadH));
        ImGui::BeginGroup();
        {
            Badge("PREVIEW EN VIVO", CanvaPalette::Pink);
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
            ImGui::PushStyleColor(ImGuiCol_CheckMark, CanvaPalette::Pink);
            ImGui::Checkbox("Texto largo de prueba", &m_LongPreview);
            ImGui::PopStyleColor();
        }
        ImGui::EndGroup();

        RenderFooter(winPos, winSize, onSave, savedThisFrame);
    }

    ImGui::End();

    if (!open) m_IsOpen = false;

    return savedThisFrame;
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderActiveTab
// ─────────────────────────────────────────────────────────────────────────────

void CanvaStyleEditor::RenderActiveTab(float colWidth, float /*contentH*/,
                                        float /*tabH*/, ImDrawList* dl) {
    switch (m_ActiveTab) {
        case 0: m_TabTypography->Render(m_Data, colWidth); break;
        case 1: m_TabAlignment->Render(m_Data, colWidth, dl); break;
        case 2: m_TabMargins->Render(m_Data, colWidth, dl); break;
        default: break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderHeader
// ─────────────────────────────────────────────────────────────────────────────

void CanvaStyleEditor::RenderHeader(ImDrawList* dl, ImVec2 winPos, ImVec2 winSize) {
    const float kHeaderH = 60.0f;

    dl->AddRectFilledMultiColor(
        winPos,
        ImVec2(winPos.x + winSize.x, winPos.y + kHeaderH),
        CanvaPalette::ToU32(ImVec4(0.18f, 0.20f, 0.38f, 1.0f)),
        CanvaPalette::ToU32(ImVec4(0.14f, 0.16f, 0.30f, 1.0f)),
        CanvaPalette::ToU32(ImVec4(0.09f, 0.09f, 0.12f, 1.0f)),
        CanvaPalette::ToU32(ImVec4(0.09f, 0.09f, 0.12f, 1.0f)));

    float dotY = winPos.y + kHeaderH * 0.5f;
    dl->AddCircleFilled(ImVec2(winPos.x + 28.0f, dotY), 7.0f,
        CanvaPalette::ToU32(CanvaPalette::Accent));
    dl->AddCircleFilled(ImVec2(winPos.x + 28.0f, dotY), 3.5f,
        IM_COL32(255, 255, 255, 210));

    std::string title = m_IsEditingExisting
        ? (std::string("Editar estilo — ") + m_Name)
        : "Nuevo estilo de texto";

    dl->AddText(ImGui::GetFont(), 15.0f,
        ImVec2(winPos.x + 46.0f, winPos.y + (kHeaderH - 15.0f) * 0.5f),
        CanvaPalette::ToU32(CanvaPalette::Text),
        title.c_str());

    const ImVec4 tabColors[] = { CanvaPalette::Accent, CanvaPalette::Green, CanvaPalette::Gold };
    const char*  tabNames[]  = { "Tipografia", "Alineacion", "Margenes" };
    float indicatorX         = winSize.x - 320.0f;

    for (int t = 0; t < 3; t++) {
        bool  active = (m_ActiveTab == t);
        float cx     = winPos.x + indicatorX + t * 100.0f + 50.0f;
        float cy     = dotY;

        dl->AddCircleFilled(ImVec2(cx - 22.0f, cy),
            active ? 5.0f : 3.5f,
            CanvaPalette::ToU32(active
                ? tabColors[t]
                : ImVec4(tabColors[t].x * 0.4f, tabColors[t].y * 0.4f,
                          tabColors[t].z * 0.4f, 1.0f)));

        dl->AddText(ImGui::GetFont(), 12.0f,
            ImVec2(cx - 10.0f, cy - 6.0f),
            CanvaPalette::ToU32(active ? tabColors[t] : CanvaPalette::TextMuted),
            tabNames[t]);
    }

    dl->AddLine(
        ImVec2(winPos.x, winPos.y + kHeaderH),
        ImVec2(winPos.x + winSize.x, winPos.y + kHeaderH),
        CanvaPalette::ToU32(CanvaPalette::Border), 1.0f);

    ImGui::Dummy(ImVec2(0.0f, kHeaderH));
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderPreview
// ─────────────────────────────────────────────────────────────────────────────

void CanvaStyleEditor::RenderPreview(ImVec2 pos, ImVec2 sz, ImDrawList* dl) {
    dl->AddRectFilled(pos, ImVec2(pos.x + sz.x, pos.y + sz.y),
        CanvaPalette::ToU32(ImVec4(0.04f, 0.04f, 0.06f, 1.0f)), 10.0f);
    dl->AddRect(pos, ImVec2(pos.x + sz.x, pos.y + sz.y),
        CanvaPalette::ToU32(CanvaPalette::Border), 10.0f, 0, 1.0f);

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
    ImU32 shadowCol = IM_COL32(0, 0, 0, 200);

    dl->AddText(previewFont, displaySize, ImVec2(tx + 1.5f, ty + 1.5f),
        shadowCol, testText, nullptr, bw);
    dl->AddText(previewFont, displaySize, ImVec2(tx, ty),
        textCol, testText, nullptr, bw);

    dl->AddText(ImGui::GetFont(), 10.0f,
        ImVec2(pos.x + 6.0f, pos.y + sz.y - 14.0f),
        CanvaPalette::ToU32(CanvaPalette::TextMuted),
        "1920 x 1080");
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderFooter
// ─────────────────────────────────────────────────────────────────────────────

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

    ImGui::SetCursorPos(ImVec2(20.0f, footerY + (kFooterH - 36.0f) * 0.5f));

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
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
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
    ImGui::SameLine(winSize.x - btnGroupW - 20.0f);

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
