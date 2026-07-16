#include "TransitionPanel.h"
#include <imgui.h>
#include <cmath>
#include <algorithm>
#include <vector>
#include "UIStrings.h"

namespace ProyecThor::UI {

// =============================================================================
//  EaseInOut — curva suave para la transicion
// =============================================================================
float TransitionPanel::EaseInOut(float t)
{
    // Smoothstep: 3t^2 - 2t^3
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// =============================================================================
//  Trigger / Update
// =============================================================================
void TransitionPanel::Trigger()
{
    if (m_SelectedType == TransitionType::None) return;
    m_Elapsed  = 0.0f;
    m_Progress = 0.0f;
    m_Active   = true;
}

void TransitionPanel::Update(float dt)
{
    if (!m_Active) return;

    m_Elapsed += dt;
    float raw  = (m_Duration > 0.0f) ? (m_Elapsed / m_Duration) : 1.0f;
    m_Progress = EaseInOut(std::clamp(raw, 0.0f, 1.0f));

    if (m_Elapsed >= m_Duration) {
        m_Elapsed  = 0.0f;
        m_Progress = 1.0f;
        m_Active   = false;
    }
}

// =============================================================================
//  Offsets (Posición X / Y)
// =============================================================================
float TransitionPanel::GetOutgoingOffsetX() const
{
    switch (m_SelectedType) {
        case TransitionType::SlideLeft:  
        case TransitionType::UncoverLeft:  
            return -m_Progress;
        case TransitionType::SlideRight: 
        case TransitionType::UncoverRight: 
            return m_Progress;
        default:                         
            return 0.0f;
    }
}

float TransitionPanel::GetOutgoingOffsetY() const
{
    switch (m_SelectedType) {
        case TransitionType::SlideUp:   
        case TransitionType::UncoverUp:   
            return -m_Progress;
        case TransitionType::SlideDown: 
        case TransitionType::UncoverDown: 
            return m_Progress;
        default:                          
            return 0.0f;
    }
}

float TransitionPanel::GetIncomingOffsetX() const
{
    switch (m_SelectedType) {
        case TransitionType::SlideLeft:  
        case TransitionType::CoverLeft:  
            return 1.0f - m_Progress;
        case TransitionType::SlideRight: 
        case TransitionType::CoverRight: 
            return -(1.0f - m_Progress);
        default:                         
            return 0.0f;
    }
}

float TransitionPanel::GetIncomingOffsetY() const
{
    switch (m_SelectedType) {
        case TransitionType::SlideUp:   
        case TransitionType::CoverUp:   
            return 1.0f - m_Progress;
        case TransitionType::SlideDown: 
        case TransitionType::CoverDown: 
            return -(1.0f - m_Progress);
        default:                          
            return 0.0f;
    }
}

// =============================================================================
//  Alpha (Opacidad para Disolver)
// =============================================================================
float TransitionPanel::GetOutgoingAlpha() const
{
    switch (m_SelectedType) {
        case TransitionType::Fade:
        case TransitionType::ZoomIn:
        case TransitionType::ZoomOut:
            // Salida y entrada SECUENCIAL, no simultanea. Primera mitad
            // de la transicion (progress 0 -> 0.5): el texto saliente se
            // desvanece de 1 a 0. Segunda mitad: ya esta invisible.
            // Antes ambas capas se dibujaban semitransparentes al mismo
            // tiempo en el mismo lugar (offset 0,0), lo que con alpha
            // secuencial de dos capas identicas producia un dip visual
            // de opacidad a mitad de camino en vez de un crossfade limpio.
            return std::max(0.0f, 1.0f - (m_Progress / 0.5f));
        default:
            return 1.0f;
    }
}

float TransitionPanel::GetIncomingAlpha() const
{
    switch (m_SelectedType) {
        case TransitionType::Fade:
        case TransitionType::ZoomIn:
        case TransitionType::ZoomOut:
            // Segunda mitad de la transicion (progress 0.5 -> 1): el
            // texto entrante aparece de 0 a 1. Durante la primera mitad
            // permanece invisible, mientras el saliente termina de irse.
            return std::max(0.0f, (m_Progress - 0.5f) / 0.5f);
        default:
            return 1.0f;
    }
}

// =============================================================================
//  Scale (Escala para Zoom)
// =============================================================================
float TransitionPanel::GetOutgoingScale() const
{
    switch (m_SelectedType) {
        case TransitionType::ZoomIn:  
            return 1.0f + (m_Progress * 0.5f); // Se agranda mientras desaparece
        case TransitionType::ZoomOut: 
            return 1.0f - (m_Progress * 0.5f); // Se achica mientras desaparece
        default: 
            return 1.0f;
    }
}

float TransitionPanel::GetIncomingScale() const
{
    switch (m_SelectedType) {
        case TransitionType::ZoomIn:  
            return 0.5f + (m_Progress * 0.5f); // Viene desde atrás (pequeño a normal)
        case TransitionType::ZoomOut: 
            return 1.5f - (m_Progress * 0.5f); // Viene desde adelante (grande a normal)
        default: 
            return 1.0f;
    }
}

// =============================================================================
//  Render — panel de control de transiciones interactivo
// =============================================================================
void TransitionPanel::Render()
{
    ImGui::Begin(GetName().c_str());

    const auto& str = ProyecThor::UI::GetUIStrings();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.369f, 0.420f, 1.0f, 1.0f));
    ImGui::TextUnformatted(str.transTitle);
    ImGui::PopStyleColor();
    
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1.0f, 1.0f, 1.0f, 0.06f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // ── Estructura de Datos para el Menú Desplegable ─────────────────────────
    struct TypeOption { TransitionType type; const char* label; const char* desc; };
    
