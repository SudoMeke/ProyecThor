#pragma once
#include <imgui.h>
#include <string>
#include <functional>
#include <vector>

struct ImGuiWindow; // ver imgui_internal.h

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  OverlayExportService — rasteriza el contenido ya dibujado por ImGui de una
//  child window (el canvas del editor de Overlays) a un PNG CON transparencia
//  (a diferencia de un fondo/tema, un Overlay se proyecta ENCIMA de lo que
//  ya este en pantalla, por eso el canal alpha se preserva tal cual en vez de
//  forzarse a opaco), reutilizando el ImDrawList que ImGui ya construyo este
//  mismo frame (no se dibuja nada a mano: se re-renderiza ese mismo draw
//  list contra un FBO propio, escalado a la resolucion de exportacion via
//  ImDrawData::FramebufferScale — el mismo mecanismo que usa ImGui para
//  pantallas HiDPI/Retina).
//
//  Uso: RequestCapture(...) se llama mientras se construye la UI (ej. al
//  apretar "Guardar"); ProcessPending() debe llamarse una vez por frame desde
//  main.cpp, justo despues de ImGui::Render() y antes de
//  ImGui_ImplOpenGL3_RenderDrawData(), para que el ImDrawList del frame
//  todavia sea valido.
// ─────────────────────────────────────────────────────────────────────────────
class OverlayExportService {
public:
    static OverlayExportService& Get();

    // canvasWindow: puntero devuelto por ImGui::GetCurrentWindow() justo
    // despues del BeginChild() del canvas, en ESTE mismo frame.
    // canvasScreenPos/Size: ImGui::GetWindowPos()/GetWindowSize() de esa
    // misma child, capturados en el mismo instante.
    void RequestCapture(ImGuiWindow* canvasWindow,
                        ImVec2 canvasScreenPos, ImVec2 canvasScreenSize,
                        const std::string& outPngPath,
                        int exportW, int exportH,
                        std::function<void(bool)> onDone);

    void ProcessPending();

private:
    OverlayExportService() = default;

    struct PendingCapture {
        ImGuiWindow*              canvasWindow;
        ImVec2                    screenPos;
        ImVec2                    screenSize;
        std::string               outPngPath;
        int                       exportW, exportH;
        std::function<void(bool)> onDone;
    };

    std::vector<PendingCapture> m_Pending;

    bool CaptureOne(const PendingCapture& req);
};

} // namespace ProyecThor::UI
