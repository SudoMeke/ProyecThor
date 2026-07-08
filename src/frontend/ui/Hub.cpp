#include "Hub.h"

// Includes del sistema que clangd no encontraba porque Hub.h no los incluia
#include <GL/glew.h>
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>
#include "settings/SettingsManager.h"
#include "../external/tools/OpenURL.h"
#include "Version.h"
#include "DesignSystem.h"
#include "HubTheme.h"

extern GLuint LoadTextureFromFile(const char* filename);

static constexpr float HUB_SIDEBAR_W  = 280.0f;
static constexpr float HUB_APPEAR_SPD = 3.0f;

namespace DS = ProyecThor::UI::DS;
namespace HT = ProyecThor::UI::HubTheme;

namespace ProyecThor::UI {

// ── Registro de versiones y portadas ────────────────────────────────────────
//  Cada entrada define su propia imagen de portada, de forma que agregar una
//  nueva actualizacion con una foto distinta sea tan simple como anadir una
//  linea aqui. "id" es el mismo valor que usa el modal (selectedUpdateVer).
// ─────────────────────────────────────────────────────────────────────────
struct UpdateVersionInfo {
    int         id;         // Identificador interno (coincide con selectedUpdateVer)
    const char* version;    // "0.3.1"
    const char* modalBadge; // Texto de insignia mostrado dentro del modal
    const char* cardBadge;  // Texto de insignia mostrado en la tarjeta de la lista
    const char* coverFile;  // Imagen de portada especifica de esta version
    const char* summary;    // Resumen corto mostrado en la tarjeta
};

// Para agregar una nueva actualizacion con su propia portada, solo hay que
// anadir una nueva linea a esta lista con su archivo de imagen.
static const std::vector<UpdateVersionInfo> kUpdateRegistry = {
    {
        3, "0.3.2",
        " ACTUALIZACION FUNCIONAL ", "ACTUALIZACION",
        "splash_bg4.png",
        "Soporte oficial para Linux, mejoras de rendimiento en video, cola de "
        "reproduccion mas estable, nuevos estilos de la app y correccion en "
        "guardado/carga de ajustes."
    },
    {
        1, "0.3.1",
        " ACTUALIZACION FUNCIONAL ", "ACTUALIZACION",
        "splash_bg1.png",
        "Audio Rework completo, nueva interfaz, sistema de covers y mejoras generales de personalizacion."
    },
    {
        2, "0.3.0",
        " ACTUALIZACION MAYOR  ", "ACTUALIZACION MAYOR",
        "bg_splash3.png",
        "Nuevas herramientas de transmision, optimizaciones y estabilidad de red."
    },
};

static const UpdateVersionInfo* FindUpdateVersion(int id) {
    for (const auto& v : kUpdateRegistry)
        if (v.id == id) return &v;
    return kUpdateRegistry.empty() ? nullptr : &kUpdateRegistry[0];
}

// ── Texturas de portada (con dimensiones) ───────────────────────────────────
//  Guardamos ancho/alto ademas del id de GL: los necesitamos para calcular
//  el recorte "cover" (llenar la caja sin deformar la imagen).
struct GLTextureInfo {
    GLuint id     = 0;
    int    width  = 0;
    int    height = 0;
};

// Cache simple de texturas por nombre de archivo: evita recargar la misma
// portada varias veces si dos entradas del registro la comparten.
static GLTextureInfo GetCoverTexture(const char* filename) {
    static std::unordered_map<std::string, GLTextureInfo> s_Cache;
    auto it = s_Cache.find(filename);
    if (it != s_Cache.end())
        return it->second;

    GLTextureInfo info;
    info.id = LoadTextureFromFile(filename);
    if (info.id != 0) {
        glBindTexture(GL_TEXTURE_2D, info.id);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH,  &info.width);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &info.height);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    s_Cache.emplace(filename, info);
    return info;
}

// ── Parallax de portadas ─────────────────────────────────────────────────
//  Estado (offset/zoom) por imagen, identificado con una key propia
//  (p.ej. "card_0.3.1", "modal_0.3.0") para poder interpolarlo suavemente
//  cuadro a cuadro en vez de saltar de golpe.
struct ParallaxState { float ox = 0.0f, oy = 0.0f, zoom = 1.0f; };

static std::unordered_map<std::string, ParallaxState>& GetParallaxStates() {
    static std::unordered_map<std::string, ParallaxState> s_States;
    return s_States;
}

