#pragma once
#include <string>
#include <vector>

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  OverlayLayerKind — de que tipo es una capa dentro del canvas.
// ─────────────────────────────────────────────────────────────────────────────
enum class OverlayLayerKind { Text, Image };

// ─────────────────────────────────────────────────────────────────────────────
//  OverlayLayer — una capa (texto o imagen) dentro de un Overlay (canvas tipo
//  Canva). La posicion es el CENTRO de la capa, normalizada 0..1 respecto al
//  canvas (asi el overlay se ve igual sin importar la resolucion de
//  exportacion). Para imagenes, sizeW/sizeH tambien son fracciones 0..1 del
//  canvas.
// ─────────────────────────────────────────────────────────────────────────────
struct OverlayLayer {
    OverlayLayerKind kind = OverlayLayerKind::Text;

    // -- Texto --
    std::string text     = "Texto";
    std::string fontName = "Predeterminada";
    float       fontSize = 72.0f;          // en px, referido a un canvas 1920x1080
    float       color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    bool  shadowEnabled  = true;           // sombra suave (look por defecto, ya existia)
    float shadowColor[4] = { 0.0f, 0.0f, 0.0f, 0.63f };
    float shadowOffsetX  = 1.5f;           // px, referido a un canvas 1920x1080
    float shadowOffsetY  = 1.5f;

    bool  outlineEnabled  = false;         // contorno (stroke)
    float outlineColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    float outlineWidth    = 2.0f;          // px, referido a un canvas 1920x1080

    bool  bgEnabled   = false;             // fondo de color detras del texto
    float bgColor[4]  = { 0.0f, 0.0f, 0.0f, 0.5f };
    float bgPaddingX  = 10.0f;             // px, referido a un canvas 1920x1080
    float bgPaddingY  = 6.0f;
    float bgRounding  = 6.0f;

    // -- Imagen --
    std::string imagePath;                 // ruta absoluta al archivo de imagen
    float       sizeW = 0.3f;              // 0..1, ancho respecto al canvas
    float       sizeH = 0.3f;              // 0..1, alto respecto al canvas
    float       rotation = 0.0f;           // grados, sentido horario

    // -- Comun --
    float posX = 0.5f;                     // 0..1, centro de la capa
    float posY = 0.5f;                     // 0..1, centro de la capa
};

// ─────────────────────────────────────────────────────────────────────────────
//  OverlayDoc — la "receta" editable de un Overlay guardado (junto al .png
//  ya rasterizado que se usa para proyectar).
// ─────────────────────────────────────────────────────────────────────────────
struct OverlayDoc {
    int   canvasW = 1920;
    int   canvasH = 1080;
    float bgColor[4] = { 0.06f, 0.07f, 0.10f, 1.0f };
    std::vector<OverlayLayer> layers;
};

} // namespace ProyecThor::UI
