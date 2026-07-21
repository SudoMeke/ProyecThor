#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace ProyecThor::Core {

class VLCBasePlayer;

// ── PreviewLoadWorker ────────────────────────────────────────────────────────
// Ejecuta Play()/Stop() del reproductor de Preview en un hilo dedicado.
// VLCBasePlayer::Play()/Stop() son sincronicos (bloquean al que los llama
// mientras VLC abre/cierra el archivo) y antes se llamaban directo desde el
// hilo principal — el MISMO hilo que actualiza y dibuja el video que esta en
// vivo al publico. Una carga de Preview lenta (disco, archivo grande) podia
// entonces trabar tambien el video en vivo, aunque sean instancias de VLC
// completamente separadas. Este worker saca esa espera del hilo principal:
// el video en vivo nunca depende de cuanto tarde el Preview en cargar.
//
// Politica "ultimo pedido gana" (mismo patron que FrameEncodeWorker): si
// llega un pedido nuevo mientras el anterior todavia se esta procesando, el
// anterior se descarta apenas el que esta en curso termina — no se acumula
// una cola de pedidos viejos.
class PreviewLoadWorker {
public:
    PreviewLoadWorker();
    ~PreviewLoadWorker();

    PreviewLoadWorker(const PreviewLoadWorker&)            = delete;
    PreviewLoadWorker& operator=(const PreviewLoadWorker&) = delete;

    // player debe seguir siendo valido mientras exista este worker (los
    // VLCBasePlayer de BackgroundLayer viven mientras vive PresentationCore).
    void RequestLoad(VLCBasePlayer* player, const std::string& path, bool loop, bool startMuted);
    void RequestStop(VLCBasePlayer* player);

private:
    struct PendingCommand {
        VLCBasePlayer* player = nullptr;
        bool           isStop = false;
        std::string    path;
        bool           loop       = false;
        bool           startMuted = true;
    };

    void ThreadFunc();

    std::thread                   m_Thread;
    std::atomic<bool>             m_Running{ false };
    std::mutex                    m_Mutex;
    std::condition_variable       m_Cv;
    std::optional<PendingCommand> m_Pending; // protegido por m_Mutex
};

} // namespace ProyecThor::Core
