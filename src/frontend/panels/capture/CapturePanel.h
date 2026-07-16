#pragma once
#include <imgui.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>

// ─────────────────────────────────────────────────────────────────────────────
//  CaptureSource — abstracción de cualquier fuente de captura
// ─────────────────────────────────────────────────────────────────────────────
namespace ProyecThor::UI {

enum class CaptureSourceType {
    Camera,         // Cámara física (webcam, capturadora HDMI, etc.)
    Window,         // Ventana específica del sistema operativo
    Monitor,        // Monitor / pantalla completa
    Unknown         // Antes "None" — renombrado para no chocar con la
                     // macro None de X11 (Xlib.h la define como 0L)
};

struct CaptureSource {
    CaptureSourceType type   = CaptureSourceType::Unknown;
    std::string       name;
    int               index  = -1;
    std::string       handle;
};

// ─────────────────────────────────────────────────────────────────────────────
//  CapturePanel
// ─────────────────────────────────────────────────────────────────────────────
// Ya no es un IPanel independiente: vive como la sección "Captura" del
// sidebar de HomePanel. Ver HomePanel.cpp.
class CapturePanel {
public:
    CapturePanel();
    ~CapturePanel();

    // Dibuja los controles (solo cuando la seccion "Captura" esta activa).
    // Ya no abre su propia ventana — HomePanel es dueño de esa.
    void        RenderContent();
    std::string GetName()  const { return "Captura"; }

    // ── Control público ──────────────────────────────────────────────────────

    /// Envía el frame actual de la captura activa al proyector.
    /// Se llama desde UIManager cuando isProjecting == true.
    void        RenderOnProjector(ImDrawList* dl,
                                  float px, float py,
                                  float pw, float ph);

    bool        IsLive()   const { return m_IsCapturing; }

private:
    // ── Enumeración de fuentes ───────────────────────────────────────────────
    void EnumerateCameras();
    void EnumerateWindows();
    void EnumerateMonitors();
    void RefreshSources();

    // ── Captura ──────────────────────────────────────────────────────────────
    bool StartCapture(const CaptureSource& src);
    void StopCapture();

    /// Obtiene el último frame como textura OpenGL.
    /// Devuelve (void*)texture_id o nullptr si no hay frame.
    void* GetCurrentTexture();

    // ── UI helpers ───────────────────────────────────────────────────────────
    void RenderSourceSelector();
    void RenderPreview();
    void RenderControls();
    void RenderProjectButton();

    // ── Estado ───────────────────────────────────────────────────────────────
    std::vector<CaptureSource> m_Sources;
    int                        m_SelectedIdx   = -1;
    CaptureSource              m_ActiveSource;

    bool  m_IsCapturing     = false;
    bool  m_ProjectOnScreen = false;   // Si true, también envía al proyector
    float m_Opacity         = 1.0f;
    bool  m_StretchToFill   = true;

    // ── Textura de preview ───────────────────────────────────────────────────
    unsigned int m_PreviewTexID = 0;   // GLuint como uint para evitar include de GL aquí
    int          m_FrameW       = 0;
    int          m_FrameH       = 0;

    // ── Backend de captura (opaco — implementado en .cpp) ────────────────────
    struct CaptureBackend;
    std::unique_ptr<CaptureBackend> m_Backend;

    // ── Búsqueda / filtro ────────────────────────────────────────────────────
    char m_SearchBuf[128] = {};
};

} // namespace ProyecThor::UI
