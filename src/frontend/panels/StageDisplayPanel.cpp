#include "StageDisplayPanel.h"
#include "ControlWidgets.h"
#include "DesignSystem.h"
#include "backend/settings/SettingsManager.h"
#include "backend/settings/StageLayoutTemplates.h"
#include "backend/core/PresentationCore.h"
#include "stb_image_write.h"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <string>
#include <vector>
#include <cstring>
#include <iostream>

namespace ProyecThor::UI {

using namespace ProyecThor::Settings;

static constexpr ImVec4 kAccent    = { 0.30f, 0.55f, 0.95f, 1.0f };
static constexpr ImVec4 kSurface   = { 0.10f, 0.11f, 0.15f, 1.0f };
static constexpr ImVec4 kSurface2  = { 0.15f, 0.16f, 0.21f, 1.0f };
static constexpr ImVec4 kGrayText  = { 0.65f, 0.68f, 0.75f, 1.0f };

static ImU32 Col(ImVec4 v) { return ImGui::ColorConvertFloat4ToU32(v); }

// Boton simple de plantilla (mismo espiritu que el selector de calidad de
// CategoryProjection.cpp, duplicado liviano ya que viven en modulos
// distintos y el widget es de solo 6 lineas).
static bool TemplateButton(const char* id, const char* label, bool active, float width) {
    ImVec4 base   = active ? ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.35f) : ImVec4(1,1,1,0.05f);
    ImVec4 hover  = active ? ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.45f) : ImVec4(1,1,1,0.10f);
    ImVec4 border = active ? kAccent : ImVec4(1,1,1,0.12f);

    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_Button, base);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, hover);
    ImGui::PushStyleColor(ImGuiCol_Border, border);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, active ? 2.0f : 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

    bool clicked = ImGui::Button(label, ImVec2(width, 36.0f));

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    ImGui::PopID();
    return clicked;
}

