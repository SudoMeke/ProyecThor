#pragma once

#include "../IPanel.h"
#include <string>

namespace ProyecThor::UI {

enum class TransitionType
{
    None,
    Fade,           // Sale, luego entra (fundido secuencial, no crossfade)
    ZoomIn,         // El texto nuevo viene desde el fondo acercándose
    ZoomOut,        // El texto nuevo viene desde el frente alejándose
    SlideLeft,
    SlideRight,
    SlideUp,
    SlideDown,
    CoverLeft,      // Entra por la derecha cubriendo al anterior
    CoverRight,     // Entra por la izquierda cubriendo al anterior
    CoverUp,        // Entra desde abajo cubriendo al anterior
    CoverDown,      // Entra desde arriba cubriendo al anterior
    UncoverLeft,    // El anterior sale hacia la izquierda revelando el nuevo
    UncoverRight,   // El anterior sale hacia la derecha revelando el nuevo
    UncoverUp,      // El anterior sale hacia arriba revelando el nuevo
    UncoverDown     // El anterior sale hacia abajo revelando el nuevo
};

class TransitionPanel : public IPanel {
public:
    TransitionPanel() = default;
    ~TransitionPanel() override = default;

    std::string GetName() const override { return "Transiciones"; }
    void Render() override;

    float GetDuration() const { return m_Duration; }

    // Llamado desde UIManager justo antes de dibujar el texto en el proyector.
    bool  IsActive()          const { return m_Active; }
    float GetProgress()       const { return m_Progress; }

    // Desplazamientos (Posición) — usan m_Progress en su rango completo
    // 0..1, ya que Slide/Cover/Uncover son movimiento continuo (el texto
    // saliente y el entrante se mueven a la vez, en direcciones/posiciones
    // distintas). No necesitan la division en mitades que si usan Fade/Zoom.
    float GetOutgoingOffsetX()  const;
    float GetOutgoingOffsetY()  const;
    float GetIncomingOffsetX()  const;
    float GetIncomingOffsetY()  const;

    // Opacidad (Alpha) para Fade/ZoomIn/ZoomOut. Salida y entrada son
    // SECUENCIALES (ver GetOutgoingLocalT/GetIncomingLocalT), no un
    // crossfade simultaneo: evita el parpadeo raro cuando el texto
    // saliente y el entrante son identicos (misma estrofa repetida).
    float GetOutgoingAlpha()    const;
    float GetIncomingAlpha()    const;

    // Escala para los efectos de Zoom. Sincronizada con las mismas mitades
    // que el alpha (via GetOutgoingLocalT/GetIncomingLocalT), para que el
    // "punch" del zoom termine justo cuando el texto se vuelve invisible,
    // en vez de seguir escalando fuera de su ventana visible.
    float GetOutgoingScale()    const;
    float GetIncomingScale()    const;

    // Iniciar una transicion. Llamar cuando el texto de presentacion cambia.
    void Trigger();

    // Avanzar el tiempo de la transicion. Llamar cada frame con el dt real.
    void Update(float dt);

    // Estado publico leido por UIManager para saber que transicion aplicar.
    TransitionType GetCurrentType() const { return m_SelectedType; }

private:
    // ── Progreso local por mitad (solo para tipos secuenciales: Fade/Zoom) ──
    // El progreso total (m_Progress, 0..1, ya suavizado por EaseInOut) se
    // divide en dos mitades iguales: [0, kSequentialSplit] para la salida
    // y [kSequentialSplit, 1] para la entrada. Estos helpers remapean esa
    // mitad a un 0..1 local, para que alpha y escala usen exactamente la
    // misma ventana de tiempo y no queden desincronizados entre si.
    float GetOutgoingLocalT() const;
    float GetIncomingLocalT() const;

    // Punto de corte entre "salida" y "entrada" dentro de la duracion
    // total configurada. 0.5 = mitad y mitad. Se deja como constante unica
    // por si en el futuro se prefiere un pequeno solape (ej. 0.45/0.55)
    // para suavizar duraciones muy cortas.
    static constexpr float kSequentialSplit = 0.5f;

    TransitionType m_SelectedType = TransitionType::Fade; // Por defecto Disolver suele ser el más elegante
    float          m_Duration     = 1.0f;   // segundos
    float          m_Elapsed      = 0.0f;
    bool           m_Active       = false;
    float          m_Progress     = 0.0f;   // 0..1, eased

    static float EaseInOut(float t);
};

} // namespace ProyecThor::UI