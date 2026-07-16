#include "CapturePanel.h"
#include "DesignSystem.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <string>
#include <cstring>
#include <cmath>
#include <iostream>
#include <chrono>

// ─────────────────────────────────────────────────────────────────────────────
//  Platform detection
// ─────────────────────────────────────────────────────────────────────────────
#if defined(_WIN32)
  #define PT_PLATFORM_WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #pragma comment(lib, "gdi32.lib")
  #ifdef PT_USE_OPENCV
    #include <opencv2/videoio.hpp>
    #include <opencv2/imgproc.hpp>
  #endif
#elif defined(__linux__)
  #define PT_PLATFORM_LINUX
  #include <X11/Xlib.h>
  #include <X11/Xutil.h>
  #include <X11/Xatom.h>
  #ifdef PT_USE_OPENCV
    #include <opencv2/videoio.hpp>
    #include <opencv2/imgproc.hpp>
  #endif
#elif defined(__APPLE__)
  #define PT_PLATFORM_MACOS
#endif

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers X11 (titulo UTF-8 + manejador de errores)
//  Antes vivian pegados por error dentro del bloque de deteccion de
//  plataforma (rompiendo el balance de #ifdef/#endif). Van aca, junto al
//  resto del codigo especifico de Linux que los usa.
// ─────────────────────────────────────────────────────────────────────────────
#if defined(PT_PLATFORM_LINUX)

// Instala un manejador de errores X11 que ignora errores asincronicos
// (p.ej. una ventana que se cierra justo entre el enumerado y la consulta
// de sus atributos). Sin esto, ese tipo de condicion de carrera benigna
// puede tumbar la aplicacion entera via el manejador default de Xlib.
static void EnsureXErrorHandlerInstalled() {
    static bool installed = false;
    if (installed) return;
    installed = true;
    XSetErrorHandler([](Display*, XErrorEvent*) -> int { return 0; });
}

// Devuelve el titulo de una ventana X11. Preferimos _NET_WM_NAME (UTF-8,
// usado por gestores de ventanas modernos) y caemos a WM_NAME/XFetchName
// (legado, ICCCM) si no esta disponible.
static std::string GetWindowTitleX11(Display* dpy, Window w) {
    Atom netWmName = XInternAtom(dpy, "_NET_WM_NAME", False);
    Atom utf8Str   = XInternAtom(dpy, "UTF8_STRING",  False);

    if (netWmName != None && utf8Str != None) {
        Atom          actualType;
        int           actualFormat;
        unsigned long numItems  = 0;
        unsigned long bytesAfter = 0;
        unsigned char* data = nullptr;

        if (XGetWindowProperty(dpy, w, netWmName, 0, ~0L, False, utf8Str,
                                &actualType, &actualFormat, &numItems, &bytesAfter, &data)
                == Success && data)
        {
            std::string title(reinterpret_cast<char*>(data), numItems);
            XFree(data);
            if (!title.empty()) return title;
        }
    }

    char* name = nullptr;
    if (XFetchName(dpy, w, &name) && name) {
        std::string title(name);
        XFree(name);
        return title;
    }
    return "";
}

#endif // PT_PLATFORM_LINUX

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

    // Stub: buffer de píxeles RGBA para el patrón de prueba (camara sin OpenCV)
    std::vector<uint8_t> stubPixels;
    int                  stubW = 1280;
    int                  stubH =  720;
    float                stubTime = 0.0f;

    bool isOpen = false;

    // ── Captura de ventana / monitor ─────────────────────────────────────
    std::vector<uint8_t> screenPixels; // buffer RGBA reutilizado entre frames
    int                  screenW = 0;
    int                  screenH = 0;

    CaptureSourceType    activeScreenType    = CaptureSourceType::Unknown;
    int                  activeMonitorIndex  = -1;

    std::chrono::steady_clock::time_point lastScreenGrab{};
    // Limite de tasa de captura: no tiene sentido volver a leer la pantalla
    // completa cada frame de ImGui (podria ser 120+ fps); con 30 fps sobra
    // de sobra para proyeccion en vivo y evita cargar la CPU/X11 de mas.
    static constexpr double kMinScreenGrabInterval = 1.0 / 30.0;

#ifdef PT_PLATFORM_WIN32
    HWND activeHwnd = nullptr;
#elif defined(PT_PLATFORM_LINUX)
    Display*   xDisplay      = nullptr;
    ::Window   activeXWindow = 0;