// Dibuja una imagen llenando por completo el rectangulo [pMin, pMax] sin
// deformarla -- recorte tipo "cover" (como background-size:cover en CSS) --
// con esquinas redondeadas y un efecto sutil de profundidad/parallax:
// al pasar el mouse por encima la imagen hace un zoom leve y se desplaza
// dentro de su propio recorte siguiendo al cursor, en vez de mostrar un
// corte estatico y "duro" al entrar en hover.
static void DrawCoverImageCover(ImDrawList* dl, GLuint texId, int texW, int texH,
                                 ImVec2 pMin, ImVec2 pMax,
                                 float rounding, ImDrawFlags roundFlags,
                                 const char* stateKey, float dt,
                                 bool hovered, float maxZoom)
{
    if (texId == 0 || texW <= 0 || texH <= 0) {
        dl->AddRectFilled(pMin, pMax, IM_COL32(20, 20, 24, 255), rounding, roundFlags);
        return;
    }

    ParallaxState& st = GetParallaxStates()[stateKey];

    const float boxW = std::max(1.0f, pMax.x - pMin.x);
    const float boxH = std::max(1.0f, pMax.y - pMin.y);

    // Objetivo de zoom y desplazamiento segun el hover actual
    float targetZoom = hovered ? maxZoom : 1.0f;
    float targetOX   = 0.0f, targetOY = 0.0f;
    if (hovered) {
        const ImVec2 mouse = ImGui::GetMousePos();
        targetOX = std::clamp(((mouse.x - pMin.x) / boxW) * 2.0f - 1.0f, -1.0f, 1.0f);
        targetOY = std::clamp(((mouse.y - pMin.y) / boxH) * 2.0f - 1.0f, -1.0f, 1.0f);
    }

    // Interpolacion suave (tipo resorte) para que el movimiento no sea brusco
    const float speed = 9.0f;
    const float t = std::clamp(dt * speed, 0.0f, 1.0f);
    st.zoom += (targetZoom - st.zoom) * t;
    st.ox   += (targetOX   - st.ox)   * t;
    st.oy   += (targetOY   - st.oy)   * t;

    // ── Recorte "cover": la imagen llena la caja completa sin deformarse ────
    const float boxAspect = boxW / boxH;
    const float imgAspect = static_cast<float>(texW) / static_cast<float>(texH);

    float baseUW, baseUH;
    if (imgAspect > boxAspect) {
        // Imagen mas ancha que la caja -> se recortan los lados, se ve completa en alto
        baseUH = 1.0f;
        baseUW = boxAspect / imgAspect;
    } else {
        // Imagen mas alta que la caja -> se recorta arriba/abajo, se ve completa en ancho
        baseUW = 1.0f;
        baseUH = imgAspect / boxAspect;
    }

    const float zoom = std::max(1.0f, st.zoom);
    const float uw = baseUW / zoom;
    const float uh = baseUH / zoom;

    // Margen disponible dentro de la textura para "pasear" la ventana visible
    const float marginX = std::max(0.0f, (1.0f - uw) * 0.5f);
    const float marginY = std::max(0.0f, (1.0f - uh) * 0.5f);

    const float centerU = 0.5f + st.ox * marginX;
    const float centerV = 0.5f + st.oy * marginY;

    const ImVec2 uv0(centerU - uw * 0.5f, centerV - uh * 0.5f);
    const ImVec2 uv1(centerU + uw * 0.5f, centerV + uh * 0.5f);

    dl->AddImageRounded((ImTextureID)(intptr_t)texId, pMin, pMax, uv0, uv1,
        IM_COL32(255, 255, 255, 255), rounding, roundFlags);
}

static float EaseOut(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return 1.0f - (1.0f - t) * (1.0f - t);
}

Hub::Hub() : m_LastFrameTime(std::chrono::steady_clock::now()) {
    const auto& settings = ProyecThor::Settings::SettingsManager::Get().GetSettings();
    m_SelectedMonitor = settings.projection.targetMonitor;
}

void Hub::ForceOpen() {
    m_Open                  = true;
    m_Appearing             = true;
    m_AppearProgress        = 0.0f;
    m_LaunchRequested       = false;
    m_OpenSettingsRequested = false;
    m_LastFrameTime         = std::chrono::steady_clock::now();
}

void Hub::UpdateAnimations(float dt) {
    if (m_Appearing) {
        m_AppearProgress += dt * HUB_APPEAR_SPD;
        if (m_AppearProgress >= 1.0f) {
            m_AppearProgress = 1.0f;
            m_Appearing      = false;
        }
    }
}

bool Hub::Render() {
    if (!m_Open) return false;

    const auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - m_LastFrameTime).count();
    m_LastFrameTime = now;
    dt = std::min(dt, 0.05f);

    m_Time += dt;

    UpdateAnimations(dt);

    ImGuiViewport* vp = ImGui::GetMainViewport();

    UpdateBgParticles(dt, vp->WorkSize.x - HUB_SIDEBAR_W, vp->WorkSize.y);

    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowBgAlpha(0.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    constexpr ImGuiWindowFlags rootFlags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoDocking;

    ImGui::Begin("##HubRoot", nullptr, rootFlags);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2      wp = ImGui::GetWindowPos();

    dl->AddRectFilled(wp,
        ImVec2(wp.x + HUB_SIDEBAR_W, wp.y + vp->WorkSize.y), HT::BgSidebar);
    dl->AddRectFilled(
        ImVec2(wp.x + HUB_SIDEBAR_W, wp.y),
        ImVec2(wp.x + vp->WorkSize.x, wp.y + vp->WorkSize.y), HT::BgMain);

    dl->AddLine(
        ImVec2(wp.x + HUB_SIDEBAR_W, wp.y),
        ImVec2(wp.x + HUB_SIDEBAR_W, wp.y + vp->WorkSize.y),
        HT::Divider, 1.0f);

    RenderSidebar(HUB_SIDEBAR_W, vp->WorkSize.y);
    ImGui::SameLine(0.0f, 0.0f);
    RenderMainContent(vp->WorkSize.x - HUB_SIDEBAR_W, vp->WorkSize.y);

    ImGui::End();
    ImGui::PopStyleVar(2);

    if (m_LaunchRequested) {
        m_LaunchRequested = false;
        m_Open            = false;
        return true;
    }

    return false;
}