void StageDisplayPanel::RenderContent() {
    ImGui::TextDisabled("Configura que ve el equipo en el escenario a traves del Monitor de Control.");
    ImGui::Spacing();

    RenderActivationCard();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    RenderTemplateSelector();
    ImGui::Spacing();
    RenderCellPreview();
    ImGui::Spacing();
    RenderCellAssignments();
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderActivationCard — encender/apagar el monitor de control y elegir a
//  que pantalla (o LAN) se sirve. Vivia en ControlPanel ("MONITOR DE
//  CONTROL"); se movio aca porque es un asunto de Stage, no de Control.
// ─────────────────────────────────────────────────────────────────────────────
void StageDisplayPanel::RenderActivationCard() {
    auto& core = Core::PresentationCore::Get();

    int monitorCount = 0;
    glfwGetMonitors(&monitorCount);
    const bool hasPhysicalOption = monitorCount >= 2;
    const int  currentAppMonitor = DetectCurrentMonitorIndex();

    // Sin segunda pantalla fisica, LAN es la unica opcion posible para el
    // monitor de control — no tiene sentido dejarlo "apagado" por eleccion.
    if (!hasPhysicalOption) m_StageUseLAN = true;
    if (hasPhysicalOption)  m_StageMonitorIndex = std::clamp(m_StageMonitorIndex, 0, monitorCount - 1);

    // El estado real de "activo" se consulta a la fuente correspondiente en
    // vez de fiarse solo del booleano local: si el usuario tambien controla
    // la transmision LAN desde el panel "Transmisión en Red", este panel debe
    // reflejar eso igual (evita que ambos paneles queden desincronizados).
    const bool stageActive = m_StageUseLAN ? core.IsStreamingNet() : core.IsStaging();

    const int  lanItemIndex = hasPhysicalOption ? monitorCount : 0;
    const int  sel          = m_StageUseLAN ? lanItemIndex : m_StageMonitorIndex;
    const bool showLanPanel = m_StageUseLAN && stageActive;

    BeginCard("StageActivationCard", showLanPanel ? 226.0f : (hasPhysicalOption ? 190.0f : 168.0f));

    ImGui::TextUnformatted(stageActive ? "Stage activo" : "Stage inactivo");
    ImGui::TextDisabled("Monitor de confianza: ");
    ImGui::SameLine();
    if (m_StageUseLAN)
        ImGui::TextColored(ToVec4(DS::TextPrimary), "Red (LAN)");
    else
        ImGui::TextColored(ToVec4(DS::TextPrimary), "Pantalla %d", m_StageMonitorIndex + 1);
    ImGui::Spacing();

    if (!hasPhysicalOption) {
        ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextSecondary));
        ImGui::TextWrapped(
            "No se detectó una segunda pantalla física: el monitor de control "
            "estará disponible solo por LAN. Cualquier celular o tablet en la "
            "misma red WiFi podrá verlo desde su navegador.");
        ImGui::PopStyleColor();
    } else {
        MonitorSelector("stage", sel, -1, /*includeLAN*/true, currentAppMonitor,
            [this](int dir) { CycleStageMonitor(dir); },
            [this, stageActive](int newSel) {
                int mc = 0;
                glfwGetMonitors(&mc);
                const bool wantLAN = (newSel >= mc);
                auto& core = Core::PresentationCore::Get();

                if (stageActive) {
                    if (m_StageUseLAN) core.ToggleNetworkStream(false);
                    else               core.SetStaging(false);
                }

                m_StageUseLAN = wantLAN;
                if (!wantLAN) m_StageMonitorIndex = newSel;

                if (stageActive) {
                    if (wantLAN) core.ToggleNetworkStream(true, m_LANPort);
                    else         core.SetStaging(true, m_StageMonitorIndex);
                }
            });

        ImGui::Spacing();
        ImGui::TextDisabled("Muestra el contenido en vivo a un segundo público (músicos, camarógrafos, etc).");
    }
    ImGui::Spacing();

    if (showLanPanel) {
        auto state = core.GetState();

        ImGui::PushStyleColor(ImGuiCol_FrameBg, ToVec4(DS::BtnDefaultFill));
        ImGui::PushStyleColor(ImGuiCol_Border,  ToVec4(DS::GlassBorder));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);

        char urlBuf[256];
        std::strncpy(urlBuf, state.networkURL.c_str(), sizeof(urlBuf) - 1);
        urlBuf[sizeof(urlBuf) - 1] = '\0';

        const float btnW   = 90.0f;
        const float gap    = 8.0f;
        const float fieldW = ImGui::GetContentRegionAvail().x - btnW - gap;

        ImGui::SetNextItemWidth(fieldW);
        ImGui::InputText("##lanurl", urlBuf, sizeof(urlBuf), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);

        ImGui::SameLine(0.0f, gap);
        if (ImGui::Button("Copiar Link", ImVec2(btnW, 0.0f)))
            ImGui::SetClipboardText(state.networkURL.c_str());

        ImGui::TextDisabled("Más opciones de calidad/resolución en el panel \"Transmisión en Red\".");
        ImGui::Spacing();
    }

    const char* buttonText = stageActive ? "DETENER STAGE" : "ACTIVAR STAGE";
    ImVec4 btnColor    = stageActive ? ToVec4(ColA(DS::AccentColor, 217)) : ToVec4(DS::BtnDefaultFill);
    ImVec4 hoverColor  = Brighten(btnColor, 0.06f);
    ImVec4 activeColor = Brighten(btnColor, -0.06f);
    ImVec4 tintCol     = stageActive ? ToVec4(DS::TextPrimary) : ToVec4(DS::TextHint);
    DrawIconFn icon    = m_StageUseLAN ? ControlIcons::DrawBroadcast : ControlIcons::DrawStageMonitor;

    if (IconLabelButton("stageToggle", buttonText, icon, ImVec2(-1.0f, 44.0f),
                        btnColor, hoverColor, activeColor, tintCol)) {
        ToggleStageDisplay(!stageActive);
    }

    EndCard();

    // Si el monitor de control se sirve por LAN, este panel es responsable de
    // alimentar el servidor con frames de video (igual que hace StreamingPanel
    // cuando esta abierto), para que el mirror funcione aunque el usuario nunca
    // haya abierto el panel "Transmisión en Red".
    if (showLanPanel) {
        double now = ImGui::GetTime();
        if (now - m_LANLastCaptureTime >= (1.0 / 8.0)) {
            m_LANLastCaptureTime = now;
            CaptureAndPushLANFrame(1280, 720, 80);
        }
    }
}