#endif

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

    /// Devuelve puntero a píxeles RGBA del último frame de camara (o nullptr).
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

    // ── Apertura / cierre de captura de ventana o monitor ────────────────
    bool OpenScreen(CaptureSourceType type, int monitorIndex, const std::string& windowHandle) {
        activeScreenType   = type;
        activeMonitorIndex = -1;

#ifdef PT_PLATFORM_WIN32
        activeHwnd = nullptr;
        if (type == CaptureSourceType::Window) {
            if (windowHandle.empty()) return false;
            activeHwnd = reinterpret_cast<HWND>(
                static_cast<uintptr_t>(std::stoull(windowHandle)));
            if (!IsWindow(activeHwnd)) {
                activeHwnd = nullptr;
                return false;
            }
        } else if (type == CaptureSourceType::Monitor) {
            activeMonitorIndex = monitorIndex;
        } else {
            return false;
        }
#elif defined(PT_PLATFORM_LINUX)
        if (!xDisplay) {
            xDisplay = XOpenDisplay(nullptr);
            if (!xDisplay) {
                std::cerr << "[CapturePanel] No se pudo abrir el display X11 para captura.\n";
                return false;
            }
        }

        activeXWindow = 0;
        if (type == CaptureSourceType::Window) {
            if (windowHandle.empty()) return false;
            activeXWindow = static_cast<::Window>(std::stoul(windowHandle));
            XWindowAttributes attrs;
            if (!XGetWindowAttributes(xDisplay, activeXWindow, &attrs)) {
                activeXWindow = 0;
                return false;
            }
        } else if (type == CaptureSourceType::Monitor) {
            activeMonitorIndex = monitorIndex;
        } else {
            return false;
        }
#else
        // macOS: sin implementacion todavia.
        (void)type; (void)monitorIndex; (void)windowHandle;
        return false;
#endif

        isOpen = true;
        return true;
    }

    void CloseScreen() {
#ifdef PT_PLATFORM_WIN32
        activeHwnd = nullptr;
#elif defined(PT_PLATFORM_LINUX)
        activeXWindow = 0;
#endif
        activeScreenType   = CaptureSourceType::Unknown;
        activeMonitorIndex = -1;
        screenPixels.clear();
        isOpen = false;
    }

    /// Devuelve puntero a píxeles RGBA de la ventana/monitor capturado
    /// (o nullptr). w/h se actualizan con las dimensiones reales.
    /// Limita internamente la tasa de recaptura a kMinScreenGrabInterval.
    const uint8_t* GrabScreenFrame(int& w, int& h) {
        using namespace std::chrono;

        auto now = steady_clock::now();
        double elapsed = duration<double>(now - lastScreenGrab).count();
        if (elapsed < kMinScreenGrabInterval && !screenPixels.empty()) {
            w = screenW; h = screenH;
            return screenPixels.data();
        }
        lastScreenGrab = now;

#ifdef PT_PLATFORM_WIN32
        RECT rect{};
        HWND srcWnd  = nullptr;
        int  srcX    = 0;
        int  srcY    = 0;

        if (activeScreenType == CaptureSourceType::Window) {
            if (!activeHwnd || !IsWindow(activeHwnd)) return nullptr;
            GetClientRect(activeHwnd, &rect);
            srcWnd = activeHwnd;
        } else if (activeScreenType == CaptureSourceType::Monitor) {
            int mCount = 0;
            GLFWmonitor** monitors = glfwGetMonitors(&mCount);
            if (activeMonitorIndex < 0 || activeMonitorIndex >= mCount) return nullptr;
            const GLFWvidmode* vm = glfwGetVideoMode(monitors[activeMonitorIndex]);
            if (!vm) return nullptr;
            int mx, my;
            glfwGetMonitorPos(monitors[activeMonitorIndex], &mx, &my);
            rect.left = 0; rect.top = 0;
            rect.right = vm->width; rect.bottom = vm->height;
            srcWnd = nullptr; // desktop
            srcX = mx; srcY = my;
        } else {
            return nullptr;
        }

        int width  = rect.right  - rect.left;
        int height = rect.bottom - rect.top;
        if (width <= 0 || height <= 0) return nullptr;

        HDC srcDC = srcWnd ? GetDC(srcWnd) : GetDC(nullptr);
        if (!srcDC) return nullptr;

        HDC     memDC  = CreateCompatibleDC(srcDC);
        HBITMAP bitmap = CreateCompatibleBitmap(srcDC, width, height);
        HGDIOBJ oldObj = SelectObject(memDC, bitmap);

        BitBlt(memDC, 0, 0, width, height, srcDC, srcX, srcY, SRCCOPY | CAPTUREBLT);

        BITMAPINFOHEADER bi{};
        bi.biSize        = sizeof(BITMAPINFOHEADER);
        bi.biWidth       = width;
        bi.biHeight      = -height; // top-down: evita tener que voltear despues
        bi.biPlanes      = 1;
        bi.biBitCount    = 32;
        bi.biCompression = BI_RGB;

        screenPixels.assign(static_cast<size_t>(width) * height * 4, 0);
        GetDIBits(memDC, bitmap, 0, height, screenPixels.data(),
                  reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);

        // GDI entrega BGRA; OpenGL espera RGBA.
        for (size_t i = 0; i < screenPixels.size(); i += 4) {
            std::swap(screenPixels[i], screenPixels[i + 2]);
            screenPixels[i + 3] = 255;
        }

        SelectObject(memDC, oldObj);
        DeleteObject(bitmap);
        DeleteDC(memDC);
        ReleaseDC(srcWnd, srcDC);

        w = width; h = height;
        screenW = width; screenH = height;
        return screenPixels.data();

#elif defined(PT_PLATFORM_LINUX)
        if (!xDisplay) return nullptr;

        ::Window srcDrawable = 0;
        int x = 0, y = 0, width = 0, height = 0;

        if (activeScreenType == CaptureSourceType::Window) {
            if (!activeXWindow) return nullptr;
            XWindowAttributes attrs;
            if (!XGetWindowAttributes(xDisplay, activeXWindow, &attrs))
                return nullptr;

            // Si la ventana esta minimizada u oculta, XGetImage dispararia
            // un error X11 (BadMatch). En vez de arriesgarnos, devolvemos
            // nullptr: la previsualizacion mostrara "Sin señal activa"
            // hasta que la ventana vuelva a estar visible.
            if (attrs.map_state != IsViewable)
                return nullptr;

            srcDrawable = activeXWindow;
            width  = attrs.width;
            height = attrs.height;
        } else if (activeScreenType == CaptureSourceType::Monitor) {
            int mCount = 0;
            GLFWmonitor** monitors = glfwGetMonitors(&mCount);
            if (activeMonitorIndex < 0 || activeMonitorIndex >= mCount) return nullptr;
            const GLFWvidmode* vm = glfwGetVideoMode(monitors[activeMonitorIndex]);
            if (!vm) return nullptr;
            int mx, my;
            glfwGetMonitorPos(monitors[activeMonitorIndex], &mx, &my);
            srcDrawable = RootWindow(xDisplay, DefaultScreen(xDisplay));
            x = mx; y = my;
            width = vm->width; height = vm->height;
        } else {
            return nullptr;
        }

        if (width <= 0 || height <= 0) return nullptr;

        XImage* img = XGetImage(xDisplay, srcDrawable, x, y, width, height,
                                AllPlanes, ZPixmap);
        if (!img) return nullptr;

        screenPixels.assign(static_cast<size_t>(width) * height * 4, 0);

        // XGetPixel es lento pixel a pixel, pero es la forma portable de
        // interpretar cualquier mascara de color/bit-order que devuelva el
        // servidor X, sin asumir un formato de memoria especifico.
        for (int py = 0; py < height; ++py) {
            for (int px = 0; px < width; ++px) {
                unsigned long pixel = XGetPixel(img, px, py);
                uint8_t r = static_cast<uint8_t>((pixel & img->red_mask)   >> 16);
                uint8_t g = static_cast<uint8_t>((pixel & img->green_mask) >> 8);
                uint8_t b = static_cast<uint8_t>( pixel & img->blue_mask);
                size_t idx = (static_cast<size_t>(py) * width + px) * 4;
                screenPixels[idx + 0] = r;
                screenPixels[idx + 1] = g;
                screenPixels[idx + 2] = b;
                screenPixels[idx + 3] = 255;
            }
        }

        XDestroyImage(img);

        w = width; h = height;
        screenW = width; screenH = height;
        return screenPixels.data();
#else
        (void)w; (void)h;
        return nullptr;
#endif
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers de dibujo locales (mismo patron visual que el resto de paneles)
// ─────────────────────────────────────────────────────────────────────────────
namespace {
    ImU32 ColU32(float r, float g, float b, float a = 1.0f) {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
    }
    ImU32 ColA(ImU32 col, int a) {
        return (col & 0x00FFFFFFu) | (static_cast<ImU32>(std::clamp(a, 0, 255)) << 24);
    }
    ImVec4 ToVec4(ImU32 col) {
        return ImGui::ColorConvertU32ToFloat4(col);
    }

    // Sombra suave reutilizando el mismo patrón visual que el resto de paneles.
    void DrawSoftShadow(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float rounding) {
        for (float i = 1.0f; i <= 5.0f; i += 1.0f) {
            int alpha = static_cast<int>(34.0f - (i * 5.0f));
            dl->AddRectFilled(
                ImVec2(p0.x - i, p0.y - i + 3.0f),
                ImVec2(p1.x + i, p1.y + i + 3.0f),
                IM_COL32(0, 0, 0, std::max(0, alpha)), rounding + i);
        }
    }
} // namespace

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
#elif defined(PT_PLATFORM_LINUX)
    EnsureXErrorHandlerInstalled();

    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        CaptureSource stub;
        stub.type = CaptureSourceType::Window;
        stub.name = "No se pudo conectar a X11";
        m_Sources.push_back(stub);
        return;
    }

    Window root = DefaultRootWindow(dpy);
    // False (en vez de True): siempre resuelve el atomo, incluso si nadie
    // lo habia interneado todavia en esta conexion. Con True, en casos
    // raros podia devolver None y la lista quedaba vacia sin motivo real.
    Atom netClientList = XInternAtom(dpy, "_NET_CLIENT_LIST", False);

    Atom          actualType;
    int           actualFormat;
    unsigned long numItems  = 0;
    unsigned long bytesAfter = 0;
    unsigned char* data = nullptr;

    bool listOk = (netClientList != None) &&
        (XGetWindowProperty(dpy, root, netClientList, 0, ~0L, False, AnyPropertyType,
                            &actualType, &actualFormat, &numItems, &bytesAfter, &data)
         == Success) && data;

    if (listOk) {
        Window* windows = reinterpret_cast<Window*>(data);
        for (unsigned long i = 0; i < numItems; ++i) {
            Window w = windows[i];

            std::string title = GetWindowTitleX11(dpy, w);
            if (title.size() < 2) continue;

            CaptureSource src;
            src.type   = CaptureSourceType::Window;
            src.name   = title;
            src.handle = std::to_string(static_cast<unsigned long>(w));
            m_Sources.push_back(src);
        }
        XFree(data);
    } else {
        CaptureSource stub;
        stub.type = CaptureSourceType::Window;
        stub.name = "El gestor de ventanas no soporta _NET_CLIENT_LIST";
        m_Sources.push_back(stub);
    }

    XCloseDisplay(dpy);
