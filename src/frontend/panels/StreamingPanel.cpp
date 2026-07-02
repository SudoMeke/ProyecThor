#include "StreamingPanel.h"
#include "backend/core/PresentationCore.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>

#include "stb_image_write.h"

#if __has_include("qrcodegen.hpp")
#   define PROYECTHOR_HAS_QRCODEGEN 1
#   include "qrcodegen.hpp"
#endif

namespace ProyecThor::UI {

// ── Paleta Mejorada (Estilo Cinemático/Premium) ───────────────────────────────
static constexpr ImVec4 kGreen      = { 0.15f, 0.85f, 0.45f, 1.0f };
static constexpr ImVec4 kRed        = { 0.90f, 0.25f, 0.30f, 1.0f };
static constexpr ImVec4 kGrayDim    = { 0.45f, 0.47f, 0.55f, 1.0f };
static constexpr ImVec4 kGrayText   = { 0.65f, 0.68f, 0.75f, 1.0f };
static constexpr ImVec4 kAccent     = { 0.25f, 0.55f, 1.00f, 1.0f };
static constexpr ImVec4 kAccentLow  = { 0.25f, 0.55f, 1.00f, 0.15f };
static constexpr ImVec4 kSurface    = { 0.05f, 0.06f, 0.08f, 1.0f };
static constexpr ImVec4 kSurface2   = { 0.10f, 0.11f, 0.15f, 1.0f };
static constexpr ImVec4 kSurface3   = { 0.13f, 0.15f, 0.20f, 1.0f };

static ImU32 Col(ImVec4 v)  { return ImGui::ColorConvertFloat4ToU32(v); }
static ImU32 ColA(ImVec4 v, float a) {
    v.w = a; return ImGui::ColorConvertFloat4ToU32(v);
}

// ── Efectos Visuales (Sombras y Gradientes) ───────────────────────────────────
static void DrawSoftShadow(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float rounding) {
    for (float i = 1.0f; i <= 6.0f; i += 1.0f) {
        float alpha = 40.0f - (i * 6.0f);
        dl->AddRectFilled(
            ImVec2(p0.x - i, p0.y - i + 4.0f), 
            ImVec2(p1.x + i, p1.y + i + 4.0f), 
            IM_COL32(0, 0, 0, (int)alpha), rounding + i);
    }
}

// ── stb callback ──────────────────────────────────────────────────────────────
static void StbCb(void* ctx, void* data, int size)
{
    auto* buf = static_cast<std::vector<uint8_t>*>(ctx);
    const uint8_t* p = static_cast<const uint8_t*>(data);
    buf->insert(buf->end(), p, p + size);
}

// ── CaptureAndPushFrame ───────────────────────────────────────────────────────
// ── CaptureAndPushFrame ───────────────────────────────────────────────────────
void StreamingPanel::CaptureAndPushFrame(int w, int h, int quality)
{
    if (w <= 0 || h <= 0) return;

    auto& core = Core::PresentationCore::Get();
    std::vector<uint8_t> rgb;
    if (!core.RenderProjectorToFBO(w, h, rgb)) return;

    // Se eliminó la inversión manual (flipped). 
    // Pasamos el buffer rgb directamente, ahorrando CPU y RAM en cada frame.
    std::vector<uint8_t> jpeg;
    jpeg.reserve(static_cast<size_t>(w) * h / 4);
    
    // Escribimos directamente desde rgb.data()
    stbi_write_jpg_to_func(StbCb, &jpeg, w, h, 3, rgb.data(), quality);

    core.PushFrame(std::move(jpeg));
}

// ── RebuildQR ─────────────────────────────────────────────────────────────────
void StreamingPanel::RebuildQRTexture(const std::string& url)
{
    if (url == m_QRCachedURL) return;
    m_QRCachedURL = url;
    m_QRModules.clear();
    m_QRSize = 0;
    if (url.empty()) return;

#ifdef PROYECTHOR_HAS_QRCODEGEN
    try {
        auto qr   = qrcodegen::QrCode::encodeText(url.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
        m_QRSize  = qr.getSize();
        m_QRModules.resize(static_cast<size_t>(m_QRSize) * m_QRSize, 0);
        for (int y = 0; y < m_QRSize; ++y)
            for (int x = 0; x < m_QRSize; ++x)
                m_QRModules[y * m_QRSize + x] = qr.getModule(x, y) ? 1 : 0;
    } catch (...) {}
#else
    m_QRSize = 21;
    m_QRModules.assign(static_cast<size_t>(m_QRSize) * m_QRSize, 0);
    auto setM = [&](int x, int y, uint8_t v) {
        if (x >= 0 && x < m_QRSize && y >= 0 && y < m_QRSize)
            m_QRModules[y * m_QRSize + x] = v;
    };
    auto finder = [&](int ox, int oy) {
        for (int dy = 0; dy < 7; ++dy)
            for (int dx = 0; dx < 7; ++dx)
                setM(ox+dx, oy+dy, (dx==0||dx==6||dy==0||dy==6||(dx>=2&&dx<=4&&dy>=2&&dy<=4)) ? 1 : 0);
    };
    finder(0, 0); finder(14, 0); finder(0, 14);
    for (int i = 8; i <= 12; i += 2) { setM(i,6,1); setM(6,i,1); }

    size_t seed = std::hash<std::string>{}(url);
    for (int y = 0; y < m_QRSize; ++y) {
        for (int x = 0; x < m_QRSize; ++x) {
            if (m_QRModules[y * m_QRSize + x]) continue;
            seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17;
            m_QRModules[y * m_QRSize + x] = (seed & 1) ? 1 : 0;
        }
    }
#endif
}

// ── DrawQR ────────────────────────────────────────────────────────────────────
void StreamingPanel::DrawQR(ImDrawList* dl, ImVec2 origin, float size)
{
    if (m_QRSize <= 0) return;

    DrawSoftShadow(dl, origin, ImVec2(origin.x + size, origin.y + size), 14.0f);

    dl->AddRectFilled(origin, ImVec2(origin.x + size, origin.y + size), IM_COL32(250, 250, 252, 255), 14.0f);
    dl->AddRect(origin, ImVec2(origin.x + size, origin.y + size), Col(kAccentLow), 14.0f, 0, 2.0f);

    const float pad   = size * 0.08f;
    const float inner = size - pad * 2.0f;
    const float cell  = inner / static_cast<float>(m_QRSize);

    for (int row = 0; row < m_QRSize; ++row) {
        for (int col = 0; col < m_QRSize; ++col) {
            if (!m_QRModules[row * m_QRSize + col]) continue;
            float x0 = origin.x + pad + col * cell + 0.5f;
            float y0 = origin.y + pad + row * cell + 0.5f;
            float x1 = x0 + cell - 1.0f;
            float y1 = y0 + cell - 1.0f;
            dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(15, 18, 26, 255), 1.5f);
        }
    }
}

// ── Helpers de layout ─────────────────────────────────────────────────────────
static void SectionDivider(const char* label)
{
    ImGui::Dummy(ImVec2(0, 10.0f));
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetCursorScreenPos();
    float       w   = ImGui::GetContentRegionAvail().x;
    ImVec2      ts  = ImGui::CalcTextSize(label);

    float cy  = pos.y + ts.y * 0.5f;
    float gap = 12.0f;
    float tx  = pos.x + (w - ts.x) * 0.5f;

    dl->AddRectFilledMultiColor(ImVec2(pos.x, cy), ImVec2(tx - gap, cy + 1.0f),
        IM_COL32(55, 60, 85, 0), IM_COL32(55, 60, 85, 180), IM_COL32(55, 60, 85, 180), IM_COL32(55, 60, 85, 0));
        
    dl->AddRectFilledMultiColor(ImVec2(tx + ts.x + gap, cy), ImVec2(pos.x + w, cy + 1.0f),
        IM_COL32(55, 60, 85, 180), IM_COL32(55, 60, 85, 0), IM_COL32(55, 60, 85, 0), IM_COL32(55, 60, 85, 180));

    dl->AddText(ImVec2(tx, pos.y), Col(kAccent), label);
    ImGui::Dummy(ImVec2(w, ts.y + 12.0f));
}

// ── Render ────────────────────────────────────────────────────────────────────
void StreamingPanel::Render()
{
    auto& core  = Core::PresentationCore::Get();
    auto  state = core.GetState();

    if (state.isStreamingNet && m_Config.sendBackground)
        CaptureAndPushFrame(m_Config.frameWidth, m_Config.frameHeight, m_Config.jpegQuality);

    if (state.isStreamingNet)
        RebuildQRTexture(state.networkURL);
    else if (!m_QRCachedURL.empty())
        RebuildQRTexture("");

    // Configuración limpia del contenedor principal sin forzar espaciados rotos
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kSurface);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ColA(kGrayText, 0.2f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ColA(kAccent, 0.5f));

