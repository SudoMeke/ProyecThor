#pragma once
#include "OverlayTypes.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

struct ImGuiWindow; // ver imgui_internal.h

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  OverlayCanvasEditor — editor tipo Canva: canvas central con capas de
//  texto, forma o imagen arrastrables (y redimensionables/rotables, en el
//  caso de imagenes/formas) + toolbar flotante arriba (Texto/Forma/Imagen/
//  Eliminar) + panel lateral de capas y propiedades. Se dibuja SIEMPRE a
//  pantalla completa (ver Render()) — el llamador es responsable de pedirle
//  a UIManager que oculte el resto de los paneles mientras este abierto
//  (UIManager::EnterFullscreenEditor/ExitFullscreenEditor).
//
//  Al guardar, rasteriza el canvas a PNG con transparencia (ver
//  OverlayExportService) y solo entonces invoca el callback de guardado con
//  el nombre + la receta editable (OverlayDoc), para que el llamador la
//  persista en disco.
// ─────────────────────────────────────────────────────────────────────────────
class OverlayCanvasEditor {
public:
    using OnSaveCallback    = std::function<void(const std::string& name, const OverlayDoc& doc)>;
    using OnCancelCallback  = std::function<void()>;
    using ResolvePngPathFn  = std::function<std::string(const std::string& name)>;
    using ListBgImagesFn    = std::function<std::vector<std::string>()>;
    using ImportImageFn     = std::function<std::string()>; // abre dialogo, copia el archivo, devuelve ruta (o "" si cancela)

    OverlayCanvasEditor(std::vector<std::string>* fontList, ResolvePngPathFn resolvePngPath,
                        ListBgImagesFn listBgImages, ImportImageFn importImage);
    ~OverlayCanvasEditor() = default;

    void OpenNew(const OverlayDoc& defaults = {});
    void OpenEdit(const std::string& existingName, const OverlayDoc& existingDoc);

    // onCancel se dispara ademas al Guardar (el editor se cierra en ambos
    // casos) -- el llamador lo usa para salir del modo pantalla completa
    // (ver UIManager::ExitFullscreenEditor) sin duplicar esa llamada en cada
    // callback de guardado.
    void Render(OnSaveCallback onSave, OnCancelCallback onClose);

    bool IsOpen() const { return m_IsOpen; }

private:
    void RenderHeader(ImDrawList* dl, ImVec2 winPos, ImVec2 winSize);
    void RenderFloatingToolbar(ImVec2 canvasPos, ImVec2 canvasSize);
    void RenderCanvas(float w, float h);
    void RenderSidebar(float w, float h);
    void RenderFooter(ImVec2 winPos, ImVec2 winSize, OnSaveCallback& onSave, OnCancelCallback& onClose);

    void RenderResizeHandle(int layerIdx, OverlayLayer& layer, int corner,
                            ImVec2 handlePos, ImDrawList* fgDl);
    void RenderRotateHandle(int layerIdx, OverlayLayer& layer, ImVec2 center, float halfH,
                            ImDrawList* fgDl);
    void AddImageLayerFromMenu();
    ImTextureID GetImageTexture(const std::string& path);

    // Dibuja SOLO el contenido real de las capas (fondo del doc + Text/
    // Image/Shape; Clock se saltea siempre, nunca se hornea) en un
    // ImDrawList aparte, propio para exportar -- ver RenderFooter. Nunca
    // incluye el cuadriculado "sin fondo" ni chrome de edicion (eso solo
    // existe en el canvas en vivo, ver RenderCanvas).
    void DrawLayersForExport(ImDrawList* dl, ImVec2 p0, ImVec2 canvasScreenSize);

    // Controles de font/tamano/color/sombra/contorno/fondo -- compartidos
    // entre capas Text y Clock (la capa Clock es un cuadro con el mismo
    // estilo de texto, solo que sin contenido editable, ver RenderSidebar).
    void RenderTextStyleProperties(OverlayLayer& layer, float w);

    std::vector<std::string>* m_FontList;
    ResolvePngPathFn           m_ResolvePngPath;
    ListBgImagesFn             m_ListBgImages;
    ImportImageFn              m_ImportImage;

    OverlayDoc m_Doc;
    char       m_Name[128] = {};
    bool       m_IsOpen            = false;
    bool       m_IsEditingExisting = false;
    int        m_SelectedLayer     = -1;
    bool       m_SaveFailed        = false;

    // Estado de arrastre de una capa (mover)
    int    m_DraggingLayer = -1;
    ImVec2 m_DragStartMouse{};
    float  m_DragStartPosX = 0.0f;
    float  m_DragStartPosY = 0.0f;

    // Estado de arrastre de una esquina (redimensionar capas de imagen/forma)
    int    m_ResizingLayer   = -1;
    int    m_ResizeCorner    = -1; // 0=TL, 1=TR, 2=BL, 3=BR
    ImVec2 m_ResizeStartMouse{};
    float  m_ResizeStartPosX  = 0.0f;
    float  m_ResizeStartPosY  = 0.0f;
    float  m_ResizeStartSizeW = 0.0f;
    float  m_ResizeStartSizeH = 0.0f;

    // Estado de arrastre del handle de rotacion (capas de imagen/forma)
    int   m_RotatingLayer      = -1;
    float m_RotateStartAngle   = 0.0f; // radianes, angulo mouse-centro al empezar
    float m_RotateStartRotation = 0.0f; // layer.rotation al empezar (grados)

    // Cache de texturas GL de imagenes usadas como capa (clave = ruta)
    std::unordered_map<std::string, ImTextureID> m_ImageTexCache;

    // Capturado cada frame al dibujar el canvas — usado al presionar Guardar
    // para pedirle a OverlayExportService que rasterice exactamente ese
    // contenido (ver RenderCanvas / RenderFooter).
    ImGuiWindow* m_CanvasWindowThisFrame = nullptr;
    ImVec2       m_CanvasScreenPos{};
    ImVec2       m_CanvasScreenSize{};
};

} // namespace ProyecThor::UI
