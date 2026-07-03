#pragma once
#include <string>
#include <vector>

namespace ProyecThor::UI {

    // Ventana "Base de datos": lista las canciones que vienen incluidas
    // con el programa (build/default/songs) y permite importarlas a la
    // biblioteca del usuario (appdata/ProyecThor/assets/songs).
    class DatabasePanel {
    public:
        DatabasePanel() = default;
        ~DatabasePanel() = default;

        // Abre la ventana y fuerza un re-escaneo de la carpeta de canciones
        // incluidas (por si el usuario ya importo algunas desde la ultima vez).
        void Open();

        // Debe llamarse cada frame; no dibuja nada si la ventana esta cerrada.
        void Render();

    private:
        struct SongEntry {
            std::string fileName;       // "amazing_grace.txt"
            std::string displayName;    // "amazing grace"
            bool        alreadyImported = false;
        };

        bool                    m_Show        = false;
        bool                    m_ScanPending = true;
        char                    m_SearchBuffer[128] = {};
        std::vector<SongEntry>  m_AvailableSongs;

        std::string m_LastActionMessage;
        bool        m_LastActionSuccess = false;

        void ScanBundledSongs();
        bool ImportSong(const SongEntry& entry);
    };

} // namespace ProyecThor::UI
