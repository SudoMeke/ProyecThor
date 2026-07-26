#include <GL/glew.h>
#include "BroadcastPanel.h"
#include "AppIcons.h"
#include "IconRail.h"
#include "backend/settings/SettingsManager.h"
#include "home/HomeIcons.h"
#include <imgui.h>
#include <algorithm>
#include <cstdio>

namespace ProyecThor::UI {

BroadcastPanel::~BroadcastPanel() {
    m_Encoder.Stop();
}

void BroadcastPanel::RenderRail() {
    static const IconRailItem kItems[] = {
        { (int)Section::Capture, HomeIcons::DrawIcon_Camera,    "Capture" },
        { (int)Section::Layer,   AppIcons::DrawIcon_Layers,     "Layer"   },
        { (int)Section::Start,   HomeIcons::DrawIcon_Broadcast, "Iniciar" },
    };
    static const float kColors[3][4] = {
        { 0.90f, 0.35f, 0.45f, 1.0f }, // Capture
        { 0.35f, 0.80f, 0.55f, 1.0f }, // Layer
        { 0.90f, 0.28f, 0.28f, 1.0f }, // Iniciar
    };

    float railW = IconRailThickness(true);
    ImGui::BeginChild("##broadcastRail", ImVec2(railW, 0.0f), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    int currentIndex = (int)m_Section;
    RenderIconRail(kItems, 3, currentIndex, IconRailOrientation::Vertical, kColors);
    m_Section = (Section)currentIndex;

    ImGui::EndChild();
}

void BroadcastPanel::RenderCaptureSection() {
    m_Capture.RenderContent();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    bool live = m_Capture.IsLive();
    if (!live) ImGui::BeginDisabled();

    ImGui::PushStyleColor(ImGuiCol_Button, m_ShowInLayer
        ? ImVec4(0.20f, 0.66f, 0.40f, 1.0f) : ImVec4(0.14f, 0.15f, 0.22f, 1.0f));
    if (ImGui::Button(m_ShowInLayer ? "Mostrando en Layer" : "Mostrar en Layer", ImVec2(220, 36)))
        m_ShowInLayer = !m_ShowInLayer;
    ImGui::PopStyleColor();

    if (!live) {
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("Prende una fuente de captura primero.");
    }
}

void BroadcastPanel::RenderLayerSection() {
    ImGui::TextUnformatted("Layer");
    ImGui::SameLine();
    ImGui::TextDisabled("(esto es lo que se transmite)");
    ImGui::Spacing();

    float avail = ImGui::GetContentRegionAvail().x;
    float h     = avail * 9.0f / 16.0f;
    ImVec2 pos  = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(pos, { pos.x + avail, pos.y + h }, IM_COL32(10, 11, 16, 255), 8.0f);
    dl->AddRect(pos, { pos.x + avail, pos.y + h }, IM_COL32(60, 63, 80, 255), 8.0f);

    if (m_ShowInLayer && m_Capture.IsLive()) {
        void* tex = m_Capture.GetPreviewTexture();
        int   fw  = m_Capture.GetFrameWidth();
        int   fh  = m_Capture.GetFrameHeight();
        if (tex && fw > 0 && fh > 0) {
            float srcR = (float)fw / (float)fh;
            float dstR = avail / h;
            float dw = avail, dh = h, ox = pos.x, oy = pos.y;
            if (srcR > dstR) { dh = avail / srcR; oy += (h - dh) * 0.5f; }
            else             { dw = h * srcR;     ox += (avail - dw) * 0.5f; }
            dl->AddImage(tex, { ox, oy }, { ox + dw, oy + dh });
        }
    } else {
        ImVec2 ts = ImGui::CalcTextSize("Sin fuente todavia -- anda a Capture y activa \"Mostrar en Layer\".");
        dl->AddText({ pos.x + (avail - ts.x) * 0.5f, pos.y + (h - ts.y) * 0.5f },
                    IM_COL32(120, 122, 140, 255), "Sin fuente todavia -- anda a Capture y activa \"Mostrar en Layer\".");
    }

    ImGui::Dummy({ avail, h });
}

void BroadcastPanel::RenderStartSection() {
    auto& s = ProyecThor::Settings::SettingsManager::Get().GetSettings().streaming;

    ImGui::TextUnformatted("Configuracion de la transmision");
    ImGui::Spacing();

    bool streaming = m_Encoder.IsStreaming();
    if (streaming) ImGui::BeginDisabled();

    static char serverBuf[256];
    static char keyBuf[256];
    static bool buffersInit = false;
    if (!buffersInit) {
        std::snprintf(serverBuf, sizeof(serverBuf), "%s", s.serverUrl.c_str());
        std::snprintf(keyBuf, sizeof(keyBuf), "%s", s.streamKey.c_str());
        buffersInit = true;
    }

    ImGui::SetNextItemWidth(400.0f);
    if (ImGui::InputText("Servidor (rtmp://...)", serverBuf, sizeof(serverBuf)))
        s.serverUrl = serverBuf;

    ImGui::SetNextItemWidth(400.0f);
    if (ImGui::InputText("Clave de stream", keyBuf, sizeof(keyBuf), ImGuiInputTextFlags_Password))
        s.streamKey = keyBuf;

    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputInt("Bitrate (kbps)", &s.videoBitrateKbps, 100);
    s.videoBitrateKbps = std::clamp(s.videoBitrateKbps, 500, 20000);

    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputInt("FPS", &s.fps, 1);
    s.fps = std::clamp(s.fps, 10, 60);

    if (streaming) ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::TextDisabled("La resolucion de salida sigue a la fuente de Capture activa (no hay escalado).");
    ImGui::Spacing();

    if (!m_StatusMessage.empty()) {
        ImGui::TextColored(m_StatusIsError ? ImVec4(0.90f, 0.35f, 0.35f, 1.0f) : ImVec4(0.40f, 0.85f, 0.55f, 1.0f),
                            "%s", m_StatusMessage.c_str());
        ImGui::Spacing();
    }

    if (!streaming) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.20f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.28f, 0.28f, 1.0f));
        if (ImGui::Button("Iniciar transmision", ImVec2(220, 40))) {
            ProyecThor::Settings::SettingsManager::Get().Save();

            if (!m_ShowInLayer || !m_Capture.IsLive()) {
                m_StatusIsError = true;
                m_StatusMessage = "Anda a Capture, prende una fuente y activa \"Mostrar en Layer\" antes de iniciar.";
            } else {
                std::string url = s.serverUrl;
                if (!url.empty() && url.back() != '/') url += "/";
                url += s.streamKey;

                std::string err;
                bool ok = m_Encoder.Start(url, m_Capture.GetFrameWidth(), m_Capture.GetFrameHeight(),
                                           s.fps, s.videoBitrateKbps, &err);
                m_StatusIsError = !ok;
                m_StatusMessage = ok ? "Transmitiendo." : err;
            }
        }
        ImGui::PopStyleColor(2);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.30f, 0.32f, 0.40f, 1.0f));
        if (ImGui::Button("Detener transmision", ImVec2(220, 40))) {
            m_Encoder.Stop();
            m_StatusIsError = false;
            m_StatusMessage = "Transmision detenida.";
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.90f, 0.30f, 0.30f, 1.0f), "EN VIVO");
    }
}