    bool open = ImGui::Begin("Transmisión en Red", nullptr, ImGuiWindowFlags_NoCollapse);
    ImGui::PopStyleColor(4);

    if (!open) { ImGui::End(); return; }

    // Aplicamos el padding de forma nativa a la subregión de Scroll
    const float PAD = 24.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(PAD, PAD));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(10.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 6.0f);

    // Activamos la región del Child permitiendo scroll automático
    ImGui::BeginChild("##scroll_area", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_None);
    ImGui::PopStyleVar(3);

    RenderServerControl();
    RenderLayerSelector();
    RenderQualitySelector();

    if (state.isStreamingNet) {
        RenderURLSection();
    }

    ImGui::Dummy(ImVec2(0.0f, 20.0f)); // Espacio final respiratorio
    ImGui::EndChild();
    ImGui::End();
}

// ── RenderServerControl ───────────────────────────────────────────────────────
void StreamingPanel::RenderServerControl()
{
    auto& core  = Core::PresentationCore::Get();
    auto  state = core.GetState();
    bool  on    = state.isStreamingNet;
    float w     = ImGui::GetContentRegionAvail().x;
    float t     = static_cast<float>(ImGui::GetTime());

    // Capturamos la base del layout local actual
    float startLocalY = ImGui::GetCursorPosY();
    float cardH = 75.0f; 

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2      p0 = ImGui::GetCursorScreenPos();
    ImVec2      p1 = ImVec2(p0.x + w, p0.y + cardH);

    DrawSoftShadow(dl, p0, p1, 10.0f);

    ImU32 bg  = on ? Col(ImVec4(0.12f, 0.22f, 0.16f, 1.0f)) : Col(kSurface2);
    ImU32 bdr = on ? IM_COL32(60, 200, 100, 80) : IM_COL32(255, 255, 255, 12);
    dl->AddRectFilled(p0, p1, bg, 10.0f);
    dl->AddRect(p0, p1, bdr, 10.0f, 0, 1.5f);

    if (on) {
        float pulse = 0.4f + 0.6f * std::sin(t * 2.5f);
        dl->AddRectFilled(p0, ImVec2(p0.x + 4.0f, p0.y + cardH),
            ColA(kGreen, pulse), 10.0f, ImDrawFlags_RoundCornersLeft);
    }

    // Dibujamos textos internos respetando el flujo sin saltar a posiciones absolutas rotas
    float innerX  = 20.0f;
    float labelY  = (cardH - ImGui::GetTextLineHeight() * 2.0f - 6.0f) * 0.5f;

    dl->AddText(ImVec2(p0.x + innerX, p0.y + labelY),
        on ? Col(kGreen) : Col(kGrayText),
        on ? "TRANSMITIENDO" : "SERVIDOR DETENIDO");

    if (on) {
        float dotPulse = 0.6f + 0.4f * std::sin(t * 4.0f);
        dl->AddCircleFilled(ImVec2(p0.x + innerX - 10.0f, p0.y + labelY + 7.0f), 3.5f, ColA(kGreen, dotPulse));
    }

    dl->AddText(ImVec2(p0.x + innerX, p0.y + labelY + ImGui::GetTextLineHeight() + 6.0f),
        on ? ColA(kGreen, 0.8f) : ColA(kGrayDim, 0.8f),
        on ? (std::string("Puerto local ") + std::to_string(m_Port) + " abierto").c_str()
           : "Configura el puerto y presiona Iniciar");

    // Input de puerto perfectamente alineado usando coordenadas Locales controladas
    float portW = 80.0f;
    ImGui::SetCursorPos(ImVec2(w - portW - 16.0f, startLocalY + (cardH - 28.0f) * 0.5f));
    ImGui::SetNextItemWidth(portW);
    
    ImGui::BeginDisabled(on);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.02f, 0.03f, 0.05f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.1f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
    ImGui::InputInt("##port", &m_Port, 0, 0);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    ImGui::EndDisabled();
    m_Port = std::max(1024, std::min(65535, m_Port));

    // Forzamos el avance limpio del cursor al final exacto de la tarjeta de estado
    ImGui::SetCursorPosY(startLocalY + cardH);
    ImGui::Spacing();

    // ── Botón ON/OFF ─────────────────────────────────────────────────────
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(0.0f, 12.0f));

    if (!on) {
        ImGui::PushStyleColor(ImGuiCol_Button, ColA(kGreen, 0.15f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ColA(kGreen, 0.25f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ColA(kGreen, 0.35f));
        ImGui::PushStyleColor(ImGuiCol_Text, kGreen);
        ImGui::PushStyleColor(ImGuiCol_Border, ColA(kGreen, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        
        if (ImGui::Button("INICIAR TRANSMISIÓN", ImVec2(w, 0.0f)))
            core.ToggleNetworkStream(true, m_Port);
            
        ImGui::PopStyleVar(1);
        ImGui::PopStyleColor(5);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ColA(kRed, 0.15f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ColA(kRed, 0.25f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ColA(kRed, 0.35f));
        ImGui::PushStyleColor(ImGuiCol_Text, kRed);
        ImGui::PushStyleColor(ImGuiCol_Border, ColA(kRed, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        
        if (ImGui::Button("DETENER TRANSMISIÓN", ImVec2(w, 0.0f)))
            core.ToggleNetworkStream(false);
            
        ImGui::PopStyleVar(1);
        ImGui::PopStyleColor(5);
    }
    ImGui::PopStyleVar(2);
    ImGui::Dummy(ImVec2(0.0f, 10.0f));
}

// ── RenderLayerSelector ───────────────────────────────────────────────────────
void StreamingPanel::RenderLayerSelector()
{
    auto& core = Core::PresentationCore::Get();
    bool  on   = core.IsStreamingNet();
    float w    = ImGui::GetContentRegionAvail().x;

    SectionDivider("CAPAS A TRANSMITIR");

    bool dirty = false;
    struct Row { const char* id; const char* name; const char* desc; bool* val; };
    Row rows[] = {
        { "##cbg", "Fondo",   "Color sólido o cámara base", &m_Config.sendBackground },
        { "##ctx", "Textos",  "Letras, Biblia y canciones", &m_Config.sendText       },
        { "##cov", "Overlay", "Gráficos e imágenes superpuestas", &m_Config.sendOverlay    },
    };

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float rowH  = 44.0f;
    float totalRowsH = rowH * 3.0f;

    ImVec2 groupP0 = ImGui::GetCursorScreenPos();
    DrawSoftShadow(dl, groupP0, ImVec2(groupP0.x + w, groupP0.y + totalRowsH), 12.0f);

    // Guardamos la base del eje Y local de las filas
    float startRowsLocalY = ImGui::GetCursorPosY();

    for (int i = 0; i < 3; ++i) {
        auto& row   = rows[i];
        float currentLocalY = startRowsLocalY + (i * rowH);

        ImVec2 p0 = ImVec2(groupP0.x, groupP0.y + (i * rowH));
        ImVec2 p1 = ImVec2(p0.x + w, p0.y + rowH);

        ImDrawFlags corners = (i == 0) ? ImDrawFlags_RoundCornersTop :
                              (i == 2) ? ImDrawFlags_RoundCornersBottom : 
                                         ImDrawFlags_RoundCornersNone;

        dl->AddRectFilled(p0, p1, Col(kSurface2), 12.0f, corners);
        
        if (i < 2) 
            dl->AddLine(ImVec2(p0.x + 16.0f, p1.y), ImVec2(p1.x - 16.0f, p1.y), IM_COL32(255, 255, 255, 10), 1.0f);

        ImGui::PushStyleColor(ImGuiCol_CheckMark, kAccent);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, Col(kSurface));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ColA(kAccent, 0.2f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));

        // Alineación perfecta basada en coordenadas locales relativas a la fila
        float localCbY = currentLocalY + (rowH - ImGui::GetFrameHeight()) * 0.5f;
        ImGui::SetCursorPos(ImVec2(16.0f, localCbY));

        bool v = *row.val;
        if (ImGui::Checkbox(row.id, &v)) { *row.val = v; dirty = true; }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);

        ImGui::SameLine(0.0f, 12.0f);
        ImGui::SetCursorPosY(currentLocalY + (rowH - ImGui::GetTextLineHeight()) * 0.5f);
        ImGui::TextUnformatted(row.name);
        
        ImGui::SameLine(0.0f, 8.0f);
        ImGui::TextColored(kGrayDim, "- %s", row.desc);
    }

    // Avanzamos el cursor de forma segura saltándonos las 3 filas
    ImGui::SetCursorPosY(startRowsLocalY + totalRowsH);

    if (dirty) m_ConfigDirty = true;

    ImGui::Dummy(ImVec2(0.0f, 15.0f));
    SectionDivider("RESOLUCIÓN Y CALIDAD");

    // ── Contenedor de resolución ─────────────────────────────────────────
    {
        float containerH = 120.0f;
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 p1 = ImVec2(p0.x + w, p0.y + containerH);
        DrawSoftShadow(dl, p0, p1, 12.0f);
        dl->AddRectFilled(p0, p1, Col(kSurface2), 12.0f);
        
        float startContainerLocalY = ImGui::GetCursorPosY();
        
        // Fila Inputs
        ImGui::SetCursorPos(ImVec2(16.0f, startContainerLocalY + 16.0f));
        float half = (w - 32.0f - 16.0f) * 0.5f;

        ImGui::PushStyleColor(ImGuiCol_FrameBg, Col(kSurface));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));

        ImGui::TextColored(kGrayText, "Ancho");
        ImGui::SameLine(0.0f, 8.0f);
        ImGui::SetNextItemWidth(half - 45.0f);
        ImGui::InputInt("##rw", &m_Config.frameWidth, 0, 0);

        ImGui::SameLine(0.0f, 16.0f);
        ImGui::TextColored(kGrayText, "Alto");
        ImGui::SameLine(0.0f, 8.0f);
        ImGui::SetNextItemWidth(half - 45.0f);
        ImGui::InputInt("##rh", &m_Config.frameHeight, 0, 0);

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(1);

        m_Config.frameWidth  = std::max(320,  std::min(1920, m_Config.frameWidth));
        m_Config.frameHeight = std::max(180,  std::min(1080, m_Config.frameHeight));
        
        // Fila Texto Slider
        ImGui::SetCursorPos(ImVec2(16.0f, startContainerLocalY + 58.0f));
        ImGui::TextColored(kGrayText, "Compresión JPEG:");
        ImGui::SameLine();
        ImGui::TextColored(kAccent, "%d%%", m_Config.jpegQuality);

        // Fila Renderizado del Slider
        ImGui::SetCursorPos(ImVec2(16.0f, startContainerLocalY + 84.0f));
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, kAccent);
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ColA(kAccent, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, Col(kSurface));
        ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::SetNextItemWidth(w - 32.0f);
        
        if (ImGui::SliderInt("##q", &m_Config.jpegQuality, 20, 100, ""))
            m_ConfigDirty = true;
            
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
        
        ImGui::SetCursorPosY(startContainerLocalY + containerH);
    }

    if (m_ConfigDirty && on) {
        core.GetNetworkServer()->SetConfig(m_Config);
        m_ConfigDirty = false;
    }
}

