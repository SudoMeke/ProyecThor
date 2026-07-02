#include "ControlPanel.h"
#include "UIManager.h"
#include "UIStrings.h"
#include "layers/LayersTheme.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>
#include <algorithm>
#include "backend/core/PresentationCore.h"
#include "../settings/SettingsManager.h"

namespace ProyecThor::UI {

static constexpr float kPulseSpeed     = 2.0f;
static constexpr float kPressAnimSpeed = 12.0f;
static constexpr float kHoverAnimSpeed = 15.0f;
static constexpr float kBtnRadius      = 28.0f;
static constexpr float kIconRadius     = 18.0f;

ControlPanel::ControlPanel(UIManager* uiManager)
    : m_UIManager(uiManager)
{}

// ─────────────────────────────────────────────────────────────────────────────
//  Render principal
// ─────────────────────────────────────────────────────────────────────────────
void ControlPanel::Render() {
    static double s_LastTime = glfwGetTime();
    double now = glfwGetTime();
    float  dt  = static_cast<float>(now - s_LastTime);
    s_LastTime = now;
    dt = std::min(dt, 0.05f);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.07f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if (ImGui::Begin(GetName().c_str(), nullptr, flags)) {

        if (m_isProjecting)
            m_PulseTime += dt * kPulseSpeed;
        else
            m_PulseTime = std::fmod(m_PulseTime + dt * 0.5f, 6.2831853f);

        if (m_PressAnim > 0.0f) {
            m_PressAnim -= dt * kPressAnimSpeed;
            if (m_PressAnim < 0.0f) m_PressAnim = 0.0f;
        }

        ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, 0.0f));

        float panelW = ImGui::GetContentRegionAvail().x;
        float panelH = ImGui::GetContentRegionAvail().y;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 winPos  = ImGui::GetWindowPos();

        // Fondo degradado sutil
        dl->AddRectFilledMultiColor(
            winPos,
            ImVec2(winPos.x + panelW, winPos.y + panelH),
            LPU32(ImVec4(0.06f, 0.06f, 0.08f, 1.0f)),
            LPU32(ImVec4(0.06f, 0.06f, 0.08f, 1.0f)),
            LPU32(ImVec4(0.03f, 0.03f, 0.04f, 1.0f)),
            LPU32(ImVec4(0.03f, 0.03f, 0.04f, 1.0f))
        );

        RenderMonitorInfo();
        RenderDivider();
        RenderProjectButton(dt);
        RenderStatusBar();
        RenderDivider();
        RenderActionRow(dt);
    }
    ImGui::End();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void ControlPanel::RenderDivider() {
    float panelW = ImGui::GetContentRegionAvail().x;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(p.x + 20.0f, p.y),
        ImVec2(p.x + panelW - 20.0f, p.y),
        LPU32(ImVec4(1.0f, 1.0f, 1.0f, 0.04f)), 1.0f
    );
    ImGui::Dummy(ImVec2(panelW, 8.0f));
}

