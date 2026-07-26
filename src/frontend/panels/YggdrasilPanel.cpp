#include <GL/glew.h>
#include "YggdrasilPanel.h"
#include "backend/core/PresentationCore.h"
#include "backend/core/OSCSender.h"
#include "backend/settings/SettingsManager.h"
#include "AppIcons.h"
#include "IconRail.h"
#include "home/HomeIcons.h"
#include <imgui.h>
#include <algorithm>
#include <chrono>
#include <ctime>

namespace ProyecThor::UI {

// ── InputText atado a std::string ───────────────────────────────────────────
// Misma tecnica que SongEditView.cpp::InputTextStd (imgui_stdlib.h no esta
// vendorizado en este proyecto): via ImGuiInputTextFlags_CallbackResize en
// vez de un buffer char[] fijo.
namespace {
struct StdStringCbData { std::string* str; };

int StdStringResizeCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto* cb = static_cast<StdStringCbData*>(data->UserData);
        std::string* str = cb->str;
        IM_ASSERT(data->Buf == str->c_str());
        str->resize(data->BufTextLen);
        data->Buf = str->data();
    }
    return 0;
}

bool InputTextStd(const char* label, std::string* str, ImGuiInputTextFlags flags = 0) {
    flags |= ImGuiInputTextFlags_CallbackResize;
    StdStringCbData cb{ str };
    return ImGui::InputText(label, str->data(), str->capacity() + 1, flags, StdStringResizeCallback, &cb);
}
} // namespace

YggdrasilPanel::YggdrasilPanel() {
    BuildParamRegistry();

    auto& y = ProyecThor::Settings::SettingsManager::Get().GetSettings().yggdrasil;
    if (y.autoListen) {
        std::string err;
        if (m_Receiver.Start(y.listenPort, &err))
            m_ListenStatus = "Escuchando en el puerto " + std::to_string(y.listenPort);
        else
            m_ListenStatus = "Error al escuchar: " + err;
    }
}

YggdrasilPanel::~YggdrasilPanel() {
    m_Receiver.Stop();
}

// Registro de parametros en vivo que se pueden vincular a OSC. Por ahora
// cubre los efectos de Shaders (todos exponen Get/SetXxxIntensity o
// Get/SetXxxAmount en 0..1 sobre PresentationCore, ver ShadersPanel.cpp
// para el mismo patron usado por los sliders manuales). Overlays no tiene
// todavia parametros de opacidad/escala en vivo expuestos a nivel de
// PresentationCore -- sumarlos es un paso aparte (requiere tocar el
// compositor de overlays), no algo que se pueda enganchar aca sin riesgo.
void YggdrasilPanel::BuildParamRegistry() {
    using Core::PresentationCore;

    m_Params = {
        { "Shaders > CRT (scanlines)",
          []{ return PresentationCore::Get().GetCRTScanlineIntensity(); },
          [](float v){ PresentationCore::Get().SetCRTScanlineIntensity(v); } },

        { "Shaders > Grano de pelicula",
          []{ return PresentationCore::Get().GetGrainIntensity(); },
          [](float v){ PresentationCore::Get().SetGrainIntensity(v); } },

        { "Shaders > Saturacion (color)",
          []{ return PresentationCore::Get().GetSaturationAmount(); },
          [](float v){ PresentationCore::Get().SetSaturationAmount(v); } },

        { "Shaders > Vinetado",
          []{ return PresentationCore::Get().GetVignetteIntensity(); },
          [](float v){ PresentationCore::Get().SetVignetteIntensity(v); } },

        { "Shaders > Desenfoque (Blur)",
          []{ return PresentationCore::Get().GetBlurIntensity(); },
          [](float v){ PresentationCore::Get().SetBlurIntensity(v); } },

        { "Shaders > Nitidez (Sharpen)",
          []{ return PresentationCore::Get().GetSharpenIntensity(); },
          [](float v){ PresentationCore::Get().SetSharpenIntensity(v); } },

        { "Shaders > Resplandor (Bloom)",
          []{ return PresentationCore::Get().GetBloomIntensity(); },
          [](float v){ PresentationCore::Get().SetBloomIntensity(v); } },

        { "Shaders > Aberracion cromatica",
          []{ return PresentationCore::Get().GetChromaticAberrationIntensity(); },
          [](float v){ PresentationCore::Get().SetChromaticAberrationIntensity(v); } },

        { "Shaders > VHS",
          []{ return PresentationCore::Get().GetVHSIntensity(); },
          [](float v){ PresentationCore::Get().SetVHSIntensity(v); } },

        { "Shaders > Cine",
          []{ return PresentationCore::Get().GetCineIntensity(); },
          [](float v){ PresentationCore::Get().SetCineIntensity(v); } },

        { "Shaders > Contraste",
          []{ return PresentationCore::Get().GetContrastAmount(); },
          [](float v){ PresentationCore::Get().SetContrastAmount(v); } },

        { "Shaders > Luminosidad",
          []{ return PresentationCore::Get().GetLuminosityAmount(); },
          [](float v){ PresentationCore::Get().SetLuminosityAmount(v); } },

        { "Shaders > TAA (antialiasing)",
          []{ return PresentationCore::Get().GetTAAIntensity(); },
          [](float v){ PresentationCore::Get().SetTAAIntensity(v); } },
    };
}