void StageDisplayPanel::CycleStageMonitor(int direction) {
    int monitorCount = 0;
    glfwGetMonitors(&monitorCount);
    const bool hasPhysicalOption = monitorCount >= 2;

    const int totalItems = (hasPhysicalOption ? monitorCount : 0) + 1;
    if (totalItems <= 1)
        return; // sin pantallas fisicas, LAN es la unica opcion: nada que ciclar

    const int lanItemIndex = totalItems - 1;
    const int current = m_StageUseLAN ? lanItemIndex : std::clamp(m_StageMonitorIndex, 0, monitorCount - 1);
    const int next     = (current + direction + totalItems) % totalItems;
    const bool wantLAN = (next == lanItemIndex);

    auto& core = Core::PresentationCore::Get();
    const bool stageActive = m_StageUseLAN ? core.IsStreamingNet() : core.IsStaging();

    if (stageActive) {
        if (m_StageUseLAN) core.ToggleNetworkStream(false);
        else                core.SetStaging(false);
    }

    m_StageUseLAN = wantLAN;
    if (!wantLAN) m_StageMonitorIndex = next;

    if (stageActive) {
        if (wantLAN) core.ToggleNetworkStream(true, m_LANPort);
        else          core.SetStaging(true, m_StageMonitorIndex);
    }
}

void StageDisplayPanel::ToggleStageDisplay(bool active) {
    auto& core = Core::PresentationCore::Get();
    if (active) {
        if (m_StageUseLAN) {
            core.ToggleNetworkStream(true, m_LANPort);
            if (core.IsStreamingNet())
                std::cout << "[StageDisplayPanel] Monitor de control (LAN) iniciado en puerto " << m_LANPort << ".\n";
            else
                std::cerr << "[StageDisplayPanel] No se pudo iniciar el monitor de control por LAN (puerto "
                          << m_LANPort << " en uso?).\n";
            return;
        }

        int monitorCount = 0;
        glfwGetMonitors(&monitorCount);
        if (monitorCount < 2) {
            std::cerr << "[StageDisplayPanel] No hay suficientes monitores para activar el stage.\n";
            return;
        }

        m_StageMonitorIndex = std::clamp(m_StageMonitorIndex, 0, monitorCount - 1);
        core.SetStaging(true, m_StageMonitorIndex);
        std::cout << "[StageDisplayPanel] Monitor de control iniciado en monitor " << m_StageMonitorIndex << ".\n";
    } else {
        if (m_StageUseLAN) {
            core.ToggleNetworkStream(false);
            std::cout << "[StageDisplayPanel] Monitor de control (LAN) detenido.\n";
        } else {
            core.SetStaging(false);
            std::cout << "[StageDisplayPanel] Monitor de control detenido.\n";
        }
    }
}

void StageDisplayPanel::CaptureAndPushLANFrame(int w, int h, int quality) {
    if (w <= 0 || h <= 0) return;

    auto& core = Core::PresentationCore::Get();
    std::vector<uint8_t> rgb;
    if (!core.RenderProjectorToFBO(w, h, rgb)) return;

    std::vector<uint8_t> jpeg;
    jpeg.reserve(static_cast<size_t>(w) * h / 4);

    auto stbCb = [](void* ctx, void* data, int size) {
        auto* buf = static_cast<std::vector<uint8_t>*>(ctx);
        const uint8_t* p = static_cast<const uint8_t*>(data);
        buf->insert(buf->end(), p, p + size);
    };
    stbi_write_jpg_to_func(stbCb, &jpeg, w, h, 3, rgb.data(), quality);

    core.PushFrame(std::move(jpeg));
}

