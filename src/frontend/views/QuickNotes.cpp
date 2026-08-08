#include "QuickNotes.h"
#include "backend/core/PresentationCore.h"
#include "backend/settings/SettingsManager.h"
#include "frontend/ui/UIStrings.h"
#include "frontend/ui/DesignSystem.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cstring>
#include <cstdio>

namespace ProyecThor::UI {

QuickNotes::QuickNotes() : m_IsLive(false) {
    m_TextBuffer.fill('\0');
    LoadPersisted();
}

QuickNotes::~QuickNotes() {
    // Al destruirse, limpiar cualquier transmision activa (principal o LAN)
    auto& core = Core::PresentationCore::Get();
    if (m_TransmitMode == QuickNoteTransmitMode::MainOnly || m_TransmitMode == QuickNoteTransmitMode::Both)
        core.ClearQuickNote();
    if (m_TransmitMode == QuickNoteTransmitMode::LANOnly  || m_TransmitMode == QuickNoteTransmitMode::Both)
        core.ClearQuickNoteLAN();

    // Ultima red de seguridad -- cubre un cierre de la app sin pasar por
    // UIManager::RenderNotesWindow (que ya persiste al cerrar la ventana).
    PersistNow();
}

std::string QuickNotes::GetName() const { return "QuickNotes"; }

// ── Persistencia (Ajustes > General, campo general.quickNotesText) ────────
// Evita que el operador pierda lo que iba tipeando si cierra la ventana o la
// app sin borrar el texto a mano -- ver comentario largo en QuickNotes.h.
void QuickNotes::LoadPersisted() {
    const std::string& saved = ProyecThor::Settings::SettingsManager::Get().GetSettings().general.quickNotesText;
    size_t n = std::min(saved.size(), m_TextBuffer.size() - 1);
    std::copy(saved.begin(), saved.begin() + n, m_TextBuffer.begin());
    m_TextBuffer[n] = '\0';
}

void QuickNotes::PersistNow() {
    auto& general = ProyecThor::Settings::SettingsManager::Get().GetSettings().general;
    std::string text(m_TextBuffer.data());
    if (general.quickNotesText == text) return; // nada nuevo que guardar
    general.quickNotesText = text;
    ProyecThor::Settings::SettingsManager::Get().Save();
    m_LastPersistTime = ImGui::GetTime();
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

    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextSecondary), "Estilo para el público");

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
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

    ImVec4 successCol = ImGui::ColorConvertU32ToFloat4(DS::SuccessColor);

    for (int i = 0; i < 4; ++i) {
        auto& opt = opts[i];
        bool disabled = opt.needsNet && !netAvailable;
        bool active   = (m_TransmitMode == opt.mode) && !disabled;

        if (i > 0) ImGui::SameLine(0, gap);

        ImVec4 bg = active
            ? ImVec4(successCol.x, successCol.y, successCol.z, 0.30f)
            : ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill);
        ImVec4 bgHover = ImVec4(successCol.x, successCol.y, successCol.z, 0.42f);

        ImGui::PushStyleColor(ImGuiCol_Button,        bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bgHover);
        ImGui::PushStyleColor(ImGuiCol_Text,
            disabled ? ImGui::ColorConvertU32ToFloat4(DS::TextHint)
                     : ImGui::ColorConvertU32ToFloat4(DS::TextPrimary));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);

        ImGui::BeginDisabled(disabled);
        if (ImGui::Button(opt.label, ImVec2(cardW, 34.0f)))
            m_TransmitMode = opt.mode;
        ImGui::EndDisabled();

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    }

    if (!netAvailable) {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextSecondary),
            "Inicia el servidor en el panel de Transmisión para habilitar \"Solo LAN\" / \"Ambos\".");
        if (m_TransmitMode == QuickNoteTransmitMode::LANOnly) m_TransmitMode = QuickNoteTransmitMode::Off;
        if (m_TransmitMode == QuickNoteTransmitMode::Both)    m_TransmitMode = QuickNoteTransmitMode::MainOnly;
    }
}

