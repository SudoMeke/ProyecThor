#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <imgui.h>
#include "backend/media/VLCBasePlayer.h"
#include "MonitorQueueEngine.h"

namespace ProyecThor::UI {

class MonitorView {
public:
    MonitorView()  = default;
    ~MonitorView() = default;

    void Render(Core::VLCBasePlayer* player);

    // Avanza la cola (detecta fin de clip real via VLC y pasa al siguiente
    // item) sin importar si este panel esta visible. IMPORTANTE: debe
    // llamarse UNA VEZ POR FRAME de forma incondicional (ver HomePanel::
    // Render(), junto a OClock::Update()) — antes esto solo corria dentro
    // de RenderQueue(), que solo se ejecuta con "Home" activo Y un video
    // seleccionado; en cuanto el operador miraba otra pestaña o
    // seleccionaba una cancion/pasaje mientras la cola reproducia, dejaba
    // de detectar el fin del clip y se quedaba pegada en el mismo video
    // para siempre.
    void Update();

    void AddToQueue(const std::string& fullPath);
    void AddURLToQueue(const std::string& url);

    void LoadPlayQueue();
    void SavePlayQueue();

private:
    void RenderPreviewMonitor(Core::VLCBasePlayer* player, float w, float h);
    void RenderCenterColumn(float w, float h, Core::VLCBasePlayer* previewPlayer);
    void RenderPreviewControls(Core::VLCBasePlayer* player, float w);
    void RenderQueue(float totalW);

    bool DrawIconButton(const char* iconName, float size,
                        ImVec4 bgCol, ImVec4 hov, ImVec4 act,
                        ImVec2 btnSize, bool isActiveState = false);

    // Punto unico de entrada para reproducir un indice de la cola desde la
    // UI. Mantiene m_LivePlaying sincronizado para el resto de paneles
    // (RenderCenterColumn, RenderPreviewControls) que aun lo consultan.
    void PlayQueueItem(int index);

    bool  m_Initialized    = false;
    bool  m_PreviewPlaying = false;
    // El monitor "PGM"/Live y sus controles (transporte + VU meters) se
    // movieron a ViewPanel::RenderLiveTransport (pantallas chicas dejaban el
    // Monitor demasiado apretado). m_LivePlaying/m_LiveMuted/m_LiveVolume
    // siguen viviendo aca porque RenderCenterColumn (boton TRANSMITIR) y
    // RenderPreviewControls (deshabilitar scrubbing si comparte player con
    // el live) todavia los necesitan — se refrescan cada frame al principio
    // de Render() en vez de en la ahora-inexistente RenderLiveControls.
    bool  m_LivePlaying    = false;
    bool  m_LiveMuted      = false;
    float m_LiveVolume     = 0.8f;

    // ── Cola (logica real en MonitorQueueEngine) ────────────────────────────
    MonitorQueueEngine m_QueueEngine;
    int m_DragSrcIndex = -1; // solo feedback visual mientras se arrastra
};

} // namespace ProyecThor::UI