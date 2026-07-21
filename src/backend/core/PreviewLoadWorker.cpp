#include "PreviewLoadWorker.h"
#include "backend/media/VLCBasePlayer.h"

namespace ProyecThor::Core {

PreviewLoadWorker::PreviewLoadWorker()
{
    m_Running.store(true);
    m_Thread = std::thread(&PreviewLoadWorker::ThreadFunc, this);
}

PreviewLoadWorker::~PreviewLoadWorker()
{
    m_Running.store(false);
    m_Cv.notify_all();
    if (m_Thread.joinable())
        m_Thread.join();
}

void PreviewLoadWorker::RequestLoad(VLCBasePlayer* player, const std::string& path, bool loop, bool startMuted)
{
    if (!player) return;
    {
        std::lock_guard<std::mutex> lk(m_Mutex);
        m_Pending = PendingCommand{ player, /*isStop=*/false, path, loop, startMuted };
    }
    m_Cv.notify_one();
}

void PreviewLoadWorker::RequestStop(VLCBasePlayer* player)
{
    if (!player) return;
    {
        std::lock_guard<std::mutex> lk(m_Mutex);
        m_Pending = PendingCommand{ player, /*isStop=*/true, "", false, true };
    }
    m_Cv.notify_one();
}

void PreviewLoadWorker::ThreadFunc()
{
    while (true) {
        PendingCommand cmd;
        {
            std::unique_lock<std::mutex> lk(m_Mutex);
            m_Cv.wait(lk, [this] { return !m_Running.load() || m_Pending.has_value(); });
            if (!m_Pending.has_value())
                return; // solo puede pasar si nos pidieron parar y no quedo nada pendiente
            cmd = std::move(*m_Pending);
            m_Pending.reset();
        }

        if (cmd.isStop)
            cmd.player->Stop();
        else
            cmd.player->Play(cmd.path, cmd.loop, cmd.startMuted);

        if (!m_Running.load())
            return;
    }
}

} // namespace ProyecThor::Core