void ControlPanel::RenderMonitorInfo() {
    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    auto& settings = ProyecThor::Settings::SettingsManager::Get().GetSettings();

    float panelW = ImGui::GetContentRegionAvail().x;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::Dummy(ImVec2(panelW, 12.0f));
    ImGui::SetCursorPosX(20.0f);

    if (monitorCount < 2) {
        ImVec2 dotBase = ImGui::GetCursorScreenPos();
        dl->AddCircleFilled(
            ImVec2(dotBase.x + 4.0f, dotBase.y + ImGui::GetTextLineHeight() * 0.5f),
            4.0f, LPU32(ImVec4(1.0f, 0.4f, 0.4f, 1.0f))
        );
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.5f, 0.9f));
        
        // EMPUJAR CURSOR: 16 píxeles a la derecha para librar el punto
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 16.0f);
        ImGui::Text("Equipo sin proyector detectado"); // Ya sin los espacios al inicio
        
        ImGui::PopStyleColor();
    } else {
        int tgt = settings.projection.targetMonitor;
        int sel = std::clamp(tgt < 0 ? 1 : tgt, 0, monitorCount - 1);

        ImVec2 dotBase = ImGui::GetCursorScreenPos();
        ImU32 dotColor = m_isProjecting
            ? LPU32(ImVec4(0.95f, 0.35f, 0.45f, 1.0f))
            : LPU32(ImVec4(0.20f, 0.80f, 0.60f, 1.0f));
        
        // Efecto de brillo en el punto indicador
        dl->AddCircleFilled(ImVec2(dotBase.x + 4.0f, dotBase.y + ImGui::GetTextLineHeight() * 0.5f), 7.0f, 
                            m_isProjecting ? LPU32(ImVec4(0.95f, 0.35f, 0.45f, 0.2f)) : LPU32(ImVec4(0.20f, 0.80f, 0.60f, 0.2f)));
        dl->AddCircleFilled(ImVec2(dotBase.x + 4.0f, dotBase.y + ImGui::GetTextLineHeight() * 0.5f), 4.0f, dotColor);

        std::string monName = glfwGetMonitorName(monitors[sel]);
        if (monName.length() > 20) monName = monName.substr(0, 17) + "...";

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.90f, 1.0f));
        
        // EMPUJAR CURSOR: 16 píxeles a la derecha para librar el halo de 7.0f
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 16.0f);
        ImGui::Text("%s", monName.c_str()); // Texto limpio, sin "  %s"
        
        ImGui::PopStyleColor();

        if (const GLFWvidmode* vm = glfwGetVideoMode(monitors[sel])) {
            char res[32];
            snprintf(res, sizeof(res), "%dx%d", vm->width, vm->height);
            ImVec2 resSz = ImGui::CalcTextSize(res);
            ImVec2 winPos = ImGui::GetWindowPos();
            dl->AddText(
                ImVec2(winPos.x + panelW - resSz.x - 20.0f, dotBase.y),
                LPU32(ImVec4(0.5f, 0.5f, 0.6f, 1.0f)), res
            );
        }
    }

    if (monitorCount >= 2) {
        int tgt = settings.projection.targetMonitor;
        int sel = std::clamp(tgt < 0 ? 1 : tgt, 0, monitorCount - 1);

        ImGui::Dummy(ImVec2(panelW, 6.0f));
        ImGui::SetCursorPosX(20.0f);

        static std::vector<std::string> s_Labels;
        static std::vector<const char*> s_Ptrs;
        s_Labels.clear(); s_Ptrs.clear();
        for (int i = 0; i < monitorCount; i++) {
            std::string lbl = "Pantalla " + std::to_string(i + 1) + ": " + glfwGetMonitorName(monitors[i]);
            s_Labels.push_back(lbl);
        }
        for (const auto& l : s_Labels) s_Ptrs.push_back(l.c_str());

        ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.09f, 0.09f, 0.12f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.12f, 0.12f, 0.16f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_PopupBg,        ImVec4(0.07f, 0.07f, 0.09f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(10.0f, 6.0f));

        float comboW = panelW - 40.0f;
        ImGui::SetNextItemWidth(comboW);
        if (ImGui::Combo("##monsel", &sel, s_Ptrs.data(), (int)s_Ptrs.size())) {
            settings.projection.targetMonitor = sel;
            ProyecThor::Settings::SettingsManager::Get().Save();
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
    }
    ImGui::Dummy(ImVec2(panelW, 10.0f));
}

