#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <mutex>
#include <future>
#include <vector>

namespace ProyecThor::Core {

// ── StreamSnapshot ────────────────────────────────────────────────────────────
struct StreamSnapshot {
    bool        isProjecting   = false;
    std::string currentText;
    bool        showText       = false;
    float       textColor[4]   = { 1.0f, 1.0f, 1.0f, 1.0f };
    float       textSize       = 60.0f;
    int         textAlignment  = 1;
    int         vAlignment     = 1;
    float       margins[4]     = { 50.0f, 50.0f, 50.0f, 50.0f }; // L,T,R,B — igual que PresentationState
    bool        autoScale      = true;
    std::string fontFamily     = "Predeterminada";  // nombre de fuente activo
    float       bgColor[3]     = { 0.0f, 0.0f, 0.0f };
    bool        isBgVideo      = false;
    bool        hasFrame       = false;   // true cuando hay frame JPEG disponible
    int         refW           = 1920;    // resolución real del proyector destino
    int         refH           = 1080;
uint64_t    version        = 0;
    uint64_t    fontVersion    = 0;       // cambia solo cuando cambia la fuente (evita recargar /font en cada poll)

    uint64_t    transitionTrigger  = 0;
    int         transitionType     = 0;
    float       transitionDuration = 1.0f;
};

// ── StreamConfig ──────────────────────────────────────────────────────────────
// Qué capas transmitir y en qué calidad
struct StreamConfig {
    bool sendBackground = true;
    bool sendText       = true;
    bool sendOverlay    = true;

    enum class VideoMode {
        HighQuality,   // MJPEG continuo ~30fps, prioriza latencia baja
        LowLatency,    // JPEG polling ~200ms, menos CPU
        UltraStable    // NUEVO: MJPEG con framerate objetivo fijo (30/60fps),
                       // pacing preciso anti-drift y calidad casi sin perdida.
                       // Acepta mas delay (buffer/timing) a cambio de fluidez
                       // perfecta — pensado para pantallas/proyectores donde
                       // la estabilidad importa mas que la latencia minima.
    };
    VideoMode videoMode = VideoMode::LowLatency;

    int  jpegQuality    = 80;     // 1-100
    int  frameWidth     = 1280;
    int  frameHeight    = 720;

    // Solo aplica cuando videoMode == UltraStable. 30 o 60.
    int  targetFPS      = 60;
};
// ── NetworkStreamServer ───────────────────────────────────────────────────────
// Endpoints:
//   GET /           → HTML interactivo
//   GET /state      → JSON StreamSnapshot (long-poll ?since=<version>)
//   GET /frame      → JPEG único del frame actual  (LowLatency mode)
//   GET /stream     → MJPEG multipart stream       (HighQuality mode)
//   GET /font       → sirve el .ttf/.otf activo, para @font-face en el cliente
class NetworkStreamServer {
public:
    NetworkStreamServer();
    ~NetworkStreamServer();

    NetworkStreamServer(const NetworkStreamServer&)            = delete;
    NetworkStreamServer& operator=(const NetworkStreamServer&) = delete;

    // Provider de estado de texto/color
    using SnapshotProvider = std::function<StreamSnapshot()>;
    void SetSnapshotProvider(SnapshotProvider provider);

    // Provider de frame JPEG comprimido (puede ser nullptr si no hay video)
    // Debe devolver un vector<uint8_t> con los bytes JPEG, o vacío si no hay frame.
    using FrameProvider = std::function<std::vector<uint8_t>()>;
    void SetFrameProvider(FrameProvider provider);

    // Provider de la ruta absoluta al archivo de fuente (.ttf/.otf) activo.
    // Debe devolver "" si se está usando la fuente por defecto del sistema
    // (en ese caso el cliente web simplemente no aplica @font-face y usa
    // su fuente sans-serif habitual).
    using FontPathProvider = std::function<std::string()>;
    void SetFontPathProvider(FontPathProvider provider);

    // Configuración de capas y calidad
    void SetConfig(const StreamConfig& cfg);
    StreamConfig GetConfig() const;

    bool Start(int port = 8080);
    void Stop();

    bool        IsRunning()  const { return m_Running.load(); }
    int         GetPort()    const { return m_Port; }
    std::string GetBaseURL() const { return m_BaseURL; }

private:
    static std::string DetectLocalIP();
    static std::string BuildHTMLPage();
    std::string SnapshotToJSON(const StreamSnapshot& snap) const;

    void ServerThreadFunc(int port, std::promise<bool> startedPromise);

    std::atomic<bool>   m_Running { false };
    int                 m_Port    { 8080 };
    std::string         m_BaseURL;
    std::thread         m_Thread;
double m_LastCaptureTime = 0.0;
    mutable std::mutex  m_ProviderMutex;
    SnapshotProvider    m_SnapshotProvider;
    FrameProvider       m_FrameProvider;
    FontPathProvider    m_FontPathProvider;

    mutable std::mutex  m_ConfigMutex;
    StreamConfig        m_Config;
};

} // namespace ProyecThor::Core