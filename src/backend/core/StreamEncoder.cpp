#include "StreamEncoder.h"
#include <cstdlib>

#if defined(_WIN32)
    #include <windows.h>
    #define PT_POPEN  _popen
    #define PT_PCLOSE _pclose
#else
    #define PT_POPEN  popen
    #define PT_PCLOSE pclose
#endif

namespace ProyecThor::Core {

// En Windows, ffmpeg.exe viene EMPAQUETADO al lado del ejecutable (ver
// extrabuild/ffmpeg.exe, mismo criterio que yt-dlp.exe): asi la
// transmision funciona para cualquier usuario sin que tenga que instalar
// ni configurar nada por su cuenta, como en OBS. Se resuelve la ruta
// absoluta via GetModuleFileName en vez de confiar en que el directorio de
// trabajo actual sea el de la app (no siempre es asi segun como se lance
// el acceso directo). En el resto de plataformas se espera un ffmpeg del
// sistema (via el gestor de paquetes de la distro), igual criterio que ya
// usa el resto de la app para dependencias externas en Linux.
static std::string FfmpegPath() {
#if defined(_WIN32)
    char exePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) > 0) {
        std::string path(exePath);
        size_t slash = path.find_last_of("\\/");
        if (slash != std::string::npos) {
            std::string candidate = path.substr(0, slash + 1) + "ffmpeg.exe";
            FILE* f = std::fopen(candidate.c_str(), "rb");
            if (f) { std::fclose(f); return "\"" + candidate + "\""; }
        }
    }
    return "ffmpeg"; // fallback: PATH del sistema, por si no esta empaquetado
#else
    return "ffmpeg";
#endif
}

static bool FfmpegAvailable(const std::string& ffmpegPath) {
    FILE* probe = PT_POPEN((ffmpegPath + " -version").c_str(), "r");
    if (!probe) return false;

    char    buf[256];
    bool    found = false;
    if (fgets(buf, sizeof(buf), probe) && std::string(buf).find("ffmpeg") != std::string::npos)
        found = true;

    PT_PCLOSE(probe);
    return found;
}

StreamEncoder::~StreamEncoder() {
    Stop();
}

bool StreamEncoder::Start(const std::string& rtmpUrl, int width, int height, int fps,
                           int videoBitrateKbps, std::string* errorOut) {
    if (m_Pipe) {
        if (errorOut) *errorOut = "Ya hay un stream activo; llama a Stop() primero.";
        return false;
    }
    if (rtmpUrl.empty() || rtmpUrl.rfind("rtmp://", 0) != 0) {
        if (errorOut) *errorOut = "La URL de destino debe empezar con rtmp://";
        return false;
    }
    std::string ffmpegPath = FfmpegPath();
    if (!FfmpegAvailable(ffmpegPath)) {
        if (errorOut) *errorOut = "No se encontro ffmpeg (deberia estar empaquetado junto a la app). "
                                   "Si lo borraste, reinstala ProyecThor o instala ffmpeg y agregalo al PATH.";
        return false;
    }

    // Ancho/alto par: requisito de libx264 con -pix_fmt yuv420p (submuestreo
    // de croma 4:2:0, necesita dimensiones divisibles por 2).
    width  &= ~1;
    height &= ~1;
    if (width <= 0 || height <= 0) {
        if (errorOut) *errorOut = "Resolucion invalida.";
        return false;
    }

    int bufsizeKbps = videoBitrateKbps * 2;
    int gop         = fps * 2;

    // Pista de audio silenciosa (anullsrc): varios servidores RTMP (Twitch/
    // YouTube incluidos) rechazan o se comportan mal con streams sin pista
    // de audio -- mas adelante se puede reemplazar por audio real, por
    // ahora es lo que hace viable "ir en vivo" ya mismo.
    char cmd[1536];
    std::snprintf(cmd, sizeof(cmd),
        "%s -y -loglevel warning "
        "-f lavfi -i anullsrc=channel_layout=stereo:sample_rate=44100 "
        "-f rawvideo -pix_fmt rgba -s %dx%d -r %d -i - "
        "-map 1:v -map 0:a "
        "-c:v libx264 -preset veryfast -tune zerolatency -pix_fmt yuv420p "
        "-b:v %dk -maxrate %dk -bufsize %dk -g %d "
        "-c:a aac -b:a 128k -shortest "
        "-f flv \"%s\"",
        ffmpegPath.c_str(), width, height, fps,
        videoBitrateKbps, videoBitrateKbps, bufsizeKbps, gop,
        rtmpUrl.c_str());

    m_Pipe = PT_POPEN(cmd, "wb");
    if (!m_Pipe) {
        if (errorOut) *errorOut = "No se pudo iniciar el proceso de ffmpeg.";
        return false;
    }

    m_Width        = width;
    m_Height       = height;
    m_Fps          = fps > 0 ? fps : 30;
    m_LastPushTime = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    return true;
}

void StreamEncoder::Stop() {
    if (!m_Pipe) return;
    std::fflush(m_Pipe);
    PT_PCLOSE(m_Pipe);
    m_Pipe = nullptr;
}

void StreamEncoder::PushFrame(const uint8_t* rgba, int width, int height) {
    if (!m_Pipe || !rgba) return;
    if (width != m_Width || height != m_Height) return; // frame de otro tamaño -- se descarta

    auto now         = std::chrono::steady_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(now - m_LastPushTime).count();
    double targetMs  = 1000.0 / (double)m_Fps;
    if (elapsedMs < targetMs) return; // todavia no toca el proximo frame -- se descarta

    m_LastPushTime = now;

    size_t frameBytes = (size_t)width * (size_t)height * 4;
    size_t written     = std::fwrite(rgba, 1, frameBytes, m_Pipe);
    if (written != frameBytes) {
        // El pipe se corto (ffmpeg murio, red caida, etc.) -- cerramos
        // limpio para que IsStreaming() refleje la realidad en el proximo
        // frame de UI en vez de seguir "streameando" a la nada.
        Stop();
    }
}

} // namespace ProyecThor::Core
