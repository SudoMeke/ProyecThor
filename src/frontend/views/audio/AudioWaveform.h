#pragma once
// AudioWaveform.h — Waveform animado para la pantalla de proyeccion de audio.
// Dibuja barras sincronizadas con el estado de AudioPanel usando ImDrawList.

#include <imgui.h>
#include <vector>

namespace ProyecThor::Audio {

struct WaveformRenderer
{
    // Renderiza el waveform usando los datos de barras y el hue de acento.
    // origin  = esquina superior izquierda del area de dibujo (screen coords)
    // size    = ancho x alto del area
    // bars    = amplitudes normalizadas [0..1], una por barra
    // hue     = tono HSV [0..1] para colorear las barras
    // time    = tiempo acumulado en segundos (para animacion de pulso)
    // playing = true si hay reproduccion activa (afecta opacidad y velocidad)
    static void Draw(ImDrawList*               dl,
                     ImVec2                    origin,
                     ImVec2                    size,
                     const std::vector<float>& bars,
                     float                     hue,
                     float                     time,
                     bool                      playing);

    // Version simplificada para el modo fullscreen del proyector.
    // Dibuja las barras centradas verticalmente en el area dada,
    // con mayor altura y sin recorte de clip.
    static void DrawProjector(ImDrawList*               dl,
                               ImVec2                    origin,
                               ImVec2                    size,
                               const std::vector<float>& bars,
                               float                     hue,
                               float                     time,
                               bool                      playing);
};

} // namespace ProyecThor::Audio