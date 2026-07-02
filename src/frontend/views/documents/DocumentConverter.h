#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <functional>

namespace ProyecThor::UI {

    enum class DocumentType {
        Unknown,
        PDF,
        PPTX
    };

    struct ConversionResult {
        bool                     success = false;
        std::string              errorMessage;
        std::vector<std::string> pagePaths;
    };

    // Callback para progreso: recibe (paginaActual, totalPaginas)
    using ProgressCallback = std::function<void(int, int)>;

    class DocumentConverter {
    public:
        DocumentConverter() = default;
        ~DocumentConverter() = default;

        // Convierte el archivo en paginas PNG dentro de cacheDir.
        // Si ya fue convertido antes (cache valida), devuelve las rutas directamente.
        ConversionResult Convert(
            const std::string&   filePath,
            const std::string&   cacheDir,
            int                  dpi            = 150,
            ProgressCallback     onProgress     = nullptr
        );

        // Limpia la cache de un documento especifico
        void ClearCache(const std::string& filePath, const std::string& cacheDir);

        static DocumentType DetectType(const std::string& filePath);

    private:
        ConversionResult ConvertPDF(
            const std::string& filePath,
            const std::string& outputDir,
            int                dpi,
            ProgressCallback   onProgress
        );

        ConversionResult ConvertPPTX(
            const std::string& filePath,
            const std::string& outputDir,
            int                dpi,
            ProgressCallback   onProgress
        );

        // Genera un nombre de directorio de cache unico basado en la ruta y fecha de modificacion
        std::string BuildCacheKey(const std::string& filePath);

        // Busca el ejecutable de LibreOffice en el sistema
        std::string FindLibreOfficeBinary();
    };

} // namespace ProyecThor::UI
