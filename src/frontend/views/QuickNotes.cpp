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
    // Al destruirse, limpiar cualquier transmision activa (principal o LAN)
    auto& core = Core::PresentationCore::Get();
    if (m_TransmitMode == QuickNoteTransmitMode::MainOnly || m_TransmitMode == QuickNoteTransmitMode::Both)
        core.ClearQuickNote();
    if (m_TransmitMode == QuickNoteTransmitMode::LANOnly  || m_TransmitMode == QuickNoteTransmitMode::Both)
        core.ClearQuickNoteLAN();
}

std::string QuickNotes::GetName() const { return "QuickNotes"; }

static ImU32 ColU32(float r, float g, float b, float a = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
}

// ── SyncTransmission ──────────────────────────────────────────────────────
// Reemplaza al viejo PushToCore(): ahora respeta el modo de transmision
// (Off/MainOnly/LANOnly/Both), igual que OClock::SyncTransmission, y aplica
// el estilo predeterminado elegido por el usuario justo antes de publicar.
void QuickNotes::SyncTransmission() {
    auto& core = Core::PresentationCore::Get();
    std::string text = std::string(m_TextBuffer.data());

    bool wasMain = (m_PrevTransmitMode == QuickNoteTransmitMode::MainOnly || m_PrevTransmitMode == QuickNoteTransmitMode::Both);
    bool wasLAN  = (m_PrevTransmitMode == QuickNoteTransmitMode::LANOnly  || m_PrevTransmitMode == QuickNoteTransmitMode::Both);
    bool isMain  = (m_TransmitMode     == QuickNoteTransmitMode::MainOnly || m_TransmitMode     == QuickNoteTransmitMode::Both);
    bool isLAN   = (m_TransmitMode     == QuickNoteTransmitMode::LANOnly  || m_TransmitMode     == QuickNoteTransmitMode::Both);

    if ((isMain || isLAN) && !m_StyleName.empty())
        core.ApplyStyleByName(m_StyleName);

    if (isMain) {
        if (text.empty()) core.ClearQuickNote();
        else               core.SetLiveQuickNote(text);
    } else if (wasMain) {
        core.ClearQuickNote();
    }

    if (isLAN) {
        if (text.empty()) core.ClearQuickNoteLAN();
        else               core.SetLiveQuickNoteLAN(text);
    } else if (wasLAN) {
        core.ClearQuickNoteLAN();
    }

    m_PrevTransmitMode = m_TransmitMode;
}

void QuickNotes::ClearFromCore() {
    auto& core = Core::PresentationCore::Get();
    core.ClearQuickNote();
    core.ClearQuickNoteLAN();
    m_TransmitMode     = QuickNoteTransmitMode::Off;
    m_PrevTransmitMode = QuickNoteTransmitMode::Off;
}

