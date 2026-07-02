#include "CapturePanel.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <string>
#include <cstring>

// ─────────────────────────────────────────────────────────────────────────────
//  Platform detection
// ─────────────────────────────────────────────────────────────────────────────
#if defined(_WIN32)
  #define PT_PLATFORM_WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  // OpenCV es opcional; si no está disponible la captura de cámara usa un stub.
  // Para habilitarla: #define PT_USE_OPENCV y linkear opencv_world.
  #ifdef PT_USE_OPENCV
    #include <opencv2/videoio.hpp>
    #include <opencv2/imgproc.hpp>
  #endif
#elif defined(__linux__)
  #define PT_PLATFORM_LINUX
  #ifdef PT_USE_OPENCV
    #include <opencv2/videoio.hpp>
    #include <opencv2/imgproc.hpp>
  #endif
#elif defined(__APPLE__)
  #define PT_PLATFORM_MACOS
#endif

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  CaptureBackend — implementación opaca
//  Cuando PT_USE_OPENCV está definido usa cv::VideoCapture.
//  En caso contrario, genera un patrón de prueba animado (stub).
// ─────────────────────────────────────────────────────────────────────────────
struct CapturePanel::CaptureBackend {
#ifdef PT_USE_OPENCV
    cv::VideoCapture cap;
    cv::Mat          frameBGR;
    cv::Mat          frameRGBA;
#endif

    // Stub: buffer de píxeles RGBA para el patrón de prueba
    std::vector<uint8_t> stubPixels;
    int                  stubW = 1280;
    int                  stubH =  720;
    float                stubTime = 0.0f;

    bool isOpen = false;

    bool OpenCamera(int index) {
#ifdef PT_USE_OPENCV
        cap.open(index, cv::CAP_ANY);
        isOpen = cap.isOpened();
        return isOpen;
#else
        // Stub: siempre "abre" correctamente
        stubPixels.assign(stubW * stubH * 4, 0);
        isOpen = true;
        return true;
#endif
    }

    void Close() {
#ifdef PT_USE_OPENCV
        if (cap.isOpened()) cap.release();
#endif
        isOpen = false;
        stubPixels.clear();
    }

    /// Devuelve puntero a píxeles RGBA del último frame (o nullptr).
    /// w/h se actualizan con las dimensiones reales.
    const uint8_t* GrabFrame(int& w, int& h) {
#ifdef PT_USE_OPENCV
        if (!cap.isOpened()) return nullptr;
        if (!cap.read(frameBGR) || frameBGR.empty()) return nullptr;
        cv::cvtColor(frameBGR, frameRGBA, cv::COLOR_BGR2RGBA);
        w = frameRGBA.cols;
        h = frameRGBA.rows;
        return frameRGBA.data;
#else
        // Genera un patrón animado SMPTE-like para debug
        stubTime += 0.016f;
        w = stubW; h = stubH;
        uint8_t* px = stubPixels.data();
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float u = (float)x / w;
                float v = (float)y / h;
                uint8_t r = (uint8_t)((std::sin(u * 6.28f + stubTime)          * 0.5f + 0.5f) * 220 + 30);
                uint8_t g = (uint8_t)((std::sin(v * 6.28f + stubTime * 0.7f)   * 0.5f + 0.5f) * 220 + 30);
                uint8_t b = (uint8_t)((std::sin((u+v) * 4.0f + stubTime * 1.3f)* 0.5f + 0.5f) * 220 + 30);
                int idx = (y * w + x) * 4;
                px[idx+0] = r; px[idx+1] = g; px[idx+2] = b; px[idx+3] = 255;
            }
        }
        return px;
#endif
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Ctor / Dtor
// ─────────────────────────────────────────────────────────────────────────────
CapturePanel::CapturePanel()
    : m_Backend(std::make_unique<CaptureBackend>())
{
    RefreshSources();
}

CapturePanel::~CapturePanel() {
    StopCapture();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Enumeración de fuentes
// ─────────────────────────────────────────────────────────────────────────────
void CapturePanel::EnumerateCameras() {
#ifdef PT_USE_OPENCV
    for (int i = 0; i < 8; ++i) {
        cv::VideoCapture probe(i, cv::CAP_ANY);
        if (probe.isOpened()) {
            CaptureSource src;
            src.type  = CaptureSourceType::Camera;
            src.name  = "Cámara " + std::to_string(i);
            src.index = i;
            m_Sources.push_back(src);
            probe.release();
        }
    }
#else
    // Stub: añade una cámara ficticia para mostrar la UI
    CaptureSource stub;
    stub.type  = CaptureSourceType::Camera;
    stub.name  = "Cámara 0 (simulada)";
    stub.index = 0;
    m_Sources.push_back(stub);
#endif
}

void CapturePanel::EnumerateWindows() {
#ifdef PT_PLATFORM_WIN32
    // Enumera ventanas visibles con título
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* sources = reinterpret_cast<std::vector<CaptureSource>*>(lParam);
        if (!IsWindowVisible(hwnd)) return TRUE;
        char title[256] = {};
        GetWindowTextA(hwnd, title, sizeof(title));
        if (strlen(title) < 3) return TRUE;

        // Filtra ventanas de sistema
        char cls[128] = {};
        GetClassNameA(hwnd, cls, sizeof(cls));
        if (strcmp(cls, "Progman") == 0 || strcmp(cls, "Shell_TrayWnd") == 0) return TRUE;

        CaptureSource src;
        src.type   = CaptureSourceType::Window;
        src.name   = std::string(title);
        src.handle = std::to_string(reinterpret_cast<uintptr_t>(hwnd));
        sources->push_back(src);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&m_Sources));