void ControlPanel::RenderProjectButton(float dt) {
    int monitorCount = 0;
    glfwGetMonitors(&monitorCount);
    bool canProject = (monitorCount >= 2);

    float panelW  = ImGui::GetContentRegionAvail().x;
    
    // 1. Ampliamos el área total para asegurar que nada choque con la barra inferior
    float areaH   = 160.0f; 
    float cursorY = ImGui::GetCursorPosY();
    ImGui::Dummy(ImVec2(panelW, areaH));

    ImVec2 winPos  = ImGui::GetWindowPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float cx = winPos.x + panelW * 0.5f;
    // 2. Desplazamos el centro visual del botón MÁS ARRIBA para dejar espacio abajo
    float cy = winPos.y + cursorY + areaH * 0.5f - 20.0f;

    float pressOffset = m_PressAnim * 3.0f;
    float drawRadius  = kBtnRadius - pressOffset * 0.5f;

    // ── Halos de pulso suaves ────────────────────────────────────────────────
    if (m_isProjecting) {
        for (int i = 0; i < 2; i++) {
            float phase = std::fmod(m_PulseTime + i * 3.14159f, 6.2831853f);
            float t = phase / 6.2831853f;
            // 3. Reducimos la expansión máxima del halo (de 24.0f a 14.0f)
            float haloR = drawRadius + t * 14.0f;
            float haloAlpha = (1.0f - t) * 0.4f;
            dl->AddCircleFilled(ImVec2(cx, cy + pressOffset), haloR,
                LPU32(ImVec4(0.85f, 0.25f, 0.35f, haloAlpha)), 64);
        }
    }

    // ── Sombras del Botón ────────────────────────────────────────────────────
    dl->AddCircleFilled(ImVec2(cx, cy + pressOffset + 6.0f), drawRadius,
        LPU32(ImVec4(0.0f, 0.0f, 0.0f, canProject ? 0.6f : 0.2f)), 64);

    // ── Cuerpo del Botón ─────────────────────────────────────────────────────
    ImVec4 btnBg = canProject ? (m_isProjecting ? ImVec4(0.75f, 0.20f, 0.30f, 1.0f) : ImVec4(0.20f, 0.25f, 0.45f, 1.0f))
                              : ImVec4(0.12f, 0.12f, 0.15f, 1.0f);
    
    dl->AddCircleFilled(ImVec2(cx, cy + pressOffset), drawRadius, LPU32(btnBg), 64);

    // Brillo superior (efecto cristal)
    dl->AddCircleFilled(ImVec2(cx, cy + pressOffset - drawRadius * 0.4f), drawRadius * 0.6f,
        LPU32(ImVec4(1.0f, 1.0f, 1.0f, 0.08f)), 64);

    // ── Ícono Vectorial Preciso ──────────────────────────────────────────────
    ImU32 iconClr = LPU32(canProject ? ImVec4(0.95f, 0.95f, 0.98f, 1.0f) : ImVec4(0.4f, 0.4f, 0.45f, 1.0f));
    float btnCy = cy + pressOffset;

    if (m_isProjecting) {
        float bw = 4.0f, bh = 8.0f, gap = 4.0f; 
        dl->AddRectFilled(ImVec2(cx - gap - bw, btnCy - bh), ImVec2(cx - gap, btnCy + bh), iconClr, 2.0f);
        dl->AddRectFilled(ImVec2(cx + gap, btnCy - bh), ImVec2(cx + gap + bw, btnCy + bh), iconClr, 2.0f);
    } else {
        float r = 9.0f;
        dl->AddTriangleFilled(
            ImVec2(cx - r * 0.4f, btnCy - r),
            ImVec2(cx - r * 0.4f, btnCy + r),
            ImVec2(cx + r * 1.1f, btnCy),
            iconClr
        );
    }

    // ── Etiqueta ─────────────────────────────────────────────────────────────
    const char* lbl = m_isProjecting ? "DETENER" : (canProject ? "INICIAR PROYECCIÓN" : "SIN MONITOR");
    ImVec4 lblCol = canProject ? (m_isProjecting ? ImVec4(0.85f, 0.40f, 0.50f, 1.0f) : ImVec4(0.50f, 0.60f, 0.85f, 1.0f))
                               : ImVec4(0.35f, 0.35f, 0.40f, 1.0f);

    ImVec2 lblSz = ImGui::CalcTextSize(lbl);
    
    // 4. Empujamos la etiqueta a una zona completamente segura (42px desde el borde del botón)
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.9f, 
        ImVec2(cx - (lblSz.x * 0.9f) * 0.5f, cy + pressOffset + drawRadius + 42.0f), 
        LPU32(lblCol), lbl);

    // ── Lógica de Interacción ────────────────────────────────────────────────
    float btnLocalX = panelW * 0.5f - drawRadius;
    float btnLocalY = cursorY + areaH * 0.5f - 20.0f - drawRadius; // Alineado con la nueva posición cy
    ImGui::SetCursorPos(ImVec2(std::max(0.0f, btnLocalX), std::max(0.0f, btnLocalY)));
    ImGui::InvisibleButton("##projbtn", ImVec2(drawRadius * 2.0f, drawRadius * 2.0f));

    if (ImGui::IsItemClicked() && canProject) {
        m_PressAnim = 1.0f;
        m_isProjecting = !m_isProjecting;
        ToggleSecondaryDisplay(m_isProjecting);
        if (!m_isProjecting) {
            Core::PresentationCore::Get().ClearLayer2();
            Core::PresentationCore::Get().StopBackgroundMedia();
        }
    }
    if (ImGui::IsItemHovered() && canProject) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    ImGui::SetCursorPos(ImVec2(0.0f, cursorY + areaH));
    ImGui::Dummy(ImVec2(panelW, 0.0f));
}