#else
    // En macOS mostramos stub por ahora
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

    bool opened = false;
    if (src.type == CaptureSourceType::Camera) {
        opened = m_Backend->OpenCamera(src.index);
    } else if (src.type == CaptureSourceType::Window ||
               src.type == CaptureSourceType::Monitor) {
        opened = m_Backend->OpenScreen(src.type, src.index, src.handle);
    }
    if (!opened) return false;

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
    m_Backend->CloseScreen();
    m_IsCapturing = false;
    m_ProjectOnScreen = false;
}

void* CapturePanel::GetCurrentTexture() {
    if (!m_IsCapturing) return nullptr;

    int w = 0, h = 0;
    const uint8_t* pixels = nullptr;

    if (m_ActiveSource.type == CaptureSourceType::Camera)
        pixels = m_Backend->GrabFrame(w, h);
    else
        pixels = m_Backend->GrabScreenFrame(w, h);

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
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.05f, 0.09f, 0.13f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.07f, 0.12f, 0.17f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##capSearch", "Buscar fuente…", m_SearchBuf, sizeof(m_SearchBuf));
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.06f, 0.09f, 0.55f));
    ImGui::PushStyleColor(ImGuiCol_Header,        ColA(DS::AccentColor, 55));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,  ColA(DS::AccentColor, 80));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,   ColA(DS::AccentColor, 110));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, DS::RadiusMedium);
    ImGui::BeginChild("##capSrcList", ImVec2(0, 180), true);

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
            ImGui::TextColored(ToVec4(ColA(DS::AccentColor, 160)), "%s", typeLabel);
            typeLabelPrev = typeLabel;
        }

        std::string label = src.name + "##cap" + std::to_string(i);

        bool selected = (m_SelectedIdx == i);
        if (selected)
            ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::AccentLight));
        if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_None, ImVec2(0, 22))) {
            m_SelectedIdx = i;
        }
        if (selected)
            ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
}