#else
    // En Linux/macOS mostramos stub
    CaptureSource stub;
    stub.type = CaptureSourceType::Window;
    stub.name = "Ventana activa (simulada)";
    m_Sources.push_back(stub);
#endif
}

void CapturePanel::EnumerateMonitors() {
    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    for (int i = 0; i < count; ++i) {
        CaptureSource src;
        src.type  = CaptureSourceType::Monitor;
        src.name  = std::string("Monitor ") + std::to_string(i + 1)
                    + " — " + glfwGetMonitorName(monitors[i]);
        src.index = i;
        m_Sources.push_back(src);
    }
}

void CapturePanel::RefreshSources() {
    m_Sources.clear();
    EnumerateCameras();
    EnumerateWindows();
    EnumerateMonitors();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Captura
// ─────────────────────────────────────────────────────────────────────────────
bool CapturePanel::StartCapture(const CaptureSource& src) {
    StopCapture();
    m_ActiveSource = src;

    if (src.type == CaptureSourceType::Camera) {
        if (!m_Backend->OpenCamera(src.index)) return false;
    } else {
        // Window / Monitor: stub por ahora
        m_Backend->OpenCamera(0); // abre stub
    }

    // Crea la textura OpenGL si no existe
    if (m_PreviewTexID == 0) {
        glGenTextures(1, &m_PreviewTexID);
        glBindTexture(GL_TEXTURE_2D, m_PreviewTexID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    m_IsCapturing = true;
    return true;
}

void CapturePanel::StopCapture() {
    m_Backend->Close();
    m_IsCapturing = false;
    m_ProjectOnScreen = false;
}

void* CapturePanel::GetCurrentTexture() {
    if (!m_IsCapturing) return nullptr;

    int w = 0, h = 0;
    const uint8_t* pixels = m_Backend->GrabFrame(w, h);
    if (!pixels || w == 0 || h == 0) return nullptr;

    glBindTexture(GL_TEXTURE_2D, m_PreviewTexID);

    if (w != m_FrameW || h != m_FrameH) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        m_FrameW = w; m_FrameH = h;
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    return reinterpret_cast<void*>(static_cast<uintptr_t>(m_PreviewTexID));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Render al proyector
// ─────────────────────────────────────────────────────────────────────────────
void CapturePanel::RenderOnProjector(ImDrawList* dl,
                                     float px, float py,
                                     float pw, float ph)
{
    if (!m_IsCapturing || !m_ProjectOnScreen) return;

    void* tex = GetCurrentTexture();
    if (!tex) return;

    float destX = px, destY = py, destW = pw, destH = ph;

    if (!m_StretchToFill && m_FrameW > 0 && m_FrameH > 0) {
        float vidR    = (float)m_FrameW / (float)m_FrameH;
        float scnR    = pw / ph;
        if (vidR > scnR + 0.001f) {
            destH = destW / vidR;
            destY = py + (ph - destH) * 0.5f;
        } else if (vidR < scnR - 0.001f) {
            destW = destH * vidR;
            destX = px + (pw - destW) * 0.5f;
        }
    }

    ImU32 col = IM_COL32(255, 255, 255, (int)(m_Opacity * 255));
    dl->AddImage(tex,
        ImVec2(destX, destY),
        ImVec2(destX + destW, destY + destH),
        ImVec2(0,0), ImVec2(1,1), col);
}

// ─────────────────────────────────────────────────────────────────────────────
//  UI helpers
// ─────────────────────────────────────────────────────────────────────────────
void CapturePanel::RenderSourceSelector() {
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##capSearch", "Buscar fuente…", m_SearchBuf, sizeof(m_SearchBuf));
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.02f, 0.02f, 0.04f, 0.70f));
    ImGui::BeginChild("##capSrcList", ImVec2(0, 180), true);

    const char* typeIcon[] = { "  ", "  ", "  ", "" };
    const char* typeLabelPrev = nullptr;

    for (int i = 0; i < (int)m_Sources.size(); ++i) {
        const auto& src = m_Sources[i];

        // Filtro de búsqueda
        if (m_SearchBuf[0] != '\0') {
            std::string lower = src.name;
            std::string filt  = m_SearchBuf;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            std::transform(filt.begin(),  filt.end(),  filt.begin(),  ::tolower);
            if (lower.find(filt) == std::string::npos) continue;
        }

        // Separador de categoría
        const char* typeLabel = nullptr;
        switch (src.type) {
            case CaptureSourceType::Camera:  typeLabel = "CÁMARAS";   break;
            case CaptureSourceType::Window:  typeLabel = "VENTANAS";  break;
            case CaptureSourceType::Monitor: typeLabel = "MONITORES"; break;
            default: break;
        }
        if (typeLabel && typeLabel != typeLabelPrev) {
            if (typeLabelPrev) ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.369f, 0.420f, 1.0f, 0.6f));
            ImGui::TextUnformatted(typeLabel);
            ImGui::PopStyleColor();
            typeLabelPrev = typeLabel;
        }

        const int type_i = (int)src.type;
        const char* icon = (type_i >= 0 && type_i <= 2) ? typeIcon[type_i] : "";
        std::string label = std::string(icon) + " " + src.name + "##cap" + std::to_string(i);

        bool selected = (m_SelectedIdx == i);
        if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_None, ImVec2(0, 22))) {
            m_SelectedIdx = i;
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void CapturePanel::RenderPreview() {
    void* tex = GetCurrentTexture();

    float avail = ImGui::GetContentRegionAvail().x;
    float previewH = avail * (9.0f / 16.0f);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::BeginChild("##capPreview", ImVec2(avail, previewH), false);
    ImVec2 pos = ImGui::GetWindowPos();

    if (tex) {
        float imgW = avail, imgH = previewH;
        if (m_FrameW > 0 && m_FrameH > 0) {
            float vidR = (float)m_FrameW / (float)m_FrameH;
            float boxR = avail / previewH;
            if (vidR > boxR) { imgH = imgW / vidR; }
            else             { imgW = imgH * vidR; }
        }
        float offX = (avail   - imgW) * 0.5f;
        float offY = (previewH - imgH) * 0.5f;

        ImGui::SetCursorPos(ImVec2(offX, offY));
        ImGui::Image(tex, ImVec2(imgW, imgH));
    } else {
        // Placeholder
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(pos, ImVec2(pos.x + avail, pos.y + previewH), IM_COL32(8,10,18,255));
        ImGui::SetCursorPos(ImVec2(avail * 0.5f - 60, previewH * 0.5f - 10));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.3f, 0.4f, 1.0f));
        ImGui::TextUnformatted("Sin señal activa");
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void CapturePanel::RenderControls() {
    ImGui::Spacing();

    // Opacidad
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.7f, 1.0f));
    ImGui::TextUnformatted("Opacidad");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160);
    ImGui::SliderFloat("##capOp", &m_Opacity, 0.0f, 1.0f, "%.2f");

    ImGui::SameLine(0, 20);
    ImGui::Checkbox("Ajustar al proyector", &m_StretchToFill);
}