    struct TransitionCategory {
        const char* title;
        std::vector<TypeOption> options;
    };

    static const TransitionCategory categories[] = {
        { "Básicas", {
            { TransitionType::None,    "Sin transición", "El texto cambia instantáneamente." },
            { TransitionType::Fade,    "Disolver",       "Transición suave de opacidad (Crossfade)." },
            { TransitionType::ZoomIn,  "Zoom In",        "El texto aparece desde el fondo." },
            { TransitionType::ZoomOut, "Zoom Out",       "El texto aparece desde el frente." }
        }},
        { "Barridos (Slide)", {
            { TransitionType::SlideLeft,  "← Barrido Izq", "El texto nuevo empuja al anterior hacia la izq." },
            { TransitionType::SlideRight, "Barrido Der →", "El texto nuevo empuja al anterior hacia la der." },
            { TransitionType::SlideUp,    "↑ Barrido Arr", "El texto nuevo empuja al anterior hacia arriba." },
            { TransitionType::SlideDown,  "↓ Barrido Aba", "El texto nuevo empuja al anterior hacia abajo." }
        }},
        { "Cubrir (Cover)", {
            { TransitionType::CoverLeft,  "← Cubrir Izq",  "El texto nuevo entra sobre el actual." },
            { TransitionType::CoverRight, "Cubrir Der →",  "El texto nuevo entra sobre el actual." },
            { TransitionType::CoverUp,    "↑ Cubrir Arr",  "El texto nuevo entra desde abajo cubriendo." },
            { TransitionType::CoverDown,  "↓ Cubrir Aba",  "El texto nuevo entra desde arriba cubriendo." }
        }},
        { "Descubrir (Uncover)", {
            { TransitionType::UncoverLeft,  "← Revelar Izq", "El texto actual sale revelando el nuevo." },
            { TransitionType::UncoverRight, "Revelar Der →", "El texto actual sale revelando el nuevo." },
            { TransitionType::UncoverUp,    "↑ Revelar Arr", "El texto actual sale revelando el nuevo." },
            { TransitionType::UncoverDown,  "↓ Revelar Aba", "El texto actual sale revelando el nuevo." }
        }}
    };