void CapturePanel::RenderPreview() {
    void* tex = GetCurrentTexture();

    float avail = ImGui::GetContentRegionAvail().x;
    float previewH = avail * (9.0f / 16.0f);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 end = ImVec2(pos.x + avail, pos.y + previewH);
    ImDrawList* dlOuter = ImGui::GetWindowDrawList();
    DrawSoftShadow(dlOuter, pos, end, DS::RadiusLarge);

    ImU32 borderCol = m_IsCapturing ? ColA(DS::AccentColor, 160) : ColA(DS::TextHint, 120);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.03f, 0.035f, 0.05f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, DS::RadiusLarge);
    ImGui::BeginChild("##capPreview", ImVec2(avail, previewH), false, ImGuiWindowFlags_NoScrollbar);

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
        ImGui::SetCursorPos(ImVec2(avail * 0.5f - 70.0f, previewH * 0.5f - 10.0f));
        ImGui::TextColored(ToVec4(DS::TextHint), "Sin señal activa");
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRect(pos, end, borderCol, DS::RadiusLarge, 0, 1.5f);

    if (m_IsCapturing) {
        // Indicador "EN VIVO" en la esquina, mismo lenguaje visual que
        // el indicador de transmision de OClock.
        float t = static_cast<float>(ImGui::GetTime());
        float pulse = 0.55f + 0.35f * std::sin(t * 3.0f);
        ImU32 dotCol = ColA(DS::DangerColor, static_cast<int>(160 + 90 * pulse));
        ImVec2 dotPos(pos.x + 14.0f, pos.y + 14.0f);
        dl->AddCircleFilled(dotPos, 4.0f, dotCol);
        dl->AddText(ImVec2(dotPos.x + 10.0f, dotPos.y - 7.0f),
                    ColA(DS::TextSecondary, 230), "EN VIVO");
    }
}