void QuickNotes::Render() {
    const auto& str = ProyecThor::UI::GetUIStrings();

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

    // ── Encabezado: titulo + descripcion breve, mismas cadenas localizadas
    //    que ya existian sin usar (ver UIStrings.h) ─────────────────────────
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextPrimary), "%s", str.quickNotesTitle);

    if (isLive) {
        float tw = ImGui::CalcTextSize(str.liveIndicator).x + 18.0f;
        ImGui::SameLine(ImGui::GetWindowWidth() - tw - 16.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::SuccessColor));
        ImGui::TextUnformatted("● ");
        ImGui::SameLine(0, 2);
        ImGui::TextUnformatted(str.liveIndicator);
        ImGui::PopStyleColor();
    }

    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextSecondary), "%s", str.quickNotesDesc);
    ImGui::PopTextWrapPos();

    ImGui::Spacing();
    DS::GlassSeparator();
    ImGui::Spacing();

    ImVec4 baseInputBg = ImGui::ColorConvertU32ToFloat4(DS::GlassFillBot);
    ImVec4 successCol  = ImGui::ColorConvertU32ToFloat4(DS::SuccessColor);
    ImVec4 inputBg = isLive
        ? ImVec4(baseInputBg.x * 0.6f + successCol.x * 0.10f,
                  baseInputBg.y * 0.6f + successCol.y * 0.10f,
                  baseInputBg.z * 0.6f + successCol.z * 0.10f, 1.0f)
        : baseInputBg;

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        inputBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, inputBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  inputBg);
    ImGui::PushStyleColor(ImGuiCol_Border,         ImGui::ColorConvertU32ToFloat4(isLive ? DS::SuccessColor : DS::GlassBorder));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusLarge);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    float inputH = ImGui::GetTextLineHeight() * 7.0f + ImGui::GetStyle().FramePadding.y * 2.0f;
    bool textChanged = ImGui::InputTextMultiline(
        "##QuickNoteInput",
        m_TextBuffer.data(),
        m_TextBuffer.size(),
        ImVec2(-FLT_MIN, inputH));

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);

    // ── Pie: contador de caracteres + estado de autoguardado ───────────────
    {
        size_t len = std::strlen(m_TextBuffer.data());
        char counter[32];
        std::snprintf(counter, sizeof(counter), "%zu / %zu", len, m_TextBuffer.size() - 1);

        double sinceSave = ImGui::GetTime() - m_LastPersistTime;
        const char* saveState = (sinceSave < 1.2) ? "Guardado" : "Autoguardado activo";

        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextHint), "%s", saveState);
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(counter).x - 16.0f);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextHint), "%s", counter);
    }

    ImGui::Spacing();
    // GlassButton, a diferencia de ImGui::Button, no interpreta size.x<=0
    // como "ancho completo" (lo autoajusta al texto) -- se pasa el ancho
    // disponible explicito para que ocupe todo el panel, igual que antes.
    if (DS::GlassButton(str.hideMessage, ImVec2(ImGui::GetContentRegionAvail().x, DS::ButtonHeight + 3.0f), DS::DangerColor)) {
        m_TextBuffer.fill('\0');
        ClearFromCore();
    }

    ImGui::Spacing();
    DS::GlassSeparator();
    ImGui::Spacing();

    RenderStyleSelector();
    ImGui::Spacing();

    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextSecondary), "Transmitir");
    RenderTransmitCards();

    ImGui::Spacing();

    if (textChanged || forceUpdate)
        SyncTransmission();

    // Autoguardado con debounce -- ver comentario en QuickNotes.h. Nunca deja
    // pasar mas de ~1.5s de tipeo continuo sin persistir a disco.
    if (textChanged && (ImGui::GetTime() - m_LastPersistTime) > 1.5)
        PersistNow();
}

} // namespace ProyecThor::UI
