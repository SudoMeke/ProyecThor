#pragma once

struct GLFWwindow;

namespace ProyecThor::Core {

// Ventana nativa minima y borderless para el motor de renderizado "VLC
// (ventana nativa)" (ver BackgroundLayer::SetUseNativeEngine). A
// diferencia de SecondaryOutputWindow (pensada para render propio via un
// contexto GL compartido, ver su .cpp), esta NO crea contexto GL: VLC
// dibuja el video directo en la superficie nativa via
// VLCBasePlayer::AttachNativeWindow(), y un contexto GL de mas ahi solo
// podria interferir con el renderer acelerado de VLC sobre esa misma
// ventana.
class NativeVideoOutputWindow {
public:
    NativeVideoOutputWindow()  = default;
    ~NativeVideoOutputWindow() { Destroy(); }

    NativeVideoOutputWindow(const NativeVideoOutputWindow&)            = delete;
    NativeVideoOutputWindow& operator=(const NativeVideoOutputWindow&) = delete;

    // Crea (la primera vez) o reposiciona (si ya existe) la ventana sobre
    // el monitor indicado y la muestra. Devuelve el handle nativo (HWND en
    // Windows, X11 Window casteada a void* en Linux) listo para pasarle a
    // VLCBasePlayer::AttachNativeWindow(), o nullptr si fallo.
    void* Show(int monitorIndex);

    // Oculta la ventana sin destruirla — Show() la vuelve a mostrar sin
    // recrearla.
    void Hide();

    void Destroy();

    bool IsVisible() const { return m_Visible; }

private:
    GLFWwindow* m_Window  = nullptr;
    bool        m_Visible = false;
};

} // namespace ProyecThor::Core