void CapturePanel::RenderControls() {
    ImGui::Spacing();

    ImGui::TextColored(ToVec4(DS::TextHint), "Opacidad");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.05f, 0.09f, 0.13f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.07f, 0.12f, 0.17f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,      ToVec4(DS::AccentColor));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,ToVec4(DS::AccentLight));
    ImGui::SliderFloat("##capOp", &m_Opacity, 0.0f, 1.0f, "%.2f");
    ImGui::PopStyleColor(4);

    ImGui::SameLine(0, 20);
    ImGui::Checkbox("Ajustar al proyector", &m_StretchToFill);
}

void CapturePanel::RenderProjectButton() {
    ImGui::Spacing();
    DS::GlassSeparator();
    ImGui::Spacing();

    float w = ImGui::GetContentRegionAvail().x;
    bool canStart = (m_SelectedIdx >= 0 && m_SelectedIdx < (int)m_Sources.size());

    if (!m_IsCapturing) {
        ImGui::BeginDisabled(!canStart);
        DS::GlassButton("Iniciar captura", ImVec2(w, 36.0f), DS::SuccessColor);
        if (ImGui::IsItemClicked() && canStart)
            StartCapture(m_Sources[m_SelectedIdx]);
        ImGui::EndDisabled();
    } else {
        if (m_ProjectOnScreen) {
            if (DS::GlassButton("Quitar del proyector", ImVec2(w, 36.0f), DS::DangerColor))
                m_ProjectOnScreen = false;
        } else {
            if (DS::GlassButton("Enviar al proyector", ImVec2(w, 36.0f), DS::AccentColor))
                m_ProjectOnScreen = true;
        }

        ImGui::Spacing();
        if (DS::GlassButton("Detener captura", ImVec2(w, 28.0f), DS::AccentColorDim))
            StopCapture();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Render principal
// ─────────────────────────────────────────────────────────────────────────────
void CapturePanel::RenderContent() {
    // ── Cabecera con botón de actualizar ─────────────────────────────────
    DS::GlassSectionHeader("FUENTE DE CAPTURA");
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 24);
    if (ImGui::SmallButton("##capRefresh")) RefreshSources();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Actualizar fuentes");

    ImGui::Spacing();
    RenderSourceSelector();

    ImGui::Spacing();
    DS::GlassSectionHeader("PREVISUALIZACIÓN");
    ImGui::Spacing();
    RenderPreview();

    RenderControls();
    RenderProjectButton();
}
} // namespace ProyecThor::UI