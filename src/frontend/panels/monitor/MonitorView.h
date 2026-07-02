#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <imgui.h>
#include "backend/media/VLCBasePlayer.h"
#include "AudioMeters.h"
#include "MonitorQueueEngine.h"

namespace ProyecThor::UI {

class MonitorView {
public:
    MonitorView()  = default;
    ~MonitorView() = default;

    void Render(Core::VLCBasePlayer* player);

    void AddToQueue(const std::string& fullPath);
    void AddURLToQueue(const std::string& url);

    void LoadPlayQueue();
    void SavePlayQueue();

private:
    void RenderPreviewMonitor(Core::VLCBasePlayer* player, float w, float h);
    void RenderLiveMonitor(Core::VLCBasePlayer* player, float w, float h);
    void RenderCenterColumn(float w, float h, Core::VLCBasePlayer* previewPlayer);
    void RenderPreviewControls(Core::VLCBasePlayer* player, float w);
    void RenderLiveControls(Core::VLCBasePlayer* player, float w);
    void RenderQueue(float totalW);

    bool DrawIconButton(const char* iconName, float size,
                        ImVec4 bgCol, ImVec4 hov, ImVec4 act,
                        ImVec2 btnSize, bool isActiveState = false);

    // Punto unico de entrada para reproducir un indice de la cola desde la
    // UI. Mantiene m_LivePlaying sincronizado para el resto de paneles
    // (RenderLiveMonitor, RenderLiveControls) que aun lo consultan.
    void PlayQueueItem(int index);

    bool  m_Initialized    = false;
    bool  m_PreviewPlaying = false;
    bool  m_LivePlaying    = false;
    bool  m_LiveMuted      = false;
    float m_LiveVolume     = 0.8f;
    bool  m_LoopEnabled    = false;

    // ── Cola (logica real en MonitorQueueEngine) ────────────────────────────
    MonitorQueueEngine m_QueueEngine;
    int m_DragSrcIndex = -1; // solo feedback visual mientras se arrastra

    AudioMeters m_AudioMeters;
};

} // namespace ProyecThor::UI