// ── RenderQualitySelector ─────────────────────────────────────────────────────
void StreamingPanel::RenderQualitySelector()
{
    auto& core = Core::PresentationCore::Get();
    bool  on   = core.IsStreamingNet();
    float w    = ImGui::GetContentRegionAvail().x;

    ImGui::Dummy(ImVec2(0.0f, 15.0f));
    SectionDivider("MODO DE TRANSMISIÓN");

    bool isHQ = (m_Config.videoMode == Core::StreamConfig::VideoMode::HighQuality);

    struct Card {
        const char* id; const char* title; const char* sub1; const char* sub2;
        bool active; Core::StreamConfig::VideoMode mode;
    } cards[2] = {
        { "##ll", "Bajo Consumo",  "~150 ms latencia", "Polling (Dispositivos lentos)",  !isHQ, Core::StreamConfig::VideoMode::LowLatency  },
        { "##hq", "Alta Calidad",  "< 33 ms latencia", "MJPEG Fluido (Recomendado)",      isHQ, Core::StreamConfig::VideoMode::HighQuality },
    };

    float cardW = (w - 12.0f) * 0.5f;
    float cardH = 85.0f;

    float startSelectorLocalY = ImGui::GetCursorPosY();
    ImVec2 baseScreenPos = ImGui::GetCursorScreenPos();

    for (int i = 0; i < 2; ++i) {
        auto& c = cards[i];
        float localX = (i == 0) ? 0.0f : cardW + 12.0f;

        ImVec2 p0 = ImVec2(baseScreenPos.x + localX, baseScreenPos.y);
        ImVec2 p1 = ImVec2(p0.x + cardW, p0.y + cardH);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        DrawSoftShadow(dl, p0, p1, 12.0f);

        ImU32 bg  = c.active ? Col(kSurface3) : Col(kSurface2);
        ImU32 bdr = c.active ? ColA(kAccent, 0.6f) : IM_COL32(255, 255, 255, 10);

        dl->AddRectFilled(p0, p1, bg, 12.0f);
        dl->AddRect(p0, p1, bdr, 12.0f, 0, c.active ? 2.0f : 1.0f);

        if (c.active) {
            dl->AddRectFilledMultiColor(p0, ImVec2(p1.x, p0.y + 20.0f),
                ColA(kAccent, 0.15f), ColA(kAccent, 0.15f), ColA(kAccent, 0.0f), ColA(kAccent, 0.0f));
            dl->AddRectFilled(p0, ImVec2(p1.x, p0.y + 4.0f), Col(kAccent), 12.0f, ImDrawFlags_RoundCornersTop);
        }

        // Posicionamiento local del botón invisible para evitar solapamientos rotos
        ImGui::SetCursorPos(ImVec2(localX, startSelectorLocalY));
        ImGui::InvisibleButton(c.id, ImVec2(cardW, cardH));
        
        bool hovered = ImGui::IsItemHovered();
        bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

        if (hovered && !c.active) {
            dl->AddRectFilled(p0, p1, ColA(kAccent, 0.05f), 12.0f);
        }

        float lh = ImGui::GetTextLineHeight();
        float py = p0.y + 14.0f;
        float px = p0.x + 14.0f;

        dl->AddText(ImVec2(px, py), c.active ? Col(kAccent) : Col(kGrayText), c.title);
        py += lh + 6.0f;
        dl->AddText(ImVec2(px, py), Col(kGrayDim), c.sub1);
        py += lh + 4.0f;
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.85f, ImVec2(px, py), 
            ColA(kGrayDim, 0.7f), c.sub2, nullptr, cardW - 20.0f);

        if (!c.active && clicked) {
            m_Config.videoMode = c.mode;
            m_ConfigDirty = true;
        }
    }

    ImGui::SetCursorPosY(startSelectorLocalY + cardH);

    if (m_ConfigDirty && on) {
        core.GetNetworkServer()->SetConfig(m_Config);
        m_ConfigDirty = false;
    }
}