// Saca los mensajes acumulados del receptor y los aplica: en modo
// "Aprender" (m_LearningIndex >= 0), el PRIMER mensaje que llega se
// convierte en el binding de ese parametro; si no, cualquier mensaje cuya
// direccion coincida con un binding existente actualiza el parametro en
// vivo (primer argumento numerico, clamp 0..1 -- mismo rango que usan los
// sliders manuales de Shaders).
void YggdrasilPanel::ApplyReceivedMessages() {
    if (!m_Receiver.IsListening()) return;

    m_DrainBuffer.clear();
    m_Receiver.DrainMessages(m_DrainBuffer);
    if (m_DrainBuffer.empty()) return;

    auto& bindings = ProyecThor::Settings::SettingsManager::Get().GetSettings().yggdrasil.bindings;

    for (const auto& msg : m_DrainBuffer) {
        if (m_LearningIndex >= 0 && m_LearningIndex < (int)m_Params.size()) {
            const std::string& paramName = m_Params[m_LearningIndex].name;

            auto it = std::find_if(bindings.begin(), bindings.end(),
                [&](const ProyecThor::Settings::OSCBinding& b){ return b.paramName == paramName; });
            if (it != bindings.end()) it->oscAddress = msg.address;
            else                      bindings.push_back({ paramName, msg.address });

            m_LearningIndex = -1;
            continue; // el mismo mensaje que se uso para aprender no dispara el valor
        }

        auto it = std::find_if(bindings.begin(), bindings.end(),
            [&](const ProyecThor::Settings::OSCBinding& b){ return b.oscAddress == msg.address; });
        if (it == bindings.end() || msg.args.empty()) continue;

        float value = 0.0f;
        bool  hasValue = true;
        switch (msg.args[0].type) {
            case Core::OSCArg::Type::Float:  value = msg.args[0].floatVal;             break;
            case Core::OSCArg::Type::Int:    value = (float)msg.args[0].intVal;        break;
            default:                         hasValue = false;                        break;
        }
        if (!hasValue) continue;
        value = std::clamp(value, 0.0f, 1.0f);

        auto pit = std::find_if(m_Params.begin(), m_Params.end(),
            [&](const BindableParam& p){ return p.name == it->paramName; });
        if (pit != m_Params.end()) pit->set(value);
    }
}

void YggdrasilPanel::RenderConnectionSection() {
    auto& y = ProyecThor::Settings::SettingsManager::Get().GetSettings().yggdrasil;

    ImGui::SeparatorText("Conexion");
    ImGui::TextWrapped("Direccion a la que se envian los mensajes (luces). No hace falta "
                        "para recibir/Aprender, eso usa el puerto de escucha de abajo.");

    ImGui::SetNextItemWidth(160.0f);
    InputTextStd("IP de destino", &y.targetIp);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    ImGui::InputInt("Puerto de destino", &y.targetPort, 0);
    y.targetPort = std::clamp(y.targetPort, 1, 65535);

    ImGui::Spacing();
    ImGui::SetNextItemWidth(90.0f);
    ImGui::InputInt("Puerto de escucha (recibir)", &y.listenPort, 0);
    y.listenPort = std::clamp(y.listenPort, 1, 65535);

    ImGui::SameLine();
    bool listening = m_Receiver.IsListening();
    if (listening) {
        if (ImGui::Button("Detener escucha")) {
            m_Receiver.Stop();
            m_ListenStatus = "Detenido.";
        }
    } else {
        if (ImGui::Button("Iniciar escucha")) {
            std::string err;
            if (m_Receiver.Start(y.listenPort, &err))
                m_ListenStatus = "Escuchando en el puerto " + std::to_string(y.listenPort);
            else
                m_ListenStatus = "Error al escuchar: " + err;
        }
    }

    ImGui::Checkbox("Escuchar automaticamente al abrir ProyecThor", &y.autoListen);

    ImVec4 statusCol = listening ? ImVec4(0.35f, 0.85f, 0.55f, 1.0f) : ImVec4(0.60f, 0.62f, 0.70f, 1.0f);
    ImGui::TextColored(statusCol, "%s", m_ListenStatus.empty() ? "Sin iniciar." : m_ListenStatus.c_str());
    ImGui::Spacing();
}

