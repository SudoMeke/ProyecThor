#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace ProyecThor::Core {

// ─────────────────────────────────────────────────────────────────────────────
//  MediaConverter — convierte un archivo de video/audio a otro formato
//  invocando "ffmpeg" (mismo binario empaquetado que usa StreamEncoder, ver
//  extrabuild/ffmpeg.exe) como subproceso, en un hilo de fondo para no
//  trabar la UI mientras dura la conversion.
//
//  No elige codecs a mano: deja que ffmpeg infiera el codec/contenedor a
//  partir de la extension del archivo de salida (comportamiento por
//  default de ffmpeg), que alcanza para una conversion tipica de formato
//  sin parametros raros.
// ─────────────────────────────────────────────────────────────────────────────
class MediaConverter {
public:
    ~MediaConverter();

    // Arranca la conversion en un hilo de fondo. Devuelve false (con
    // errorOut) si ya hay una conversion corriendo o si no se encontro
    // ffmpeg. No bloquea -- consultar IsRunning()/PollFinished() desde
    // Render() en cada frame.
    bool Start(const std::string& inputPath, const std::string& outputPath, std::string* errorOut = nullptr);

    bool IsRunning() const { return m_Running.load(); }

    // Devuelve true UNA sola vez, la primera vez que se llama despues de
    // que la conversion en curso termino (exito o error) -- llamadas
    // siguientes devuelven false hasta la proxima conversion. outSuccess/
    // outMessage quedan completos solo cuando devuelve true.
    bool PollFinished(bool& outSuccess, std::string& outMessage);

private:
    std::thread       m_Thread;
    std::atomic<bool> m_Running{ false };

    std::mutex  m_ResultMutex;
    bool        m_HasPendingResult = false;
    bool        m_LastSuccess      = false;
    std::string m_LastMessage;
};

} // namespace ProyecThor::Core
