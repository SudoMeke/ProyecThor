#include "MediaConverter.h"
#include "FfmpegPath.h"
#include <cstdio>

#if defined(_WIN32)
    #include "HiddenProcess.h"
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
        // RenderConverterSection en LibraryPanel.cpp). Sin -hwaccel
        // ni flags de codec: se apoya en que ffmpeg elige un codec/
        // contenedor razonable a partir de la extension de salida, igual
        // que hace cualquier conversor simple de formato.
        std::string cmd = ffmpegPath + " -y -loglevel error -i \"" + inputPath + "\" \"" + outputPath + "\"";

        bool        success = false;
        std::string lastErrorLine;

#if defined(_WIN32)
        // CREATE_NO_WINDOW (via HiddenProcess): sin esto aparece una
        // consola de ffmpeg tapando la app por cada conversion. Tambien
        // captura stdout+stderr combinados -- antes con _popen(cmd,"r") se
        // perdia stderr (donde ffmpeg reporta los errores de verdad),
        // quedaba solo un generico "fallo" sin decir por que.
        FILE* pipe = nullptr;
        void* proc = nullptr;
        if (StartHiddenProcess(cmd, /*wantStdinPipe=*/false, nullptr,
                                /*wantOutputCapture=*/true, &pipe, &proc)) {
            char buf[512];
            while (pipe && std::fgets(buf, sizeof(buf), pipe)) {
                std::string line(buf);
                if (!line.empty() && line.back() == '\n') line.pop_back();
                if (!line.empty()) lastErrorLine = line;
            }
            if (pipe) std::fclose(pipe);
            int status = WaitHiddenProcess(proc);
            success = (status == 0);
        }
#else
        FILE* pipe = PT_POPEN((cmd + " 2>&1").c_str(), "r");
        if (pipe) {
            char buf[512];
            while (std::fgets(buf, sizeof(buf), pipe)) {
                std::string line(buf);
                if (!line.empty() && line.back() == '\n') line.pop_back();
                if (!line.empty()) lastErrorLine = line;
            }
            int status = PT_PCLOSE(pipe);
            success = (status == 0);
        }
#endif

        std::lock_guard<std::mutex> lock(m_ResultMutex);
        m_HasPendingResult = true;
        m_LastSuccess       = success;
        m_LastMessage       = success
            ? ("Listo: " + outputPath)
            : ("La conversion fallo" + (lastErrorLine.empty() ? std::string(".") : (": " + lastErrorLine)));
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