    // ── Renderizado de Categorías (Acordeones + Grid) ────────────────────────
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(8.0f, 8.0f));

    for (const auto& cat : categories)
    {
        // Verificar si la transición activa pertenece a esta categoría para abrirla por defecto
        bool hasActiveChild = false;
        for (const auto& opt : cat.options) {
            if (m_SelectedType == opt.type) { hasActiveChild = true; break; }
        }

        if (hasActiveChild) ImGui::SetNextItemOpen(false, ImGuiCond_Once);

        ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(1.0f, 1.0f, 1.0f, 0.05f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.10f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImVec4(1.0f, 1.0f, 1.0f, 0.15f));
        
        bool isOpen = ImGui::CollapsingHeader(cat.title);
        ImGui::PopStyleColor(3);

        if (isOpen)
        {
            ImGui::Spacing();
            
            // Iniciamos la Grilla de 2 columnas
            if (ImGui::BeginTable(cat.title, 2, ImGuiTableFlags_SizingStretchProp))
            {
                for (size_t i = 0; i < cat.options.size(); ++i)
                {
                    const auto& opt = cat.options[i];
                    bool selected = (m_SelectedType == opt.type);

                    ImGui::TableNextColumn();

                    // Estilos dinámicos para el botón
                    if (selected) {
                        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.38f, 0.82f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.48f, 0.92f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.16f, 0.30f, 0.70f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.92f, 0.95f, 1.00f, 1.0f));
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f, 0.12f, 0.18f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.20f, 0.30f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.22f, 0.25f, 0.38f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.65f, 0.68f, 0.78f, 1.0f));
                    }

                    // Forzamos el ancho del botón para que llene la celda de la grilla
                    if (ImGui::Button(opt.label, ImVec2(-FLT_MIN, 32.0f))) {
                        m_SelectedType = opt.type;
                    }

                    ImGui::PopStyleColor(4);

                    // Tooltip interactivo al pasar el mouse
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 1.0f));
                        ImGui::TextUnformatted(opt.label);
                        ImGui::PopStyleColor();
                        ImGui::Separator();
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                        ImGui::TextUnformatted(opt.desc);
                        ImGui::PopStyleColor();
                        ImGui::EndTooltip();
                    }
                }
                ImGui::EndTable();
            }
            ImGui::Spacing();
        }
    }
    ImGui::PopStyleVar(2);

    // ── Controles de Animación (Duración y Barra) ────────────────────────────
    if (m_SelectedType != TransitionType::None)
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1.0f, 1.0f, 1.0f, 0.06f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();

        // Icono de reloj + texto
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
        ImGui::TextUnformatted("Duración de la transición");
        ImGui::PopStyleColor();

        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.1f, 0.1f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.15f, 0.15f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_SliderGrab,     ImVec4(0.369f, 0.420f, 1.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        
        ImGui::SliderFloat("##dur", &m_Duration, 0.1f, 3.0f, "%.2f Segundos");
        
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        // Barra de progreso animada
        if (m_Active)
        {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.369f, 0.420f, 1.0f, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg,       ImVec4(0.08f, 0.08f, 0.12f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            
            // Animación sutil de texto en la barra
            char progressText[32];
            snprintf(progressText, sizeof(progressText), "Ejecutando... %d%%", (int)(m_Progress * 100));
            ImGui::ProgressBar(m_Progress, ImVec2(-FLT_MIN, 16.0f), progressText);
            
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
        }
    }

    // ── Pie de página / Ayuda ────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1.0f, 1.0f, 1.0f, 0.06f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

 ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.48f, 0.60f, 1.0f));
    ImGui::TextWrapped("%s", str.transTip);
    ImGui::PopStyleColor();

    ImGui::End();
}
} // namespace ProyecThor::UI