void ControlPanel::RenderStatusBar() {
    float panelW = ImGui::GetContentRegionAvail().x;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    float barH = 28.0f;
    ImVec4 barBg = m_isProjecting ? ImVec4(0.15f, 0.05f, 0.08f, 1.0f) : ImVec4(0.07f, 0.07f, 0.09f, 1.0f);

    dl->AddRectFilled(p, ImVec2(p.x + panelW, p.y + barH), LPU32(barBg));

    float dotX = p.x + 20.0f;
    float dotY = p.y + barH * 0.5f;

    if (m_isProjecting) {
        float pulse = std::sin(m_PulseTime * 3.0f) * 0.5f + 0.5f;
        dl->AddCircleFilled(ImVec2(dotX, dotY), 4.5f + pulse * 2.5f, LPU32(ImVec4(0.95f, 0.35f, 0.45f, 0.3f)));
        dl->AddCircleFilled(ImVec2(dotX, dotY), 4.0f, LPU32(ImVec4(0.95f, 0.35f, 0.45f, 1.0f)));
    } else {
        dl->AddCircleFilled(ImVec2(dotX, dotY), 4.0f, LPU32(ImVec4(0.35f, 0.35f, 0.45f, 1.0f)));
    }

    const char* txt = m_isProjecting ? "EN VIVO" : "EN ESPERA";
    ImVec4 txtCol = m_isProjecting ? ImVec4(0.95f, 0.55f, 0.60f, 1.0f) : ImVec4(0.45f, 0.45f, 0.55f, 1.0f);

    dl->AddText(ImVec2(dotX + 14.0f, p.y + (barH - ImGui::GetTextLineHeight()) * 0.5f), LPU32(txtCol), txt);
    ImGui::Dummy(ImVec2(panelW, barH));
}

