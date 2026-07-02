#pragma once
#include <string>
#include <vector>

namespace ProyecThor::UI {

enum class QueueState { Stopped, Playing };

// Motor de la cola. Regla unica: al reproducirse, recorre TODOS los items
// en orden, uno por uno, sin loops y sin repetir ninguno. El avance es
// estrictamente por evento (VLC reporta fin de clip), nunca por tiempo.
class MonitorQueueEngine {
public:
    void Load();
    void Save() const;

    void Add(const std::string& fullPath);
    void AddURL(const std::string& url);

    void Move(int from, int to);
    void Remove(int index);
    void Clear();

    // Arranca en "index" y, si la entrada es invalida, salta sola hacia
    // adelante hasta encontrar una valida (o detiene si no queda ninguna).
    void PlayIndex(int index);

    // Boton unico de la cola: si esta detenida, arranca SIEMPRE desde el
    // item 0 (garantiza recorrer la lista completa). Si esta reproduciendo,
    // detiene.
    void TogglePlayStop();

    void Stop();

    // Llamar una vez por frame. Es el UNICO lugar del programa que debe
    // llamar a ConsumeEndReached() sobre el reproductor de fondo mientras
    // la cola esta activa.
    void Update();

    void  SetVolume(float vol0to2) { m_Volume = vol0to2; ApplyAV(); }
    void  SetMuted(bool muted)     { m_Muted  = muted;   ApplyAV(); }
    float GetVolume() const { return m_Volume; }
    bool  IsMuted()   const { return m_Muted; }

    const std::vector<std::string>& Items() const { return m_Items; }
    int  CurrentIndex()  const { return m_CurrentIndex; }
    int  SelectedIndex() const { return m_SelectedIndex; }
    void SetSelectedIndex(int i) { m_SelectedIndex = i; }
    QueueState State()  const { return m_State; }
    bool IsActive()     const { return m_State == QueueState::Playing; }

private:
    void ApplyAV();

    std::vector<std::string> m_Items;
    int        m_CurrentIndex  = -1;
    int        m_SelectedIndex = -1;
    QueueState m_State         = QueueState::Stopped;
    float      m_Volume        = 0.8f;
    bool       m_Muted         = false;
};

} // namespace ProyecThor::UI