void BroadcastPanel::PumpEncoder() {
    if (!m_Encoder.IsStreaming()) return;
    if (!m_ShowInLayer || !m_Capture.IsLive()) return;

    void* texVoid = m_Capture.GetPreviewTexture();
    if (!texVoid) return;

    int w = m_Capture.GetFrameWidth();
    int h = m_Capture.GetFrameHeight();
    if (w <= 0 || h <= 0) return;

    size_t need = (size_t)w * (size_t)h * 4;
    if (m_ReadbackBuffer.size() != need) m_ReadbackBuffer.resize(need);

    GLuint tex = (GLuint)(intptr_t)texVoid;
    GLint  prevTex = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);

    glBindTexture(GL_TEXTURE_2D, tex);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_ReadbackBuffer.data());
    glBindTexture(GL_TEXTURE_2D, (GLuint)prevTex);

    m_Encoder.PushFrame(m_ReadbackBuffer.data(), w, h);
}

void BroadcastPanel::Render() {
    PumpEncoder();

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove       |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("##BroadcastRoot", nullptr, flags);
    ImGui::BeginChild("##broadcastContent", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AlwaysUseWindowPadding);

    ImGui::TextUnformatted("Streaming");
    ImGui::SameLine();
    ImGui::TextDisabled("(transmision en vivo por RTMP)");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    RenderRail();
    ImGui::SameLine();
    ImGui::BeginChild("##broadcastSection", ImVec2(0.0f, 0.0f));

    switch (m_Section) {
        case Section::Capture: RenderCaptureSection(); break;
        case Section::Layer:   RenderLayerSection();   break;
        case Section::Start:   RenderStartSection();   break;
    }

    ImGui::EndChild();
    ImGui::EndChild();
    ImGui::End();
}

} // namespace ProyecThor::UI
