#include "MediaConverter.h"
#include "FfmpegPath.h"
#include <cstdio>

#if defined(_WIN32)
    #define PT_POPEN  _popen
    #define PT_PCLOSE _pclose
#else
    #define PT_POPEN  popen
    #define PT_PCLOSE pclose
#endif

namespace ProyecThor::Core {

MediaConverter::~MediaConverter() {
    if (m_Thread.joinable()) m_Thread.join();
}

bool MediaConverter::Start(const std::string& inputPath, const std::string& outputPath, std::string* errorOut) {
    if (m_Running.load()) {
        if (errorOut) *errorOut = "Ya hay una conversion en curso.";
        return false;
    }

    std::string ffmpegPath = FfmpegPath();
    if (!FfmpegAvailable(ffmpegPath)) {
        if (errorOut) *errorOut = "No se encontro ffmpeg (deberia estar empaquetado junto a la app). "
                                   "Si lo borraste, reinstala ProyecThor o instala ffmpeg y agregalo al PATH.";
        return false;
    }

    if (m_Thread.joinable()) m_Thread.join(); // conversion anterior ya terminada, solo falta unir el hilo

    m_Running = true;
    {
        std::lock_guard<std::mutex> lock(m_ResultMutex);
        m_HasPendingResult = false;
    }

    m_Thread = std::thread([this, ffmpegPath, inputPath, outputPath]() {
        // -y: sobreescribe si por algun motivo ya existe el destino (el
        // llamador ya se encarga de elegir un nombre libre, ver
        // RenderConverterSection en LibraryManagerPanel.cpp). Sin -hwaccel
        // ni flags de codec: se apoya en que ffmpeg elige un codec/
        // contenedor razonable a partir de la extension de salida, igual
        // que hace cualquier conversor simple de formato.
        std::string cmd = ffmpegPath + " -y -loglevel error -i \"" + inputPath + "\" \"" + outputPath + "\"";

        FILE* pipe = PT_POPEN(cmd.c_str(), "r");
        bool  success = false;
        if (pipe) {
            char buf[256];
            while (std::fgets(buf, sizeof(buf), pipe)) {} // drena stderr (ffmpeg -loglevel error solo emite fallos reales)
            int status = PT_PCLOSE(pipe);
            success = (status == 0);
        }

        std::lock_guard<std::mutex> lock(m_ResultMutex);
        m_HasPendingResult = true;
        m_LastSuccess       = success;
        m_LastMessage       = success ? ("Listo: " + outputPath)
                                       : "La conversion fallo (revisa que el archivo de origen sea valido).";
        m_Running = false;
    });

    return true;
}

bool MediaConverter::PollFinished(bool& outSuccess, std::string& outMessage) {
    std::lock_guard<std::mutex> lock(m_ResultMutex);
    if (!m_HasPendingResult) return false;

    m_HasPendingResult = false;
    outSuccess         = m_LastSuccess;
    outMessage         = m_LastMessage;
    return true;
}

} // namespace ProyecThor::Core