void StageDisplayPanel::RenderTemplateSelector() {
    auto& sd = ProyecThor::Settings::SettingsManager::Get().GetSettings().stageDisplay;

    ImGui::TextUnformatted("Distribucion");
    ImGui::Spacing();

    float w = ImGui::GetContentRegionAvail().x;
    float gap = 6.0f;
    float btnW = (w - gap * (kStageLayoutTemplateCount - 1)) / kStageLayoutTemplateCount;

    for (int i = 0; i < kStageLayoutTemplateCount; i++) {
        if (i > 0) ImGui::SameLine(0.0f, gap);
        std::string id = "tmpl" + std::to_string(i);
        if (TemplateButton(id.c_str(), kStageLayoutTemplates[i].label,
                            sd.layoutTemplateIndex == i, btnW)) {
            sd.layoutTemplateIndex = i;
            ProyecThor::Settings::SettingsManager::Get().Save();
        }
    }
}

void StageDisplayPanel::RenderCellPreview() {
    auto& sd = ProyecThor::Settings::SettingsManager::Get().GetSettings().stageDisplay;
    int idx = std::clamp(sd.layoutTemplateIndex, 0, kStageLayoutTemplateCount - 1);
    const auto& tmpl = kStageLayoutTemplates[idx];

    float w = ImGui::GetContentRegionAvail().x;
    float h = w * 9.0f / 16.0f; // vista previa en proporcion 16:9

    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1 = ImVec2(p0.x + w, p0.y + h);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(p0, p1, Col(kSurface), 8.0f);
    dl->AddRect(p0, p1, Col(ImVec4(1,1,1,0.15f)), 8.0f);

    for (int i = 0; i < tmpl.cellCount; i++) {
        const float* r = tmpl.rect[i];
        ImVec2 c0 = ImVec2(p0.x + r[0] * w, p0.y + r[1] * h);
        ImVec2 c1 = ImVec2(c0.x + r[2] * w, c0.y + r[3] * h);

        dl->AddRectFilled(ImVec2(c0.x + 2, c0.y + 2), ImVec2(c1.x - 2, c1.y - 2), Col(kSurface2), 6.0f);
        dl->AddRect(ImVec2(c0.x + 2, c0.y + 2), ImVec2(c1.x - 2, c1.y - 2), Col(ImVec4(1,1,1,0.10f)), 6.0f);

        auto widget = static_cast<StageWidgetType>(sd.cellWidget[i]);
        const char* label = StageWidgetTypeName(widget);
        ImVec2 ts = ImGui::CalcTextSize(label);
        ImVec2 center = ImVec2((c0.x + c1.x) * 0.5f - ts.x * 0.5f, (c0.y + c1.y) * 0.5f - ts.y * 0.5f);
        dl->AddText(center, Col(kGrayText), label);
    }

    ImGui::Dummy(ImVec2(w, h));
}

void StageDisplayPanel::RenderCellAssignments() {
    auto& sd = ProyecThor::Settings::SettingsManager::Get().GetSettings().stageDisplay;
    int idx = std::clamp(sd.layoutTemplateIndex, 0, kStageLayoutTemplateCount - 1);
    const auto& tmpl = kStageLayoutTemplates[idx];

    ImGui::TextUnformatted("Contenido por celda");
    ImGui::Spacing();

    static const char* kWidgetNames[] = { "Vacio", "Reloj", "Texto en vivo", "Proxima linea" };

    for (int i = 0; i < tmpl.cellCount; i++) {
        std::string label = "Celda " + std::to_string(i + 1);
        int current = std::clamp(sd.cellWidget[i], 0, 3);

        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::Combo(label.c_str(), &current, kWidgetNames, 4)) {
            sd.cellWidget[i] = current;
            ProyecThor::Settings::SettingsManager::Get().Save();
        }
    }
}

} // namespace ProyecThor::UI
