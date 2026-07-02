#pragma once

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include "ImageView.h"
#include "documents/DocumentConverter.h"

namespace ProyecThor::UI {

    enum class DocumentLoadState {
        Idle,
        Converting,
        Ready,
        Error
    };

    class DocumentView {
    public:
        DocumentView();
        ~DocumentView();

        // Carga un archivo .pdf o .pptx desde disco.
        // La conversion ocurre en un hilo separado para no bloquear el render.
        void LoadDocument(const std::string& filePath, const std::string& cacheDir);

        void Render(const std::string& docTitle, const std::vector<std::string>& pages);

        // Overload principal: usa el documento cargado internamente
        void Render();

        DocumentLoadState GetLoadState() const { return m_LoadState.load(); }
        const std::string& GetLastError() const { return m_LastError; }

    private:
        void GoToPage(int pageIndex);
        void StartConversion(const std::string& filePath, const std::string& cacheDir);

        // Estado de carga asincrona
        std::atomic<DocumentLoadState> m_LoadState{ DocumentLoadState::Idle };
        std::thread                    m_ConversionThread;
        std::mutex                     m_PagesMutex;

        // Progreso de conversion
        std::atomic<int>               m_ConversionProgress{ 0 };
        std::atomic<int>               m_ConversionTotal{ 0 };

        // Datos del documento
        std::string              m_FilePath;
        std::string              m_DocTitle;
        std::vector<std::string> m_Pages;         // rutas a los PNG ya convertidos
        std::string              m_LastError;

        // Estado de navegacion
        std::string m_LastDocument = "";
        int         m_CurrentPage  = 0;

        ImageView          m_PageViewer;
        DocumentConverter  m_Converter;
    };

} // namespace ProyecThor::UI