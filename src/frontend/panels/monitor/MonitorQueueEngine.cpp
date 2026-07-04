#include "MonitorQueueEngine.h"
#include "MonitorQueueHelpers.h"
#include "MonitorQueueIO.h"
#include "backend/core/PresentationCore.h"
#include "backend/media/VLCBasePlayer.h"
#include <filesystem>
#include <algorithm>
#include <iostream>

namespace ProyecThor::UI {
using namespace QueueHelpers;

static Core::VLCBasePlayer* ActivePlayer()
{
    return Core::PresentationCore::Get().GetBackgroundPlayer();
}

void MonitorQueueEngine::Load()
{
    QueueIO::LoadQueue(m_Items);
    m_CurrentIndex  = -1;
    m_SelectedIndex = -1;
    m_State         = QueueState::Stopped;
}

void MonitorQueueEngine::Save() const { QueueIO::SaveQueue(m_Items); }

void MonitorQueueEngine::Add(const std::string& fullPath)
{
    m_Items.push_back(QueueEntry(k_PfxLocal, fullPath));
    Save();
}

void MonitorQueueEngine::AddURL(const std::string& url)
{
    m_Items.push_back(QueueEntry(k_PfxURL, url));
    Save();
}

static int RemapAfterMove(int tracked, int from, int to)
{
    if (tracked < 0)     return tracked;
    if (tracked == from) return to;
    if (from < to) { if (tracked > from && tracked <= to) return tracked - 1; }
    else           { if (tracked >= to && tracked < from) return tracked + 1; }
    return tracked;
}

static int RemapAfterRemove(int tracked, int removed, int newSize)
{
    if (tracked < 0)        return -1;
    if (tracked == removed) return -1;
    if (tracked > removed)  return tracked - 1;
    if (tracked >= newSize) return newSize - 1;
    return tracked;
}

void MonitorQueueEngine::Move(int from, int to)
{
    if (from < 0 || from >= (int)m_Items.size() ||
        to   < 0 || to   >= (int)m_Items.size() || from == to)
        return;

    std::string moved = m_Items[from];
    m_Items.erase(m_Items.begin() + from);
    m_Items.insert(m_Items.begin() + to, moved);

    m_CurrentIndex  = RemapAfterMove(m_CurrentIndex,  from, to);
    m_SelectedIndex = RemapAfterMove(m_SelectedIndex, from, to);
    Save();
}

void MonitorQueueEngine::Remove(int index)
{
    if (index < 0 || index >= (int)m_Items.size()) return;
    if (index == m_CurrentIndex)
        Stop();

    m_Items.erase(m_Items.begin() + index);
    int sz = (int)m_Items.size();
    m_CurrentIndex  = RemapAfterRemove(m_CurrentIndex,  index, sz);
    m_SelectedIndex = RemapAfterRemove(m_SelectedIndex, index, sz);
    Save();
}

void MonitorQueueEngine::Clear()
{
    Stop();
    m_Items.clear();
    m_SelectedIndex = -1;
    Save();
}

void MonitorQueueEngine::ApplyAV()
{
    Core::PresentationCore::Get().SetLiveMute(m_Muted);
    Core::PresentationCore::Get().SetLiveVolume(
        m_Muted ? 0 : static_cast<int>(std::min(m_Volume, 2.0f) * 100.0f));
}

void MonitorQueueEngine::PlayIndex(int index)
{
    auto* p = ActivePlayer();
    if (!p) { Stop(); return; }

    // Salta automaticamente cualquier entrada sin ruta valida, para que
    // la cola jamas quede "pegada" sin avanzar.
    while (index >= 0 && index < (int)m_Items.size())
    {
        std::string path = QueuePath(m_Items[index]);
        if (path.empty())
        {
            std::cerr << "[Queue] Entrada " << index << " sin ruta valida, se omite.\n";
            ++index;
            continue;
        }

        m_CurrentIndex  = index;
        m_SelectedIndex = index;
        m_State         = QueueState::Playing;

        // loop = false SIEMPRE. La cola nunca debe pedirle al reproductor
        // que repita un clip a nivel nativo.
        Core::PresentationCore::Get().SetBackgroundMedia(path, /*isVideo=*/true);
        Core::PresentationCore::Get().SetProjecting(true);

        ApplyAV();
        p->SetPause(false);

        std::string displayName = std::filesystem::path(path).filename().string();
        Core::LibrarySelection sel;
        sel.title = displayName;
        sel.type  = Core::ItemType::Video;
        Core::PresentationCore::Get().SetSelection(sel);

        std::cout << "[Queue] Reproduciendo " << (index + 1) << "/" << m_Items.size()
                  << ": " << path << "\n";
        return;
    }

    // No quedo ningun item valido desde index en adelante: fin de cola.
    Stop();
}

void MonitorQueueEngine::TogglePlayStop()
{
    if (m_State == QueueState::Playing) { Stop(); return; }
    if (!m_Items.empty())
        PlayIndex(0); // siempre desde el principio: reproduce TODA la lista
}

void MonitorQueueEngine::Stop()
{
    Core::PresentationCore::Get().StopBackgroundMedia();
    m_CurrentIndex = -1;
    m_State        = QueueState::Stopped;
}

void MonitorQueueEngine::Update()
{
    if (m_State != QueueState::Playing || m_CurrentIndex < 0)
        return;

    auto* p = ActivePlayer();
    if (!p) { Stop(); return; }

    // Unica condicion de avance: el evento REAL de fin de clip que reporta
    // VLC. ConsumeEndReached() solo puede devolver true una vez por clip,
    // asi que este avance ocurre exactamente una vez, de forma instantanea,
    // sin ventana de tiempo en la que algo mas pueda "repetir" el clip.
    if (!p->ConsumeEndReached())
        return;

    PlayIndex(m_CurrentIndex + 1);
};
}
 // namespace ProyecThor::UI