void YggdrasilPanel::RenderControlListSection() {
    auto& bindings = ProyecThor::Settings::SettingsManager::Get().GetSettings().yggdrasil.bindings;

    ImGui::SeparatorText("Control List (recibir + OSC Learn)");
    ImGui::TextWrapped("Vincula un parametro en vivo de ProyecThor a un mensaje OSC entrante: "
                        "apreta \"Aprender\", mové el fader/control externo, y queda vinculado.");
    ImGui::Spacing();

    if (!ImGui::BeginTable("##yggControlList", 4,
                            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
        return;

    ImGui::TableSetupColumn("Parametro",     ImGuiTableColumnFlags_WidthStretch, 0.34f);
    ImGui::TableSetupColumn("Valor",         ImGuiTableColumnFlags_WidthStretch, 0.20f);
    ImGui::TableSetupColumn("Direccion OSC", ImGuiTableColumnFlags_WidthStretch, 0.26f);
    ImGui::TableSetupColumn("Accion",        ImGuiTableColumnFlags_WidthStretch, 0.20f);
    ImGui::TableHeadersRow();

    for (int i = 0; i < (int)m_Params.size(); i++) {
        const auto& p = m_Params[i];
        ImGui::PushID(i);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(p.name.c_str());

        ImGui::TableSetColumnIndex(1);
        float value = p.get();
        ImGui::ProgressBar(value, ImVec2(-FLT_MIN, 0.0f));

        ImGui::TableSetColumnIndex(2);
        auto it = std::find_if(bindings.begin(), bindings.end(),
            [&](const ProyecThor::Settings::OSCBinding& b){ return b.paramName == p.name; });
        if (it != bindings.end()) ImGui::TextUnformatted(it->oscAddress.c_str());
        else                      ImGui::TextDisabled("Sin vincular");

        ImGui::TableSetColumnIndex(3);
        if (m_LearningIndex == i) {
            if (ImGui::Button("Cancelar (esperando...)")) m_LearningIndex = -1;
        } else {
            if (ImGui::Button("Aprender")) {
                m_LearningIndex = i;
                if (!m_Receiver.IsListening()) {
                    auto& y = ProyecThor::Settings::SettingsManager::Get().GetSettings().yggdrasil;
                    std::string err;
                    if (m_Receiver.Start(y.listenPort, &err))
                        m_ListenStatus = "Escuchando en el puerto " + std::to_string(y.listenPort);
                    else
                        m_ListenStatus = "Error al escuchar: " + err;
                }
            }
            if (it != bindings.end()) {
                ImGui::SameLine();
                if (ImGui::Button("Quitar")) bindings.erase(it);
            }
        }

        ImGui::PopID();
    }

    ImGui::EndTable();
    ImGui::Spacing();
}

void YggdrasilPanel::RenderSendSection() {
    auto& messages = ProyecThor::Settings::SettingsManager::Get().GetSettings().yggdrasil.messages;

    ImGui::SeparatorText("Luces (enviar)");
    ImGui::TextWrapped("Cada fila es una luz/cue disparable a mano. El punto de color muestra "
                        "si el ultimo envio a esa luz funciono.");
    ImGui::Spacing();

    if (ImGui::Button("+ Agregar luz")) {
        ProyecThor::Settings::OSCMessageDef m;
        m.label = "Luz " + std::to_string(messages.size() + 1);
        messages.push_back(m);
    }
    ImGui::Spacing();

    for (int i = 0; i < (int)messages.size(); i++) {
        auto& m = messages[i];
        ImGui::PushID(i + 1000);
        ImGui::BeginGroup();

        // Punto de estado: gris = nunca probado, verde = ultimo envio OK,
        // rojo = fallo -- para "trackear" de un vistazo cual luz es cual.
        ImVec4 dotCol = ImVec4(0.45f, 0.47f, 0.55f, 1.0f);
        if (!m.lastSentAt.empty())
            dotCol = m.lastSendOk ? ImVec4(0.30f, 0.86f, 0.48f, 1.0f) : ImVec4(0.90f, 0.30f, 0.30f, 1.0f);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 dotPos = ImGui::GetCursorScreenPos();
        dl->AddCircleFilled(ImVec2(dotPos.x + 6.0f, dotPos.y + 10.0f), 5.0f, ImGui::ColorConvertFloat4ToU32(dotCol));
        ImGui::Dummy(ImVec2(16.0f, 1.0f));
        ImGui::SameLine();

        ImGui::SetNextItemWidth(140.0f);
        InputTextStd("##label", &m.label);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.0f);
        InputTextStd("Direccion##addr", &m.address);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200.0f);
        InputTextStd("Argumentos##args", &m.argsText);
        ImGui::SameLine();

        if (ImGui::Button("Enviar")) {
            auto& y = ProyecThor::Settings::SettingsManager::Get().GetSettings().yggdrasil;
            std::string err;
            bool ok = Core::SendOSCMessage(y.targetIp, y.targetPort, m.address,
                                            Core::ParseOSCArgs(m.argsText), &err);
            m.lastSendOk = ok;

            auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            char timeBuf[16];
            std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", std::localtime(&now));
            m.lastSentAt = timeBuf;
        }
        ImGui::SameLine();
        if (ImGui::Button("Borrar")) {
            messages.erase(messages.begin() + i);
            ImGui::EndGroup();
            ImGui::PopID();
            break; // el vector se resizeo -- no seguir iterando este frame
        }

        if (!m.lastSentAt.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("%s a las %s", m.lastSendOk ? "OK" : "Fallo", m.lastSentAt.c_str());
        }

        ImGui::EndGroup();
        ImGui::PopID();
    }
}

