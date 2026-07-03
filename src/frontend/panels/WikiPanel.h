#pragma once

namespace ProyecThor::UI {

    // Ventana "Wiki": informacion general de la app, atajos y ayuda rapida.
    class WikiPanel {
    public:
        WikiPanel() = default;
        ~WikiPanel() = default;

        void Open();

        // Debe llamarse cada frame; no dibuja nada si la ventana esta cerrada.
        void Render();

    private:
        bool m_Show = false;
    };

} // namespace ProyecThor::UI
