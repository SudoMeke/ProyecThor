#include "NativeVideoOutputWindow.h"

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#else
#define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <cstdint>
#include <iostream>

namespace ProyecThor::Core {

void* NativeVideoOutputWindow::Show(int monitorIndex)
{
    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    if (monitorIndex < 0 || monitorIndex >= monitorCount) {
        std::cerr << "[NativeVideoOutputWindow] Indice de monitor invalido: " << monitorIndex << "\n";
        return nullptr;
    }

    GLFWmonitor* target = monitors[monitorIndex];
    const GLFWvidmode* vm = glfwGetVideoMode(target);
    if (!vm) return nullptr;

    int monX = 0, monY = 0;
    glfwGetMonitorPos(target, &monX, &monY);

    if (!m_Window)
    {
        // GLFW_NO_API: sin contexto GL — VLC dibuja directo en la
        // superficie nativa via AttachNativeWindow().
        glfwWindowHint(GLFW_CLIENT_API,    GLFW_NO_API);
        glfwWindowHint(GLFW_DECORATED,     GLFW_FALSE);
        glfwWindowHint(GLFW_FLOATING,      GLFW_TRUE);
        glfwWindowHint(GLFW_RESIZABLE,     GLFW_FALSE);
        glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
        glfwWindowHint(GLFW_VISIBLE,       GLFW_FALSE);

        m_Window = glfwCreateWindow(vm->width, vm->height,
                                    "ProyecThor - Video (VLC)", nullptr, nullptr);
        glfwDefaultWindowHints();

        if (!m_Window) {
            const char* desc = nullptr;
            int code = glfwGetError(&desc);
            std::cerr << "[NativeVideoOutputWindow] glfwCreateWindow fallo. Codigo: "
                      << code << " Desc: " << (desc ? desc : "N/A") << "\n";
            return nullptr;
        }
    }

    glfwSetWindowPos(m_Window, monX, monY);
    glfwSetWindowSize(m_Window, vm->width, vm->height);
    glfwShowWindow(m_Window);
    m_Visible = true;

#ifdef _WIN32
    return static_cast<void*>(glfwGetWin32Window(m_Window));
#else
    return reinterpret_cast<void*>(static_cast<uintptr_t>(glfwGetX11Window(m_Window)));
#endif
}

void NativeVideoOutputWindow::Hide()
{
    if (m_Window) glfwHideWindow(m_Window);
    m_Visible = false;
}

void NativeVideoOutputWindow::Destroy()
{
    if (m_Window) {
        glfwDestroyWindow(m_Window);
        m_Window = nullptr;
    }
    m_Visible = false;
}

} // namespace ProyecThor::Core
