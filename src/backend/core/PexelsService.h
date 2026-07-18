#pragma once
#include <string>
#include <vector>
#include <unordered_map>

namespace ProyecThor::Core {

// ─────────────────────────────────────────────────────────────────────────────
//  PexelsService — busqueda de fotos libres de derechos via la API de Pexels
//  (https://www.pexels.com/api/), para usarlas como capas de imagen en el
//  editor de Overlays.
//
//  La key la pega cada usuario en Ajustes > Integraciones (ver
//  SettingsManager::IntegrationsSettings) y se guarda solo en su
//  settings.json local — el codigo (open source) nunca contiene una key
//  propia hardcodeada.
//
//  Todas las llamadas de red se hacen invocando "curl" como subproceso (igual
//  patron que los dialogos de archivo de LayersBgTab/TabTypography via
//  zenity/kdialog), asi no se agrega ninguna dependencia de build nueva. Solo
//  disponible en Linux/macOS por ahora: en Windows Search()/DownloadImage()
//  devuelven error sin intentar nada (ver .cpp).
//
//  Manejo de cuota (el plan gratuito de Pexels limita pedidos por hora/mes):
//   - Se cachea cada busqueda exitosa en memoria (por query+pagina): repetir
//     la misma busqueda en la sesion no vuelve a golpear la red.
//   - Se exige un intervalo minimo entre pedidos reales (ver kMinRequestGapSec).
//   - Se lee la cuota restante de los headers X-Ratelimit-* que Pexels manda
//     en cada respuesta, y se bloquean nuevas busquedas si se agoto hasta
//     que Pexels indique que se renueva (ver GetQuota()).
//  Todo esto es ademas de que la UI debe evitar disparar una busqueda por
//  cada tecla (buscar solo al apretar Enter/boton, ver OverlayCanvasEditor).
// ─────────────────────────────────────────────────────────────────────────────

struct PexelsPhoto {
    long long   id = 0;
    int         width  = 0;
    int         height = 0;
    std::string photographer;
    std::string photographerUrl;
    std::string thumbnailUrl; // "medium" — para el grid de resultados
    std::string downloadUrl;  // "large2x" — para usar como capa de imagen
};

struct PexelsSearchResult {
    bool                     ok = false;
    std::string              error;      // mensaje legible si ok == false
    std::vector<PexelsPhoto> photos;
    int                      totalResults = 0;
};

struct PexelsQuota {
    bool      known         = false; // aun no llego ninguna respuesta real de Pexels
    int       limit         = 0;
    int       remaining     = 0;
    long long resetUnixTime = 0;
};

class PexelsService {
public:
    static PexelsService& Get();

    // Bloqueante — llamar desde un hilo de fondo (ver OverlayCanvasEditor,
    // que la invoca via std::async para no congelar la UI).
    PexelsSearchResult Search(const std::string& query, int page = 1, int perPage = 12);

    // Descarga una imagen por su URL publica (CDN de Pexels, sin auth) a disco.
    // Tambien bloqueante.
    bool DownloadImage(const std::string& url, const std::string& destPath);

    PexelsQuota GetQuota() const { return m_Quota; }
    bool        HasApiKey() const;

    // true si se puede disparar una busqueda ya mismo (hay key, no hay
    // cooldown activo, y no se agoto la cuota conocida). msgOut explica por
    // que no, si corresponde.
    bool CanSearchNow(std::string* reasonOut = nullptr) const;

private:
    PexelsService() = default;

    PexelsQuota m_Quota;
    double      m_LastRequestTime = 0.0; // segundos (reloj monotonico propio)
    std::unordered_map<std::string, PexelsSearchResult> m_Cache;

    static constexpr double kMinRequestGapSec = 1.5;
};

} // namespace ProyecThor::Core