void YggdrasilPanel::RenderRail() {
    static const IconRailItem kItems[] = {
        { (int)Section::OSC,  AppIcons::DrawIcon_Yggdrasil,  "OSC"  },
        { (int)Section::Red,     HomeIcons::DrawIcon_Broadcast, "Red"     },
        { (int)Section::Chat,    HomeIcons::DrawIcon_Chat,      "Chat"    },
        { (int)Section::Capture, HomeIcons::DrawIcon_Camera,    "Capture" },
        { (int)Section::Layer,   AppIcons::DrawIcon_Layers,     "Layer"   },
        { (int)Section::Start,   AppIcons::DrawIcon_Monitor,    "Iniciar" },
    };
    static const float kColors[6][4] = {
        { 0.65f, 0.31f, 0.94f, 1.0f }, // OSC
        { 0.30f, 0.80f, 0.85f, 1.0f }, // Red
        { 0.75f, 0.40f, 0.90f, 1.0f }, // Chat
        { 0.90f, 0.35f, 0.45f, 1.0f }, // Capture
        { 0.35f, 0.80f, 0.55f, 1.0f }, // Layer
        { 0.90f, 0.28f, 0.28f, 1.0f }, // Iniciar
    };

    float railW = IconRailThickness(true);
    ImGui::BeginChild("##yggdrasilRail", ImVec2(railW, 0.0f), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    int currentIndex = (int)m_Section;
    RenderIconRail(kItems, 6, currentIndex, IconRailOrientation::Vertical, kColors);
    m_Section = (Section)currentIndex;

    ImGui::EndChild();
}

void YggdrasilPanel::RenderOSCSection() {
    RenderConnectionSection();
    RenderControlListSection();
    RenderSendSection();
}

void YggdrasilPanel::Render() {
    ApplyReceivedMessages();
    // Red/Chat/Streaming ya no se actualizan aca: UIManager les llama
    // Update() de forma incondicional en cada frame (ver UIManager.h,
    // GetRedPanel/GetChatPanel/GetBroadcastPanel), sin importar el
    // WorkspaceMode activo.

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove       |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("##YggdrasilRoot", nullptr, flags);

    ImGui::BeginChild("##yggdrasilContent", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AlwaysUseWindowPadding);

    {
        float iconSz = 30.0f;
        ImVec2 iconOrigin = ImGui::GetCursorScreenPos();
        AppIcons::DrawIcon_Yggdrasil(ImGui::GetWindowDrawList(), iconOrigin, iconSz,
                                      ImGui::GetColorU32(ImGuiCol_Text));
        ImGui::Dummy(ImVec2(iconSz + 8.0f, iconSz));
        ImGui::SameLine();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (iconSz - ImGui::GetTextLineHeight()) * 0.5f);
        ImGui::TextUnformatted("Yggdrasil");
        ImGui::SameLine();
        ImGui::TextDisabled("(OSC, Red, Chat y Streaming)");
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    RenderRail();
    ImGui::SameLine();
    ImGui::BeginChild("##yggdrasilSection", ImVec2(0.0f, 0.0f));

    switch (m_Section) {
        case Section::OSC:     RenderOSCSection();                                          break;
        case Section::Red:     if (m_Red)       m_Red->RenderContent();                     break;
        case Section::Chat:    if (m_Chat)      m_Chat->RenderContent();                    break;
        case Section::Capture: if (m_Broadcast) m_Broadcast->RenderCaptureSection();        break;
        case Section::Layer:   if (m_Broadcast) m_Broadcast->RenderLayerSection();          break;
        case Section::Start:   if (m_Broadcast) m_Broadcast->RenderStartSection();          break;
    }

    ImGui::EndChild();
    ImGui::EndChild();
    ImGui::End();
}

} // namespace ProyecThor::UI
