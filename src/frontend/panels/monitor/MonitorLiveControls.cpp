#include "MonitorView.h"
#include "MonitorTheme.h"
#include "backend/core/PresentationCore.h"
#include "backend/media/VLCBasePlayer.h"
#include "StyleGeneralApp.h"
#include <imgui.h>
#include <algorithm>
#include <cstdint>
#include <cmath>

#include "MonitorDesign.h"
#include "MonitorUIHelpers.h"

namespace ProyecThor::UI {

namespace MT = MonitorTheme;
using namespace Design;
using namespace Components;

void MonitorView::RenderLiveControls(Core::VLCBasePlayer* /*unused*/, float w)
{
    Core::VLCBasePlayer* bg = Core::PresentationCore::Get().GetBackgroundPlayer();
    m_LiveMuted  = Core::PresentationCore::Get().GetLiveMute();
    m_LiveVolume = static_cast<float>(Core::PresentationCore::Get().GetLiveVolume()) * 0.01f;
    m_LivePlaying = bg && !bg->IsPaused();

    int64_t liveLenMs = bg ? bg->GetLength() : 0;

    if (m_LivePlaying && bg && liveLenMs > 0)
    {
        int64_t curMs = bg->GetTime();
        float   fpos  = (liveLenMs > 0)
            ? static_cast<float>(curMs) / static_cast<float>(liveLenMs)
            : 0.0f;

        bool nearEnd = (fpos >= 0.995f);

        if (nearEnd && m_LoopEnabled)
        {
            bg->SetPosition(0.0f);
            bg->SetPause(false);
        }
    }

    // ── Contenedor del panel ──────────────────────────────────────────────────
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  { MT::k_PadLg, MT::k_Pad });
    ImGui::PushStyleColor(ImGuiCol_ChildBg, MT::k_Bg1);
    ImGui::PushStyleColor(ImGuiCol_Border,  MT::k_BorderSubtle);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   MT::k_R);

    ImGui::BeginChild("##ctrl_live", { w, MT::k_ControlsH }, true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const float innerW = w - MT::k_PadLg * 2.0f;

    // ── Cabecera PGM ──────────────────────────────────────────────────────────
    {
        ImVec2 headerPos = ImGui::GetCursorScreenPos();
        DrawStatusDot(
            ImGui::GetWindowDrawList(),
            { headerPos.x + 7.0f, headerPos.y + 9.0f },
            4.5f, MT::k_LiveAccent, m_LivePlaying);

        ImGui::SetCursorPosX(MT::k_PadLg + 20.0f);
        ImGui::PushStyleColor(ImGuiCol_Text,
            m_LivePlaying ? MT::k_LiveAccent : MT::k_TextSecondary);
        ImGui::TextUnformatted(m_LivePlaying ? "PROGRAM  —  ON AIR" : "PROGRAM");
        ImGui::PopStyleColor();
    }

    DrawAccentLine(innerW, MT::k_LiveAccentDim, 1.0f);

    // ── Barra de progreso ─────────────────────────────────────────────────────
    int64_t liveCurMs = bg ? bg->GetTime()   : 0;
    int64_t liveLen   = bg ? bg->GetLength() : 0;
    float   livePos   = (liveLen > 0)
        ? std::clamp(static_cast<float>(liveCurMs) / static_cast<float>(liveLen), 0.0f, 1.0f)
        : 0.0f;

    ImGui::Spacing();
    ImGui::SetCursorPosX(MT::k_PadLg);

    float displayPos = livePos;
    if (BMSlider("##tl_live", &displayPos, 0.0f, 1.0f, "",
                 MT::k_LiveTrack, MT::k_LiveGrab,
                 { MT::k_LiveGrab.x * 1.1f, MT::k_LiveGrab.y * 1.1f, MT::k_LiveGrab.z * 1.1f, 1.0f },
                 innerW))
    {
        Core::PresentationCore::Get().SetLivePosition(displayPos);
        liveCurMs = static_cast<int64_t>(displayPos * static_cast<float>(liveLen));
        livePos   = displayPos;
    }

    DrawTimeRow(innerW, MT::k_PadLg, liveCurMs, liveLen);
    ImGui::Spacing();

    float gap = MT::k_Gap;
    float btnH = MT::k_TransportH;
    float navBtnW = (innerW - (gap * 2.0f)) / 3.0f;
    float iconSize = 16.0f;

    ImGui::Spacing();

    // FILA 1: Controles de Navegación (-10s | +10s | STOP)
    ImGui::PushID("btn_replay");
    if (DrawIconButton("replay_10", iconSize, MT::k_NeutBtn, MT::k_NeutBtnHov, MT::k_NeutBtnAct, {navBtnW, btnH})) {
        float np = livePos - (liveLen > 0 ? 10000.0f / static_cast<float>(liveLen) : 0.0f);
        Core::PresentationCore::Get().SetLivePosition(std::max(0.0f, np));
    }
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    ImGui::PushID("btn_fwd");
    if (DrawIconButton("forward_10", iconSize, MT::k_NeutBtn, MT::k_NeutBtnHov, MT::k_NeutBtnAct, {navBtnW, btnH})) {
        float np = livePos + (liveLen > 0 ? 10000.0f / static_cast<float>(liveLen) : 0.0f);
        Core::PresentationCore::Get().SetLivePosition(std::min(1.0f, np));
    }
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    ImGui::PushID("btn_stop");
    if (DrawIconButton("stop", iconSize, MT::k_NeutBtn, MT::k_NeutBtnHov, MT::k_NeutBtnAct, {navBtnW, btnH})) {
        Core::PresentationCore::Get().SetLivePosition(0.0f);
        if (bg) { bg->SetPosition(0.0f); bg->SetPause(true); }

    }
    ImGui::PopID();

    ImGui::Spacing();

    // FILA 2: Botón principal de PLAY / PAUSA
    const char* iconToUse = m_LivePlaying ? "pause" : "play";
    ImGui::PushID("btn_main_transport");
    if (DrawIconButton(iconToUse, 24.0f, MT::k_LiveBtn, MT::k_LiveBtnHov, MT::k_LiveBtnAct, {innerW, btnH * 1.2f}, m_LivePlaying)) {
        if (bg) {
            if (m_LivePlaying) {
                bg->SetPause(true);
            } else {
                Core::PresentationCore::Get().SetLiveMute(m_LiveMuted);
                Core::PresentationCore::Get().SetLiveVolume(
                    m_LiveMuted ? 0 : static_cast<int>(m_LiveVolume * 100.0f));
                bg->SetPause(false);
                m_PreviewPlaying = false;
            }
        }
        // Igual que en STOP: el icono cambia solo, porque m_LivePlaying se
        // recalcula desde bg->IsPaused() al principio de la funcion.
    }
    ImGui::PopID();

    ImGui::Spacing();

    // ── Volumen ───────────────────────────────────────────────────────────────
    ImGui::SetCursorPosX(MT::k_PadLg);
    bool isDanger = (m_LiveVolume > 1.0f);
    float volBtnW = btnH;
    float sliderW = innerW - volBtnW - gap;

    ImGui::PushID("btn_mute");
    const char* volIcon = m_LiveMuted ? "no_sound" : "volume_up";
    ImVec4 volBtnBg = isDanger ? ImVec4(0.36f, 0.08f, 0.08f, 1.0f) : MT::k_NeutBtn;

    if (DrawIconButton(volIcon, 16.0f, volBtnBg, MT::k_NeutBtnHov, MT::k_NeutBtnAct, {volBtnW, btnH}, m_LiveMuted)) {
        m_LiveMuted = !m_LiveMuted;

        // Se aplica siempre, este o no en reproduccion: BackgroundLayer
        // guarda el target y lo aplica de inmediato si ya esta al aire
        // (SetProjecting(true) previo), o lo deja listo para cuando lo
        // este. Antes esto solo corria si m_LivePlaying era true, lo que
        // dejaba el mute desincronizado si se tocaba en pausa.
        Core::PresentationCore::Get().SetLiveMute(m_LiveMuted);
        Core::PresentationCore::Get().SetLiveVolume(
            m_LiveMuted ? 0 : static_cast<int>(m_LiveVolume * 100.0f));
    }
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    ImVec4 slBg   = isDanger ? ImVec4(0.36f, 0.08f, 0.08f, 1.0f) : MT::k_NeutBtn;
    ImVec4 slGrab = isDanger ? ImVec4(0.92f, 0.20f, 0.20f, 1.0f) : MT::k_LiveGrab;
    ImVec4 slAct  = isDanger ? ImVec4(1.00f, 0.30f, 0.30f, 1.0f) : ImVec4(MT::k_LiveGrab.x * 1.1f, MT::k_LiveGrab.y * 1.1f, MT::k_LiveGrab.z * 1.1f, 1.0f);

    if (BMSlider("##vol_l", &m_LiveVolume, 0.0f, 2.0f, "", slBg, slGrab, slAct, sliderW)) {
        // Mismo criterio que el boton de mute: siempre se aplica.
        Core::PresentationCore::Get().SetLiveVolume(
            m_LiveMuted ? 0 : static_cast<int>(m_LiveVolume * 100.0f));
    }

    ImGui::Spacing();

    // ── Medidores VU ──────────────────────────────────────────────────────────
    ImGui::SetCursorPosX(0.0f);
    m_AudioMeters.Update(bg, true, m_LivePlaying, m_LiveMuted, m_LiveVolume);
    m_AudioMeters.Render(innerW, MT::k_Meters_H);

    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

} // namespace ProyecThor::UI