// ── RenderURLSection ──────────────────────────────────────────────────────────
void StreamingPanel::RenderURLSection()
{
    auto  state = Core::PresentationCore::Get().GetState();
    float w     = ImGui::GetContentRegionAvail().x;

    ImGui::Dummy(ImVec2(0.0f, 15.0f));
    SectionDivider("ACCESO DISPOSITIVOS MÓVILES");

    // ── QR ────────────────────────────────────────────────────────────────
    {
        float qrSize = std::min(w * 0.75f, 220.0f);
        float cardPad = 16.0f;
        float cardW   = qrSize + cardPad * 2.0f;
        float cardH   = qrSize + cardPad * 2.0f;
        float cardOffX = (w - cardW) * 0.5f;

        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 cp0 = ImVec2(p0.x + cardOffX, p0.y);
        ImVec2 cp1 = ImVec2(cp0.x + cardW, cp0.y + cardH);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        DrawSoftShadow(dl, cp0, cp1, 16.0f);

        dl->AddRectFilled(cp0, cp1, Col(kSurface2), 16.0f);
        dl->AddRect(cp0, cp1, IM_COL32(255, 255, 255, 12), 16.0f, 0, 1.0f);

        DrawQR(dl, ImVec2(cp0.x + cardPad, cp0.y + cardPad), qrSize);

        ImGui::Dummy(ImVec2(w, cardH + 10.0f));
    }

    // ── URL copiable ──────────────────────────────────────────────────────
    {
        float btnW = 90.0f;
        float gap  = 8.0f;
        float fieldW = w - btnW - gap;

        ImGui::PushStyleColor(ImGuiCol_FrameBg, Col(kSurface));
        ImGui::PushStyleColor(ImGuiCol_Border, ColA(kAccent, 0.3f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        char urlBuf[256];
        std::strncpy(urlBuf, state.networkURL.c_str(), sizeof(urlBuf) - 1);
        urlBuf[sizeof(urlBuf) - 1] = '\0';

        ImGui::SetNextItemWidth(fieldW);
        ImGui::InputText("##url", urlBuf, sizeof(urlBuf), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);

        ImGui::SameLine(0.0f, gap);

        ImGui::PushStyleColor(ImGuiCol_Button, ColA(kAccent, 0.2f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ColA(kAccent, 0.35f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ColA(kAccent, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 10.0f));

        if (ImGui::Button("Copiar Link", ImVec2(btnW, 0.0f)))
            ImGui::SetClipboardText(state.networkURL.c_str());

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);
    }

    ImGui::Dummy(ImVec2(0.0f, 5.0f));

    // Hint
    {
        const char* hint = "Abre esta URL desde la cámara de tu teléfono";
        const char* hint2 = "estando en la misma red WiFi local.";
        ImVec2 ts1 = ImGui::CalcTextSize(hint);
        ImVec2 ts2 = ImGui::CalcTextSize(hint2);
        
        ImGui::SetCursorPosX((w - ts1.x) * 0.5f);
        ImGui::TextUnformatted(hint);
        
        ImGui::SetCursorPosX((w - ts2.x) * 0.5f);
        ImGui::TextColored(kGrayDim, "%s", hint2);
    }
}

} // namespace ProyecThor::UI