void Hub::RenderSidebar(float w, float h) {
    ImGui::BeginChild("##Sidebar", ImVec2(w, h), false);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2      wp = ImGui::GetWindowPos();

    ImGui::SetCursorPosY(40.0f);
    ImGui::SetCursorPosX(30.0f);

    {
        ImFont*     font         = ImGui::GetFont();
        const float logoFontSize = ImGui::GetFontSize() * 1.5f;

        const ImVec2 logoScreenPos = ImGui::GetCursorScreenPos();

        const ImVec2 sizeProyec = font->CalcTextSizeA(logoFontSize, FLT_MAX, 0.0f, "Proyec");
        const ImVec2 sizeThor   = font->CalcTextSizeA(logoFontSize, FLT_MAX, 0.0f, "Thor");

        const ImVec2 posProyec = logoScreenPos;
        const ImVec2 posThor   = ImVec2(logoScreenPos.x + sizeProyec.x, logoScreenPos.y);

        for (int ox = -3; ox <= 3; ox++) {
            for (int oy = -3; oy <= 3; oy++) {
                if (ox == 0 && oy == 0) continue;
                const float dist = sqrtf(static_cast<float>(ox * ox + oy * oy));
                if (dist > 3.5f) continue;
                const int glowAlpha = static_cast<int>(18.0f * (1.0f - dist / 3.5f));
                dl->AddText(font, logoFontSize,
                    ImVec2(posThor.x + static_cast<float>(ox),
                           posThor.y + static_cast<float>(oy)),
                    IM_COL32(115, 244, 233, glowAlpha), "Thor");
            }
        }
        for (int ox = -1; ox <= 1; ox++) {
            for (int oy = -1; oy <= 1; oy++) {
                if (ox == 0 && oy == 0) continue;
                dl->AddText(font, logoFontSize,
                    ImVec2(posThor.x + static_cast<float>(ox),
                           posThor.y + static_cast<float>(oy)),
                    IM_COL32(115, 244, 233, 35), "Thor");
            }
        }

        dl->AddText(font, logoFontSize, posProyec, IM_COL32(255, 255, 255, 255), "Proyec");
        dl->AddText(font, logoFontSize, posThor,   IM_COL32(115, 244, 233, 255), "Thor");

        ImGui::Dummy(ImVec2(sizeProyec.x + sizeThor.x, logoFontSize));
    }

    ImGui::SetCursorPosX(30.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, HT::TextMuted);
    ImGui::Text("v%s", PROYECTHOR_VERSION_STRING);
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 32.0f));

    dl->AddLine(
        ImVec2(wp.x + 20.0f, wp.y + ImGui::GetCursorPosY()),
        ImVec2(wp.x + w - 20.0f, wp.y + ImGui::GetCursorPosY()),
        HT::Divider, 1.0f);

    ImGui::Dummy(ImVec2(0.0f, 16.0f));

    ImGui::SetCursorPosX(30.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        HT::AccentBlue);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertFloat4ToU32(
        ImVec4(ImGui::ColorConvertU32ToFloat4(HT::AccentBlue).x + 0.08f,
               ImGui::ColorConvertU32ToFloat4(HT::AccentBlue).y + 0.08f,
               ImGui::ColorConvertU32ToFloat4(HT::AccentBlue).z + 0.08f, 1.0f)));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImGui::ColorConvertFloat4ToU32(
        ImVec4(ImGui::ColorConvertU32ToFloat4(HT::AccentBlue).x - 0.08f,
               ImGui::ColorConvertU32ToFloat4(HT::AccentBlue).y - 0.08f,
               ImGui::ColorConvertU32ToFloat4(HT::AccentBlue).z - 0.08f, 1.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

    if (ImGui::Button("Empezar a proyectar", ImVec2(w - 60.0f, 45.0f)))
        m_LaunchRequested = true;

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    ImGui::SetCursorPosX(30.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(38, 38, 46, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(52, 52, 62, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(30, 30, 38, 255));
    ImGui::PushStyleColor(ImGuiCol_Text,          HT::TextPri);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

    if (ImGui::Button("Abrir configuracion", ImVec2(w - 60.0f, 36.0f)))
        m_OpenSettingsRequested = true;

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    ImGui::Dummy(ImVec2(0.0f, 28.0f));

    dl->AddLine(
        ImVec2(wp.x + 20.0f, wp.y + ImGui::GetCursorPosY()),
        ImVec2(wp.x + w - 20.0f, wp.y + ImGui::GetCursorPosY()),
        HT::Divider, 1.0f);

    ImGui::Dummy(ImVec2(0.0f, 16.0f));

    ImGui::SetCursorPosX(30.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, HT::TextMuted);
    ImGui::TextUnformatted("Accesos rapidos");
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    auto QuickBtn = [&](const char* icon, const char* label, int settingsTab) {
        ImGui::SetCursorPosX(30.0f);
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(48, 48, 58, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(38, 38, 48, 255));
        ImGui::PushStyleColor(ImGuiCol_Text,          IM_COL32(190, 190, 198, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

        char id[64];
        snprintf(id, sizeof(id), "%s  %s##qb%d", icon, label, settingsTab);

        if (ImGui::Button(id, ImVec2(w - 60.0f, 32.0f))) {
            m_ActiveTab             = settingsTab;
            m_OpenSettingsRequested = true;
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
    };

    QuickBtn("", "Apariencia",      0);
    QuickBtn("", "Idioma",          4);
    QuickBtn("", "Actualizaciones", 5);

    ImGui::EndChild();
}

void Hub::InitBgParticles(float w, float h) {
    std::mt19937 rng(static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(m_BgParticles.data()) ^ 0xDEADBEEF));

    auto frand = [&](float lo, float hi) -> float {
        return lo + (hi - lo) * (static_cast<float>(rng()) / static_cast<float>(rng.max()));
    };

    for (auto& p : m_BgParticles) {
        p.x      = frand(0.0f, w);
        p.y      = frand(0.0f, h);
        p.vx     = frand(-0.18f, 0.18f);
        p.vy     = frand(-0.18f, 0.18f);
        p.r      = frand(0.6f, 2.2f);
        p.phase  = frand(0.0f, 6.28318530717958647f);
        p.isCyan = (frand(0.0f, 1.0f) > 0.72f);
    }

    m_BgParticlesInit = true;
}

void Hub::UpdateBgParticles(float dt, float w, float h) {
    for (auto& p : m_BgParticles) {
        p.x += p.vx * dt * 60.0f;
        p.y += p.vy * dt * 60.0f;

        if (p.x < 0.0f) p.x += w;
        if (p.x > w)    p.x -= w;
        if (p.y < 0.0f) p.y += h;
        if (p.y > h)    p.y -= h;
    }
}

void Hub::RenderBgCanvas(ImDrawList* dl, ImVec2 origin, float w, float h) {
    const ImU32 gridCol = IM_COL32(255, 255, 255, 6);
    for (float x = 0.0f; x < w; x += BG_GRID_SIZE)
        dl->AddLine(ImVec2(origin.x + x, origin.y), ImVec2(origin.x + x, origin.y + h), gridCol, 0.5f);
    for (float y = 0.0f; y < h; y += BG_GRID_SIZE)
        dl->AddLine(ImVec2(origin.x, origin.y + y), ImVec2(origin.x + w, origin.y + y), gridCol, 0.5f);

    for (const auto& p : m_BgParticles) {
        const float sinVal = sinf(m_Time * 0.75f + p.phase);
        const float alpha  = 0.18f + 0.14f * sinVal;
        const ImU32 col    = p.isCyan
            ? IM_COL32(0,   212, 232, static_cast<int>(alpha * 255.0f))
            : IM_COL32(115, 244, 205, static_cast<int>(alpha * 255.0f));
        dl->AddCircleFilled(ImVec2(origin.x + p.x, origin.y + p.y), p.r, col, 8);
    }

    for (int i = 0; i < BG_PARTICLE_COUNT; i++) {
        for (int j = i + 1; j < BG_PARTICLE_COUNT; j++) {
            const float dx   = m_BgParticles[i].x - m_BgParticles[j].x;
            const float dy   = m_BgParticles[i].y - m_BgParticles[j].y;
            const float dist = sqrtf(dx * dx + dy * dy);
            if (dist < BG_CONNECT_DIST) {
                const float t       = 1.0f - (dist / BG_CONNECT_DIST);
                const float alpha   = t * t * 0.09f;
                const ImU32 lineCol = IM_COL32(115, 244, 205, static_cast<int>(alpha * 255.0f));
                dl->AddLine(
                    ImVec2(origin.x + m_BgParticles[i].x, origin.y + m_BgParticles[i].y),
                    ImVec2(origin.x + m_BgParticles[j].x, origin.y + m_BgParticles[j].y),
                    lineCol, 0.5f);
            }
        }
    }
}

void Hub::RenderMainContent(float w, float h) {
    static GLuint bgTex             = 0;
    static bool   texLoaded         = false;
    static bool   isUpdateModalOpen = false;
    static int    selectedUpdateVer = 3; // id de kUpdateRegistry (3 = v0.3.2, 1 = v0.3.1, 2 = v0.3.0)

    if (!texLoaded) {
        bgTex     = LoadTextureFromFile("splash_bg2.png");
        texLoaded = true;
    }

    if (!m_BgParticlesInit)
        InitBgParticles(w, h);

    // dt propio para las animaciones de parallax de esta funcion (no depende
    // de m_Time para poder reutilizar el helper de forma autonoma).
    float parallaxDt;
    {
        static std::chrono::steady_clock::time_point s_LastParallaxT = std::chrono::steady_clock::now();
        const auto now = std::chrono::steady_clock::now();
        parallaxDt = std::chrono::duration<float>(now - s_LastParallaxT).count();
        s_LastParallaxT = now;
        parallaxDt = std::clamp(parallaxDt, 0.0f, 0.05f);
    }

    ImGui::BeginChild("##MainContent", ImVec2(w, h), false);

    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2      wp = ImGui::GetWindowPos();
        RenderBgCanvas(dl, wp, w, h);
        if (bgTex != 0)
            dl->AddImage((ImTextureID)(intptr_t)bgTex, wp, ImVec2(wp.x + w, wp.y + h),
                ImVec2(0,0), ImVec2(1,1), IM_COL32(255,255,255,30));
    }

    const float marginX       = 50.0f;
    const float marginTop     = 40.0f;
    const float spacingX      = 40.0f;
    const float totalWidth    = w - (marginX * 2.0f);
    const float leftColWidth  = totalWidth * 0.55f;
    const float rightColWidth = totalWidth * 0.45f - spacingX;

    ImGui::SetCursorPos(ImVec2(marginX, marginTop));

    // ── Columna izquierda ─────────────────────────────────────────────────────
    ImGui::BeginGroup();

    ImGui::SetWindowFontScale(1.3f);
    ImGui::Text("Actualizaciones");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Dummy(ImVec2(0.0f, 15.0f));

    // Altura del bloque de acciones que va debajo de la lista (boton "Buscar
    // actualizaciones" + "Foro / Soporte"), para poder descontarla del calculo
    // del scroll y que este siempre termine justo antes de dichos botones.
    const float actionsRowH   = 36.0f;
    const float gapBeforeList = ImGui::GetCursorPosY(); // lo ya consumido: titulo + dummy
    const float gapAfterList  = 15.0f;                  // Dummy entre la lista y los botones
    const float bottomMargin  = 55.0f;                  // espacio final, grande, tras los botones

    // Alto restante disponible para la lista scrolleable: ocupa todo lo que
    // sobra hasta el final del panel, dejando lugar para los botones de abajo
    // y un margen inferior comodo.
    const float updatesListH = std::max(
        220.0f,
        h - marginTop - gapBeforeList - gapAfterList - actionsRowH - bottomMargin
    );

    // Contenedor scrolleable para la lista de actualizaciones
    ImGui::BeginChild("##UpdatesList", ImVec2(leftColWidth, updatesListH), false);

    // Función auxiliar para dibujar tarjetas de actualización.
    // Cada tarjeta usa la portada especifica de su propia entrada en el registro,
    // con recorte tipo "cover" + parallax al hover y esquinas redondeadas.
    auto RenderUpdateCard = [&](const UpdateVersionInfo& info) {
        const GLTextureInfo cardCover = GetCoverTexture(info.coverFile);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(24, 24, 29, 230));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::BeginChild(info.version, ImVec2(leftColWidth, 140.0f), false, ImGuiWindowFlags_NoScrollbar);

        ImVec2 cardStartPos = ImGui::GetCursorScreenPos();
        ImVec2 cardEndPos   = ImVec2(cardStartPos.x + leftColWidth, cardStartPos.y + 140.0f);
        const bool cardHovered = ImGui::IsMouseHoveringRect(cardStartPos, cardEndPos);

        ImGui::SetCursorPos(ImVec2(10.0f, 10.0f));
        ImGui::BeginGroup();

        const float thumbW = 180.0f, thumbH = 120.0f;
        if (cardCover.id != 0) {
            const ImVec2 thumbMin = ImGui::GetCursorScreenPos();
            const ImVec2 thumbMax = ImVec2(thumbMin.x + thumbW, thumbMin.y + thumbH);

            char stateKey[96];
            snprintf(stateKey, sizeof(stateKey), "card_%s", info.version);

            DrawCoverImageCover(ImGui::GetWindowDrawList(), cardCover.id, cardCover.width, cardCover.height,
                thumbMin, thumbMax, 10.0f, ImDrawFlags_RoundCornersAll,
                stateKey, parallaxDt, cardHovered, 1.10f);

            ImGui::Dummy(ImVec2(thumbW, thumbH));
            ImGui::SameLine(0.0f, 15.0f);
        }

        ImGui::BeginGroup();
        ImGui::Dummy(ImVec2(0.0f, 5.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, HT::TextMuted);
        ImGui::Text("%s", info.cardBadge);
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(1.1f);
        ImGui::Text("Version v%s", info.version);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, HT::TextMuted);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + leftColWidth - (cardCover.id != 0 ? 220.0f : 30.0f));
        ImGui::TextWrapped("%s", info.summary);
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        ImGui::EndGroup();

        ImGui::EndGroup();

        ImGui::SetCursorScreenPos(cardStartPos);
        if (ImGui::InvisibleButton(info.version, ImVec2(leftColWidth, 140.0f))) {
            selectedUpdateVer = info.id;
            isUpdateModalOpen = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            ImGui::GetWindowDrawList()->AddRectFilled(cardStartPos, cardEndPos,
                IM_COL32(255,255,255,15), 8.0f);
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0.0f, 15.0f)); // Espacio entre tarjetas
    };

    // Renderizamos una tarjeta por cada version registrada, en orden (mas reciente primero)
    for (const auto& info : kUpdateRegistry)
        RenderUpdateCard(info);

    ImGui::EndChild(); // Fin de UpdatesList

    ImGui::Dummy(ImVec2(0.0f, gapAfterList));

    ImGui::BeginGroup();
    ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(42, 42, 50, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(60, 60, 72, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(32, 32, 40, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    if (ImGui::Button("Buscar actualizaciones", ImVec2(180.0f, actionsRowH))) {
        m_ActiveTab = 5; m_OpenSettingsRequested = true;
    }
    ImGui::SameLine(0.0f, 15.0f);
    if (ImGui::Button("Foro / Soporte", ImVec2(180.0f, actionsRowH)))
        ProyecThor::External::OpenURL("https://github.com/TheVixcho/ProyecThor/discussions");
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    ImGui::EndGroup();

    ImGui::EndGroup();

    // ── Columna derecha ───────────────────────────────────────────────────────
    ImGui::SameLine(0.0f, spacingX);
    ImGui::BeginGroup();

    ImGui::EndGroup();
    ImGui::EndChild();

    // ── Modal Universal de Actualizacion ──────────────────────────────────────
    if (isUpdateModalOpen) {
        const UpdateVersionInfo* selInfo = FindUpdateVersion(selectedUpdateVer);
        const GLTextureInfo modalCover = selInfo ? GetCoverTexture(selInfo->coverFile) : GLTextureInfo{};

        ImGuiViewport* vp = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(vp->Pos);
        ImGui::SetNextWindowSize(vp->Size);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 170));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin("##DimOverlay", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        const float modalW = 780.0f, modalH = 660.0f;
        const float headerH = 200.0f, footerH = 62.0f;
        const float modalRounding = 10.0f;

        ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(modalW, modalH), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(28, 31, 36, 255));
        ImGui::PushStyleColor(ImGuiCol_Border,   IM_COL32(60, 65, 75, 200));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   modalRounding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0.0f, 0.0f));

        bool vis = ImGui::Begin("##UpdateModal", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);

        if (vis) {
            ImDrawList* dl   = ImGui::GetWindowDrawList();
            ImVec2      winP = ImGui::GetWindowPos();

            const ImVec2 headerMin = winP;
            const ImVec2 headerMax = ImVec2(winP.x + modalW, winP.y + headerH);

            if (modalCover.id != 0) {
                char stateKey[96];
                snprintf(stateKey, sizeof(stateKey), "modal_%s", selInfo ? selInfo->version : "none");
                const bool headerHovered = ImGui::IsMouseHoveringRect(headerMin, headerMax);

                // Solo se redondean las esquinas superiores: coinciden con el
                // borde del modal, mientras que abajo continua el contenido.
                DrawCoverImageCover(dl, modalCover.id, modalCover.width, modalCover.height,
                    headerMin, headerMax, modalRounding, ImDrawFlags_RoundCornersTop,
                    stateKey, parallaxDt, headerHovered, 1.06f);
                ImGui::Dummy(ImVec2(modalW, headerH));
            } else {
                dl->AddRectFilled(headerMin, headerMax, IM_COL32(15,15,15,255),
                    modalRounding, ImDrawFlags_RoundCornersTop);
                ImGui::Dummy(ImVec2(modalW, headerH));
            }

            dl->AddRectFilledMultiColor(
                ImVec2(winP.x, winP.y+headerH-60), ImVec2(winP.x+modalW, winP.y+headerH),
                IM_COL32(0,0,0,0), IM_COL32(0,0,0,0),
                IM_COL32(28,31,36,255), IM_COL32(28,31,36,255));

            ImGui::SetCursorPos(ImVec2(0, headerH));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0,0,0,0));
            ImGui::BeginChild("##ModalScroll", ImVec2(modalW, modalH-headerH-footerH), false);

            const float mg = 36.0f, cw = modalW - mg*2;
            ImGui::SetCursorPos(ImVec2(mg, 18.0f));
            ImGui::BeginGroup();

            {
                ImVec2 bp = ImGui::GetCursorScreenPos();
                const char* bt = selInfo ? selInfo->modalBadge : " ACTUALIZACION ";
                ImVec2 bs = ImGui::CalcTextSize(bt);
                ImGui::GetWindowDrawList()->AddRectFilled(
                    ImVec2(bp.x-1, bp.y-2), ImVec2(bp.x+bs.x+1, bp.y+bs.y+2),
                    IM_COL32(35,116,225,200), 3.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255,255,255,255));
                ImGui::SetWindowFontScale(0.8f); ImGui::Text("%s", bt); ImGui::SetWindowFontScale(1.0f);
                ImGui::PopStyleColor();
                ImGui::SameLine(0,40);
            }
            ImGui::SetWindowFontScale(0.8f);
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100,108,118,255));
            ImGui::Text("HISTORIAL DE VERSIONES");
            ImGui::PopStyleColor();
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Dummy(ImVec2(0,6));

            ImGui::SetWindowFontScale(1.7f);
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(240,242,245,255));
            ImGui::Text("Actualizacion v%s", selInfo ? selInfo->version : "?");
            ImGui::PopStyleColor();
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Dummy(ImVec2(0,20));

            auto Cat = [&](const char* t) {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(210,215,220,255));
                ImGui::SetWindowFontScale(1.05f); ImGui::Text("%s",t); ImGui::SetWindowFontScale(1.0f);
                ImGui::PopStyleColor();
                ImVec2 p = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddLine(
                    ImVec2(p.x,p.y+1), ImVec2(p.x+cw,p.y+1), IM_COL32(255,255,255,18));
                ImGui::Dummy(ImVec2(0,10));
            };
            auto Bul = [&](const char* t) {
                ImVec2 bp = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddCircleFilled(
                    ImVec2(bp.x+6, bp.y+ImGui::GetTextLineHeight()*0.5f), 2.5f, IM_COL32(75,130,200,220));
                ImGui::SetCursorPosX(ImGui::GetCursorPosX()+18);
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(185,190,198,255));
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+cw-22);
                ImGui::TextWrapped("%s",t);
                ImGui::PopTextWrapPos(); ImGui::PopStyleColor();
                ImGui::Dummy(ImVec2(0,4));
            };
            // ── Bloque de contenido condicional por versión ──────────────────
           if (selectedUpdateVer == 3) { // v0.3.2
    Cat("Soporte para Linux");
    Bul("ProyecThor ahora corre de forma nativa en Linux, con build propio via CMake.");
    Bul("Pruebas realizadas en Arch Linux (y derivados como CachyOS), incluyendo el flujo completo de instalacion via paquete.");
    Bul("Deteccion y manejo del backend X11/XWayland para compatibilidad con GLEW en sesiones Wayland.");
    Bul("Rutas de configuracion y assets ahora siguen la convencion XDG en Linux ($XDG_CONFIG_HOME o ~/.config), en vez de asumir rutas de Windows.");
    ImGui::Dummy(ImVec2(0,12));

    Cat("Motor de Video (VLC) y Rendimiento");
    Bul("Correccion de un problema de rendimiento que afectaba la reproduccion fluida de video en ciertos escenarios.");
    Bul("Cambios en la logica interna de manejo del motor VLC para mejorar la estabilidad de la reproduccion.");
    Bul("Ajustes en la forma en que se inicializan y liberan los recursos del reproductor.");
    Bul("Correcciones relacionadas con la sincronizacion del motor multimedia y el bloqueo/desbloqueo de rutas al eliminar archivos en uso.");
    ImGui::Dummy(ImVec2(0,12));

    Cat("Cola de Reproduccion");
    Bul("Mejoras de estabilidad en la cola: avance mas confiable entre clips y manejo correcto de entradas invalidas o eliminadas.");
    Bul("Correccion de condiciones donde la cola podia quedar desincronizada con lo que realmente se estaba proyectando.");
    ImGui::Dummy(ImVec2(0,12));

    Cat("Ajustes y Configuracion");
    Bul("Corregido un problema donde los ajustes de la aplicacion no se guardaban o cargaban correctamente entre sesiones.");
    Bul("Mayor consistencia al persistir preferencias del usuario, incluyendo configuracion de proyeccion y monitor.");
    ImGui::Dummy(ImVec2(0,12));

    Cat("Estilos y Personalizacion");
    Bul("Nuevos estilos visuales disponibles para personalizar la apariencia de la aplicacion.");
    Bul("Ajustes de consistencia visual entre paneles.");
    ImGui::Dummy(ImVec2(0,12));

    Cat("Red y Transmision LAN");
    Bul("Mejoras de estabilidad en la transmision por red local, reduciendo cortes y desconexiones.");
    Bul("Correcciones en la sincronizacion entre el estado de la aplicacion y los clientes conectados por LAN.");
    ImGui::Dummy(ImVec2(0,12));

    Cat("Limpieza de Codigo");
    Bul("Refactorizacion y limpieza general del codigo base, sin cambios visibles para el usuario.");
    Bul("Eliminacion de codigo obsoleto y simplificacion de varias rutinas internas.");
    Bul("Mejoras de mantenibilidad para facilitar el desarrollo de futuras versiones.");
            } else if (selectedUpdateVer == 1) { // v0.3.1
                Cat("Audio Rework");
                Bul("Nueva interfaz para la seccion de audio, con animaciones renovadas y un sistema de portadas (covers) para cada pista.");
                Bul("Diseno mas versatil, adaptable e intuitivo.");
                Bul("Nuevo ecualizador (EQ), control de ganancia y cola de reproduccion.");
                Bul("Ahora es posible asignar autores a las canciones.");
                Bul("Mejoras visuales en los iconos de la cola de reproduccion.");
                Bul("Correccion: el dispositivo de audio ahora se abre una unica vez por reproductor; los cambios de pista solo reinician la cola en lugar de renegociar el hardware, reduciendo cortes y mejorando la fluidez.");
                Bul("Reemplazo del modelo de hilos de reproduccion por un unico hilo de trabajo persistente con cola de solicitudes protegida, eliminando condiciones de carrera al cerrar el reproductor.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Rendimiento y Optimizacion");
                Bul("Nuevas funciones para mejorar y hacer mas eficiente la gestion de memoria general del programa.");
                Bul("Optimizacion en la reproduccion de video del motor multimedia.");
                Bul("Solucionado un problema critico que hacia que el panel de transiciones tomara prioridad de forma incorrecta.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Reproduccion y Previsualizacion");
                Bul("Separacion completa entre el reproductor de previsualizacion (biblioteca) y el reproductor del monitor en vivo: cada uno cuenta con su propio decodificador, salida de audio y textura, evitando que la navegacion por la biblioteca afecte lo que se esta proyectando.");
                Bul("Mejor manejo general de hilos en el sistema de reproduccion.");
                Bul("Solucionado el problema de pantallas negras en el monitor secundario.");
                Bul("El monitor ahora respeta las proporciones adecuadas de la pantalla.");
                Bul("El reproductor de video ahora se inicializa por defecto en modo estirado.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Sistema y Personalizacion");
                Bul("Los ajustes y configuraciones ahora se guardan y persisten correctamente entre sesiones.");
                Bul("Nueva capacidad de personalizacion profunda: seleccion de idioma, apariencia (incluye colores de la aplicacion) y ajustes varios.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Interfaz y Experiencia (UX/UI)");
                Bul("Cambio total del logo oficial del programa.");
                Bul("Rework completo de la pantalla de carga inicial, con nuevas ilustraciones.");
                Bul("Nuevas animaciones y transiciones mas fluidas en el Hub principal.");
                Bul("Iconografia renovada en las secciones de Monitor y Reproductor de Video, junto con mejoras visuales en la Libreria.");
                Bul("Rework visual y estructural de la lista de reproduccion de videos.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Soporte y Comunidad");
                Bul("Ahora contamos con un canal oficial de comunicacion y soporte en WhatsApp y Discord.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Conocido / En desarrollo");
                Bul("Sincronizacion entre la previsualizacion y los controles de play/pause: en revision.");
                Bul("Transmision de fuentes personalizadas a traves de la red LAN: planificada para una proxima actualizacion.");
                Bul("Sistema de reproduccion con varianza inteligente que evita repeticiones consecutivas: en desarrollo.");
            } else { // v0.3.0
                Cat("General");
                Bul("Hub de administracion centralizado para gestionar la aplicacion de forma integral.");
                Bul("Generacion automatica de codigo QR para visualizar la transmision desde dispositivos moviles.");
                Bul("Nuevos splash screen al iniciar.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Multimedia y Streaming");
                Bul("Mejoras en transmision LAN, creacion de servidor y sincronizacion de clientes.");
                Bul("Estilos predeterminados de letras por categoria de lista.");
                Bul("Optimizacion del motor VLC para reproduccion de video mas fluida.");
                Bul("Nueva opcion para transmitir fondos con orientacion corregida.");
                Bul("Mejoras de rendimiento en biblioteca y area de previsualizacion.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Soporte y Estabilidad");
                Bul("Mejor manejo de rutas y mayor estabilidad general.");
                Bul("Edicion de canciones sin perdida de foco en pantalla.");
                Bul("Correccion en cola de reproduccion y transiciones de vistas.");
                Bul("Multiples correcciones de estabilidad y prevencion de cuelgues.");
            }

            ImGui::Dummy(ImVec2(0,24));
            ImGui::EndGroup();
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::SetCursorPos(ImVec2(0, modalH-footerH));
            ImVec2 flp = ImGui::GetCursorScreenPos();
            dl->AddLine(ImVec2(flp.x,flp.y), ImVec2(flp.x+modalW,flp.y), IM_COL32(50,55,65,255), 1.0f);

            const float bw=130, bh=34;
            ImGui::SetCursorPos(ImVec2((modalW-bw)*0.5f, (modalH-footerH)+(footerH-bh)*0.5f));
            ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(55,60,72,255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(72,78,92,255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(42,46,56,255));
            ImGui::PushStyleColor(ImGuiCol_Text,          IM_COL32(235,235,235,255));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
            if (ImGui::Button("Cerrar", ImVec2(bw, bh)))
                isUpdateModalOpen = false;
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
        }

        ImGui::End();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
    }
}

} // namespace ProyecThor::UI