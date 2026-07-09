#pragma once

#include "../IPanel.h"
#include <string>

namespace ProyecThor::UI {

enum class TransitionType
{
    None,
    Fade,           // Disolver cruzado
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
    
    // Desplazamientos (Posición)
    float GetOutgoingOffsetX()  const;
    float GetOutgoingOffsetY()  const;
    float GetIncomingOffsetX()  const;
    float GetIncomingOffsetY()  const;

    // Opacidad (Alpha) para el efecto Disolver
    float GetOutgoingAlpha()    const;
    float GetIncomingAlpha()    const;

    // Escala para los efectos de Zoom
    float GetOutgoingScale()    const;
    float GetIncomingScale()    const;

    // Iniciar una transicion. Llamar cuando el texto de presentacion cambia.
    void Trigger();

    // Avanzar el tiempo de la transicion. Llamar cada frame con el dt real.
    void Update(float dt);

    // Estado publico leido por UIManager para saber que transicion aplicar.
    TransitionType GetCurrentType() const { return m_SelectedType; }

private:
    TransitionType m_SelectedType = TransitionType::Fade; // Por defecto Disolver suele ser el más elegante
    float          m_Duration     = 1.0f;   // segundos
    float          m_Elapsed      = 0.0f;
    bool           m_Active       = false;
    float          m_Progress     = 0.0f;   // 0..1, eased

    static float EaseInOut(float t);
};

} // namespace ProyecThor::UI