void ControlPanel::RenderActionRow(float dt) {
    float panelW   = ImGui::GetContentRegionAvail().x;
    ImVec2 winPos  = ImGui::GetWindowPos();
    float rowAreaH = 70.0f;
    float cursorY  = ImGui::GetCursorPosY();

    ImGui::Dummy(ImVec2(panelW, rowAreaH));
    float rowCenterY = winPos.y + cursorY + rowAreaH * 0.5f;
    ImDrawList* dl   = ImGui::GetWindowDrawList();

    float spacing = panelW / 3.0f;
    float centers[3] = { winPos.x + spacing * 0.5f, winPos.x + spacing * 1.5f, winPos.x + spacing * 2.5f };
    float* hovers[3] = { &m_HoverClearText, &m_HoverStopVideo, &m_HoverMonitor };

    struct ActionDef {
        const char* id; const char* label; const char* tooltip; ImVec4 colorHov;
    };
    ActionDef actions[3] = {
        { "##iconcleartext", "Limpiar",   "Quitar texto proyectado", ImVec4(0.20f, 0.25f, 0.45f, 1.0f) },
        { "##iconstopvideo", "Apagar",    "Quitar fondo multimedia", ImVec4(0.35f, 0.15f, 0.18f, 1.0f) },
        { "##iconmonitor",   "Pantalla",  "Propiedades del monitor", ImVec4(0.15f, 0.30f, 0.28f, 1.0f) },
    };

    for (int i = 0; i < 3; i++) {
        bool clicked = RenderIconButton(
            actions[i].id, centers[i], rowCenterY - 8.0f, kIconRadius, *hovers[i],
            ImVec4(0.10f, 0.10f, 0.13f, 1.0f), actions[i].colorHov, dt
        );

        float hv = *hovers[i];
        float bx = centers[i], by = rowCenterY - 8.0f;
        ImU32 iconCol = LPU32(ImVec4(0.6f + hv*0.4f, 0.6f + hv*0.4f, 0.7f + hv*0.3f, 1.0f));

        // Dibujo de Íconos Geométricos Mejorados
        if (i == 0) {
            // Icono Limpiar: Documento con una X
            float w = 7.0f, h = 9.0f;
            dl->AddRect(ImVec2(bx - w, by - h), ImVec2(bx + w, by + h), iconCol, 2.0f, 0, 1.5f);
            dl->AddLine(ImVec2(bx - 3.0f, by - 3.0f), ImVec2(bx + 3.0f, by + 3.0f), iconCol, 1.5f);
            dl->AddLine(ImVec2(bx + 3.0f, by - 3.0f), ImVec2(bx - 3.0f, by + 3.0f), iconCol, 1.5f);
        } else if (i == 1) {
            // Icono Apagar/Fondo: Cuadrado Stop redondeado
            dl->AddRectFilled(ImVec2(bx - 5.0f, by - 5.0f), ImVec2(bx + 5.0f, by + 5.0f), iconCol, 2.0f);
        } else {
            // Icono Pantalla: Monitor moderno
            float mw = 9.0f, mh = 6.0f;
            dl->AddRect(ImVec2(bx - mw, by - mh), ImVec2(bx + mw, by + mh), iconCol, 2.0f, 0, 1.5f);
            dl->AddLine(ImVec2(bx - 4.0f, by + mh + 4.0f), ImVec2(bx + 4.0f, by + mh + 4.0f), iconCol, 1.5f);
            dl->AddLine(ImVec2(bx, by + mh), ImVec2(bx, by + mh + 4.0f), iconCol, 1.5f);
        }

        ImVec2 lblSz = ImGui::CalcTextSize(actions[i].label);
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.85f, 
            ImVec2(bx - lblSz.x * 0.42f, rowCenterY + kIconRadius), LPU32(ImVec4(0.5f, 0.5f, 0.6f, 1.0f)), actions[i].label);

        if (clicked) {
            if (i == 0) Core::PresentationCore::Get().ClearLayer2();
            if (i == 1) Core::PresentationCore::Get().StopBackgroundMedia();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", actions[i].tooltip);
    }
}

bool ControlPanel::RenderIconButton(const char* id, float cx, float cy, float radius,
                                    float& hoverAnim, ImVec4 colorBase, ImVec4 colorHover, float dt) {
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2 winPos   = ImGui::GetWindowPos();

    float localX = std::max(0.0f, cx - winPos.x - radius);
    float localY = std::max(0.0f, cy - winPos.y - radius);

    ImGui::SetCursorPos(ImVec2(localX, localY));
    ImGui::InvisibleButton(id, ImVec2(radius * 2.0f, radius * 2.0f));

    bool hov     = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();

    hoverAnim += ((hov ? 1.0f : 0.0f) - hoverAnim) * std::min(1.0f, dt * kHoverAnimSpeed);

    ImVec4 cLerp = ImVec4(
        colorBase.x + (colorHover.x - colorBase.x) * hoverAnim,
        colorBase.y + (colorHover.y - colorBase.y) * hoverAnim,
        colorBase.z + (colorHover.z - colorBase.z) * hoverAnim,
        1.0f
    );

    float r = radius + hoverAnim * 1.5f;

    dl->AddCircleFilled(ImVec2(cx, cy + 3.0f), r, LPU32(ImVec4(0.0f, 0.0f, 0.0f, 0.4f)), 32);
    dl->AddCircleFilled(ImVec2(cx, cy), r, LPU32(cLerp), 32);

    if (hov) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    return clicked;
}

void ControlPanel::ToggleSecondaryDisplay(bool active) {
    auto& core = Core::PresentationCore::Get();
    core.SetProjecting(active);
    if (active) {
        core.CreateProjectorWindow();
        std::cout << "[ControlPanel] Proyeccion iniciada.\n";
    } else {
        core.DestroyProjectorWindow();
        std::cout << "[ControlPanel] Proyeccion detenida.\n";
    }
}

} // namespace ProyecThor::UI