void CapturePanel::RenderProjectButton() {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    bool canStart = (m_SelectedIdx >= 0 && m_SelectedIdx < (int)m_Sources.size());

    if (!m_IsCapturing) {
        // ── Iniciar captura ──────────────────────────────────────────────
        ImGui::BeginDisabled(!canStart);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.200f, 0.550f, 0.200f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.280f, 0.680f, 0.280f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.150f, 0.420f, 0.150f, 1.0f));
        if (ImGui::Button("  Iniciar captura", ImVec2(-1, 36)))
            StartCapture(m_Sources[m_SelectedIdx]);
        ImGui::PopStyleColor(3);
        ImGui::EndDisabled();
    } else {
        // ── Proyectar / detener ──────────────────────────────────────────
        if (m_ProjectOnScreen) {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.700f, 0.200f, 0.200f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.850f, 0.280f, 0.280f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.550f, 0.150f, 0.150f, 1.0f));
            if (ImGui::Button("  Quitar del proyector", ImVec2(-1, 36)))
                m_ProjectOnScreen = false;
            ImGui::PopStyleColor(3);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.369f, 0.420f, 1.000f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.500f, 0.550f, 1.000f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.280f, 0.330f, 0.800f, 1.0f));
            if (ImGui::Button("  Enviar al proyector", ImVec2(-1, 36)))
                m_ProjectOnScreen = true;
            ImGui::PopStyleColor(3);
        }

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f, 0.12f, 0.16f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.20f, 0.26f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.08f, 0.08f, 0.12f, 1.0f));
        if (ImGui::Button("Detener captura", ImVec2(-1, 28)))
            StopCapture();
        ImGui::PopStyleColor(3);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Render principal
// ─────────────────────────────────────────────────────────────────────────────
void CapturePanel::Render() {
    if (!m_ShowCapture) return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 14.0f));
    bool open = ImGui::Begin("Captura", &m_ShowCapture);
    ImGui::PopStyleVar();

    if (!open) {
        ImGui::End();
        return;
    }

    // ── Cabecera con botón de actualizar ─────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.75f, 1.0f, 1.0f));
    ImGui::TextUnformatted("FUENTE DE CAPTURA");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 24);
    if (ImGui::SmallButton("##capRefresh")) RefreshSources();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Actualizar fuentes");

    ImGui::Spacing();
    RenderSourceSelector();

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.75f, 1.0f, 1.0f));
    ImGui::TextUnformatted("PREVISUALIZACIÓN");
    ImGui::PopStyleColor();
    ImGui::Spacing();
    RenderPreview();

    RenderControls();
    RenderProjectButton();

    ImGui::End();
}
} // namespace ProyecThor::UI