// ── RenderStyleSelector ────────────────────────────────────────────────────
void QuickNotes::RenderStyleSelector() {
    auto& core = Core::PresentationCore::Get();
    std::vector<std::string> styleNames = core.GetSavedStyleNames();
    std::string preview = m_StyleName.empty() ? "Usar estilo actual" : m_StyleName;

    ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.68f, 1.0f), "Estilo para el público");

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.05f, 0.09f, 0.13f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.07f, 0.12f, 0.17f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
    ImGui::SetNextItemWidth(-1.0f);

    if (ImGui::BeginCombo("##quickNoteStyle", preview.c_str())) {
        bool noneSelected = m_StyleName.empty();
        if (ImGui::Selectable("Usar estilo actual", noneSelected))
            m_StyleName.clear();
        for (const auto& name : styleNames) {
            bool sel = (m_StyleName == name);
            if (ImGui::Selectable(name.c_str(), sel))
                m_StyleName = name;
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

// ── RenderTransmitCards ──────────────────────────────────────────────────
// Mismo patron de "tarjetas" que OClock::Render usa para TRANSMITIR, pero
// autocontenido aqui (QuickNotes no comparte GlassRenderer con OClock).
void QuickNotes::RenderTransmitCards() {
    auto& core       = Core::PresentationCore::Get();
    bool  netAvailable = core.IsStreamingNet();

    struct ModeOpt { const char* label; QuickNoteTransmitMode mode; bool needsNet; };
    ModeOpt opts[4] = {
        { "Apagado",  QuickNoteTransmitMode::Off,      false },
        { "Pantalla", QuickNoteTransmitMode::MainOnly, false },
        { "Solo LAN", QuickNoteTransmitMode::LANOnly,  true  },
        { "Ambos",    QuickNoteTransmitMode::Both,     true  },
    };

    float w      = ImGui::GetContentRegionAvail().x;
    float gap    = 6.0f;
    float cardW  = (w - gap * 3.0f) / 4.0f;

    for (int i = 0; i < 4; ++i) {
        auto& opt = opts[i];
        bool disabled = opt.needsNet && !netAvailable;
        bool active   = (m_TransmitMode == opt.mode) && !disabled;

        if (i > 0) ImGui::SameLine(0, gap);

        ImVec4 bg = active ? ImVec4(0.20f, 0.55f, 0.30f, 0.35f) : ImVec4(1,1,1,0.04f);
        ImGui::PushStyleColor(ImGuiCol_Button,        bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.60f, 0.35f, 0.45f));
        ImGui::PushStyleColor(ImGuiCol_Text,
            disabled ? ImVec4(0.4f,0.4f,0.4f,1.0f) : ImVec4(0.85f,0.9f,0.9f,1.0f));

        ImGui::BeginDisabled(disabled);
        if (ImGui::Button(opt.label, ImVec2(cardW, 34.0f)))
            m_TransmitMode = opt.mode;
        ImGui::EndDisabled();

        ImGui::PopStyleColor(3);
    }

    if (!netAvailable) {
        ImGui::TextColored(ImVec4(0.55f,0.58f,0.68f,1.0f),
            "Inicia el servidor en el panel de Transmisión para habilitar \"Solo LAN\" / \"Ambos\".");
        if (m_TransmitMode == QuickNoteTransmitMode::LANOnly) m_TransmitMode = QuickNoteTransmitMode::Off;
        if (m_TransmitMode == QuickNoteTransmitMode::Both)    m_TransmitMode = QuickNoteTransmitMode::MainOnly;
    }
}

void QuickNotes::Render() {
    const auto& str = ProyecThor::UI::GetUIStrings();
    auto& core      = Core::PresentationCore::Get();

    bool forceUpdate = false;
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        if (ImGui::IsKeyPressed(ImGuiKey_F5) && m_TransmitMode == QuickNoteTransmitMode::Off) {
            m_TransmitMode = QuickNoteTransmitMode::MainOnly;
            forceUpdate    = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) && m_TransmitMode != QuickNoteTransmitMode::Off) {
            ClearFromCore();
        }
    }

    bool isLive = (m_TransmitMode != QuickNoteTransmitMode::Off);

    if (isLive) {
        ImGui::SameLine();
        float tw = ImGui::CalcTextSize(str.liveIndicator).x + 18.0f;
        ImGui::SameLine(ImGui::GetWindowWidth() - tw - 16.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
        ImGui::TextUnformatted("● ");
        ImGui::SameLine(0, 2);
        ImGui::TextUnformatted(str.liveIndicator);
        ImGui::PopStyleColor();
    }

    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.65f, 0.15f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.78f, 0.20f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.50f, 0.10f, 0.10f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    if (ImGui::Button(str.songClearScreen, ImVec2(-1, 35))) {
        m_TextBuffer.fill('\0');
        ClearFromCore();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    ImGui::Separator();
    ImGui::Spacing();

    ImVec4 inputBg = isLive
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

    RenderStyleSelector();
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.55f,0.58f,0.68f,1.0f), "Transmitir");
    RenderTransmitCards();

    ImGui::Spacing();

    if (textChanged || forceUpdate)
        SyncTransmission();
}

} // namespace ProyecThor::UI