#pragma once
#include <GLFW/glfw3.h>
#include <functional>
#include <string>
#include <vector>

namespace ProyecThor::Core {

    // Ventana secundaria borderless en un monitor elegido, con contexto GL
    // compartido con la ventana principal. La usan Proyector y Stage (y
    // cualquier salida de multi-monitor futura) — la unica diferencia entre
    // ellas es QUE se dibuja adentro (el callback de Render), no COMO se
    // crea/destruye la ventana. Por eso vive como pieza generica reusable
    // en vez de duplicarse por cada tipo de salida.
    class SecondaryOutputWindow {
    public:
        using RenderFn = std::function<void(int width, int height)>;

        // Cualquier subsistema con un cache de recursos GL indexado por
        // contexto (ej. BackgroundLayer::s_ResourcesPerContext, ver su
        // .cpp) puede registrarse aca para enterarse cuando un contexto
        // secundario se destruye, y purgar su entrada. Sin esto, el cache
        // queda con punteros de contexto muertos que GLFW podria llegar a
        // reciclar para una ventana nueva.
        using ContextDestroyCallback = std::function<void(GLFWwindow*)>;
        static void RegisterContextDestroyCallback(ContextDestroyCallback cb);

        SecondaryOutputWindow() = default;
        ~SecondaryOutputWindow() { Destroy(); }

        SecondaryOutputWindow(const SecondaryOutputWindow&)            = delete;
        SecondaryOutputWindow& operator=(const SecondaryOutputWindow&) = delete;
        SecondaryOutputWindow(SecondaryOutputWindow&&)                 = default;
        SecondaryOutputWindow& operator=(SecondaryOutputWindow&&)      = default;

        // sharedContext: normalmente la ventana principal, para compartir
        // texturas/shaders/buffers (los VAO y FBO NO se comparten aunque
        // haya share — ver nota en BackgroundLayer.cpp sobre por que esto
        // importa).
        bool Create(GLFWwindow* sharedContext, int monitorIndex, const std::string& title);
        void Destroy();

        // Llamar UNA VEZ POR FRAME desde el loop principal, DESPUES de que
        // el core haya actualizado su estado (core.Update()), para no
        // quedar un frame atras. Hace MakeContextCurrent, invoca
        // renderFn(w,h), SwapBuffers, y restaura el contexto previo.
        void RenderFrame(const RenderFn& renderFn);

        bool        IsActive()        const { return m_Window != nullptr; }
        GLFWwindow* GetWindow()        const { return m_Window; }
        int         GetMonitorIndex()  const { return m_MonitorIndex; }

    private:
        static std::vector<ContextDestroyCallback>& DestroyCallbacks();

        GLFWwindow* m_Window       = nullptr;
        int         m_MonitorIndex = -1;
    };

} // namespace ProyecThor::Core