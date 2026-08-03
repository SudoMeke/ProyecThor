#include "Hub.h"
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
#include "SongPlayStats.h"

extern GLuint LoadTextureFromFile(const char* filename);

static void RenderSplashScreen(GLFWwindow *splashWindow, const std::string &status, float progress, GLuint logoTexture, GLuint bgTexture, ImFont *titleFont, ImFont *regularFont, ImFont *smallFont, const std::string &creditText, const ProyecThor::Settings::ThemeSettings &theme);

static constexpr float HUB_SIDEBAR_W  = 280.0f;
static constexpr float HUB_APPEAR_SPD = 3.0f;

namespace DS = ProyecThor::UI::DS;
namespace HT = ProyecThor::UI::HubTheme;

namespace ProyecThor::UI {

// Reemplaza el canal alfa de un color existente, preservando su tinte
// (RGB). Se usa para reutilizar los colores derivados del tema (HT::*)
// con las intensidades variables que antes usaban IM_COL32 hardcodeado.
static ImU32 ColA(ImU32 col, int alpha) {
    alpha = std::clamp(alpha, 0, 255);
    return (col & 0x00FFFFFFu) | (static_cast<ImU32>(alpha) << IM_COL32_A_SHIFT);
}
static ImU32 ColAf(ImU32 col, float alpha01) {
    return ColA(col, static_cast<int>(std::clamp(alpha01, 0.0f, 1.0f) * 255.0f));
}

// Mismo patron que LPHoverLerp (src/frontend/panels/layers/LayersTheme.h):
// anima un 0..1 suavizado entre frames usando el ImGuiStorage del contexto
// actual en vez de floats miembro. No se puede incluir LayersTheme.h desde
// frontend/ui (evita la dependencia cruzada con frontend/panels/layers,
// mismo motivo documentado en DesignSystem.cpp), asi que se replica local.
static float HubHoverLerp(ImGuiID id, bool hovered, float speed = 12.0f) {
    ImGuiStorage* storage = ImGui::GetStateStorage();
    float* pT = storage->GetFloatRef(id ^ 0x48554248u, 0.0f); // salt "HUB H"
    const float target = hovered ? 1.0f : 0.0f;
    *pT += (target - *pT) * std::min(1.0f, ImGui::GetIO().DeltaTime * speed);
    return *pT;
}

// ── Registro de versiones y portadas ────────────────────────────────────────
//  Cada entrada define su propia imagen de portada, de forma que agregar una
//  nueva actualizacion con una foto distinta sea tan simple como anadir una
//  linea aquí. "id" es el mismo valor que usa el modal (selectedUpdateVer).
// ─────────────────────────────────────────────────────────────────────────
struct UpdateVersionInfo {
    int         id;         // Identificador interno (coincide con selectedUpdateVer)
    const char* version;    // "0.3.1"
    const char* modalBadge; // Texto de insignia mostrado dentro del modal
    const char* cardBadge;  // Texto de insignia mostrado en la tarjeta de la lista
    const char* coverFile;  // Imagen de portada especifica de esta version
    const char* summary;    // Resumen corto mostrado en la tarjeta
};

static const std::vector<UpdateVersionInfo> kUpdateRegistry = {
    {
        12, "0.5.1",
        "ACTUALIZACIÓN", "ACTUALIZACIÓN",
        "splash_bg5.png",  // TODO: reemplazar por portada propia cuando este lista
        "Reloj y Contadores ahora es solo \"Contadores\". Nuevo cuadro de reloj dentro del "
        "editor de Overlays: lo posicionas y le das estilo una sola vez, y se reemplaza en vivo "
        "por la hora/cronómetro activo — la transmisión a pantalla ahora depende de que overlay "
        "tengas activo, en vez de un modo aparte. Overlays con reordenar capas y overlays de "
        "reloj predeterminados listos para probar. Corregido un bug por el cual el cuadriculado "
        "de \"sin fondo\" del editor de Overlays podia quedar horneado como fondo opaco al "
        "guardar."
    },
    {
        11, "0.5.0",
        "GRAN ACTUALIZACIÓN", "GRAN ACTUALIZACIÓN",
        "splash_bg5.png",  // TODO: reemplazar por portada propia cuando este lista
        "Ajustes reorganizado por completo: cada configuración ahora es su propia página, con "
        "buscador incluido, Proyección y Pantallas agrupadas juntas, y Red/Mobile/Streaming/OSC "
        "viviendo dentro de Proyección. Nueva opción \"Bucle falso\" para Fondos, que reproduce "
        "hacia adelante y hacia atras en vez de cortar siempre al mismo frame. Nueva sección de "
        "Overlays: crea textos, formas e imágenes en un editor a pantalla completa y proyectalos "
        "como una capa transparente encima del fondo y la letra (antes tapaban el fondo por "
        "error). Vista en Vivo renovada: reproductor más simple, Overlays/Chat/Pads/Reloj ahora "
        "se abren dentro del mismo panel en vez de ventanas flotantes sueltas. Corregidos varios "
        "colores que quedaban fijos sin importar el tema elegido y los fondos de los paneles "
        "ahora son solidos en vez de verse transparentes."
    },
    {
        10, "0.4.3",
        "ACTUALIZACIÓN PREELIMINAR", "ACTUALIZACIÓN PREELIMINAR",
        "bg_splash3.png",  // TODO: reemplazar por portada propia cuando este lista
        "Nueva sección Conexiones (OSC, Red, Chat y Streaming en vivo por RTMP), nueva "
        "Biblioteca para gestionar tus archivos con conversor de formato incluido, "
        "Biblia a pantalla completa, selector rápido (Alt+Espacio), Monitor de Vista "
        "en Vivo más compacto, editor de Estilos renovado, nuevo instalador para "
        "Windows, Biblioteca con Biblias y canción de bienvenida incluidas de entrada, "
        "corregido el título de las canciones al guardarlas, y varias correcciones de "
        "estabilidad."
    },
    {
        9, "0.4.2",
        "ACTUALIZACIÓN PREELIMINAR", "ACTUALIZACIÓN PREELIMINAR",
        "bg_splash3.png",  // TODO: reemplazar por portada propia cuando este lista
        "Pads de Vista en Vivo arreglados y renovados con escenas de Captura sincronizadas, "
        "transporte y volumen rediseñados tipo consola/MIDI, buscador de versiculos por "
        "palabras en la Biblia, editor de Estilos acoplado dentro de Home con selector de "
        "fuentes en grilla y nuevos efectos de texto (fondo, borde, sombra, glow, neon, "
        "subrayado), y un monton de efectos nuevos en Shaders: NIS (NVIDIA), VHS, Cine, "
        "Contraste, Luminosidad, Blur, Sharpen, Bloom, Aberración cromática y TAA."
    },
    {
        8, "0.4.1",
        "ACTUALIZACIÓN", "ACTUALIZACIÓN",
        "bg_splash3.png",
        "Nuevo panel de Shaders (FSR, CRT, grano, saturación, vinetado y "
        "relleno desenfocado tipo Smart TV) para el video de fondo, miniaturas "
        "y vista en grilla/lista en Biblioteca > Videos, escenas rápidas "
        "guardadas para Captura, fuente de interfaz personalizable, un "
        "motor de renderizado alternativo (libvlc en ventana nativa) para "
        "videos, editor de canciones rediseñado por completo y menu "
        "principal reorganizado, con una corrección importante de "
        "sincronización de audio/video en equipos de bajos recursos."
    },
    {
        7, "0.4.0",
        "GRAN ACTUALIZACIÓN", "GRAN ACTUALIZACIÓN",
        "bg_splash3.png",  // TODO: reemplazar por portada propia cuando este lista
        "Cola de videos mucho más estable, nueva sección de Overlays, "
        "Vista en Vivo con acciones rápidas, panel de Rendimiento y un "
        "rediseño más compacto de Fondos y Estilos."
    },
    {
        6, "0.3.5",
        "ACTUALIZACIÓN PREELIMINAR", "ACTUALIZACIÓN PREELIMINAR",
        "splash_bg1.png",  // TODO: reemplazar por portada propia cuando este lista
        "Versión estable: Audio Rework completo, biblioteca renovada con sistema de "
        "etiquetas, soporte oficial para Linux, estadisticas locales, atajos de "
        "teclado globales y mejoras de estabilidad en toda la aplicacion."
    },
    {
        2, "0.3.0",
        "GRAN ACTUALIZACIÓN", "GRAN ACTUALIZACIÓN",
        "splash_bg1.png",
        "Nuevas herramientas de transmisión, optimizaciones y estabilidad de red."
    },
};

static const UpdateVersionInfo* FindUpdateVersion(int id) {
    for (const auto& v : kUpdateRegistry)
        if (v.id == id) return &v;
    return kUpdateRegistry.empty() ? nullptr : &kUpdateRegistry[0];
}

struct GLTextureInfo {
    GLuint id     = 0;
    int    width  = 0;
    int    height = 0;
};

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
        dl->AddRectFilled(pMin, pMax, ColA(HT::CardAlt, 255), rounding, roundFlags);
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

// Carrusel de novedades — se muestra una vez por version nueva.
void Hub::RenderWhatsNewIfNeeded() {
    auto& general = ProyecThor::Settings::SettingsManager::Get().GetSettings().general;
    if (general.dismissedChangelog == PROYECTHOR_VERSION_STRING) return;

    struct Slide { const char* title; const char* body; };
    static const Slide kSlides[] = {
        { "Bienvenido a ProyecThor v" PROYECTHOR_VERSION_STRING,
          "Este es un resumen rápido de lo nuevo en esta versión. Recorrelo con los botones o los puntos de abajo." },
        { "Ajustes reorganizado",
          "Cada configuración ahora es su propia página, con buscador incluido. Proyección y Pantallas quedaron agrupadas juntas, y Red, Mobile, Streaming y OSC pasaron a vivir dentro de Proyección en vez de tener su propia categoría aparte." },
        { "Fondos: bucle falso",
          "Nueva opción en Ajustes > Proyección > Fondos: el video reproduce hacia adelante y despues \"hacia atras\" en vez de cortar siempre al mismo frame, dando sensacion de bucle continuo." },
        { "Overlays",
          "Crea textos, formas e imágenes en un editor a pantalla completa y proyectalos como una capa transparente encima del fondo y la letra, desde Biblioteca > Overlay o directo desde Vista en Vivo." },
        { "Vista en Vivo renovada",
          "Reproductor más simple: Overlays, Chat, Pads y Reloj ahora se abren dentro del mismo panel en vez de ventanas flotantes sueltas." },
        { "Nueva sección: Pantallas",
          "La configuración de Stage ahora tiene su propio menu \"Pantallas\" arriba de todo, en vez de estar mezclada con Proyección." },
        { "Correcciones de tema y apariencia",
          "Varios menus y ventanas que ignoraban el tema elegido ahora lo respetan, y los fondos de los paneles son solidos en vez de verse transparentes." },
    };
    constexpr int kSlideCount = (int)(sizeof(kSlides) / sizeof(kSlides[0]));

    static int  s_Index         = 0;
    static bool s_OpenedOnce    = false;
    // Desmarcado por default: si el operador cierra sin marcarlo, el
    // carrusel vuelve a aparecer en el proximo arranque (dismissedChangelog
    // NO se persiste). Solo marcando la casilla se guarda la version actual
    // en dismissedChangelog y deja de mostrarse.
    static bool s_DontShowAgain = false;
    if (!s_OpenedOnce) {
        ImGui::OpenPopup("##WhatsNewCarousel");
        s_OpenedOnce     = true;
        s_Index          = 0;
        s_DontShowAgain  = false;
    }

    ImGuiViewport* vp      = ImGui::GetMainViewport();
    const ImVec2   winSize = ImVec2(580.0f, 434.0f);
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + (vp->WorkSize.x - winSize.x) * 0.5f,
                                    vp->WorkPos.y + (vp->WorkSize.y - winSize.y) * 0.5f));
    ImGui::SetNextWindowSize(winSize);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(30.0f, 28.0f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.055f, 0.060f, 0.085f, 0.99f));

    if (ImGui::BeginPopupModal("##WhatsNewCarousel", nullptr,
                               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.75f, 0.30f, 1.0f));
        ImGui::TextUnformatted("NOVEDADES");
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Spacing();

        const Slide& slide = kSlides[s_Index];

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.92f, 0.93f, 0.97f, 1.0f));
        ImGui::SetWindowFontScale(1.18f);
        ImGui::TextWrapped("%s", slide.title);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.74f, 0.85f, 1.0f));
        ImGui::TextWrapped("%s", slide.body);
        ImGui::PopStyleColor();

        ImGui::SetCursorPosY(winSize.y - 130.0f);
        float dotsW = kSlideCount * 16.0f;
        ImGui::SetCursorPosX((winSize.x - dotsW) * 0.5f);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2      dp = ImGui::GetCursorScreenPos();
        for (int i = 0; i < kSlideCount; i++) {
            ImU32 col = (i == s_Index) ? IM_COL32(120, 150, 255, 255) : IM_COL32(70, 72, 90, 255);
            dl->AddCircleFilled(ImVec2(dp.x + i * 16.0f + 5.0f, dp.y + 5.0f), 5.0f, col);
        }
        ImGui::Dummy(ImVec2(dotsW, 14.0f));

        ImGui::SetCursorPosY(winSize.y - 96.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.62f, 0.64f, 0.76f, 1.0f));
        ImGui::Checkbox("No volver a mostrar", &s_DontShowAgain);
        ImGui::PopStyleColor();

        ImGui::SetCursorPosY(winSize.y - 60.0f);

        if (ImGui::Button("Configuración inicial", ImVec2(170, 34))) {
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Proximamente");

        ImGui::SameLine();
        if (s_Index == 0) ImGui::BeginDisabled();
        if (ImGui::Button("< Anterior", ImVec2(100, 34))) s_Index--;
        if (s_Index == 0) ImGui::EndDisabled();

        ImGui::SameLine();
        // "No volver a mostrar" sin marcar (default): dismissedChangelog NO
        // se toca, asi que el carrusel vuelve a aparecer en el proximo
        // arranque -- cerrar (con cualquiera de los dos botones) solo lo
        // saca de la vista por esta sesion.
        auto closeCarousel = [&]() {
            if (s_DontShowAgain) {
                general.dismissedChangelog = PROYECTHOR_VERSION_STRING;
                ProyecThor::Settings::SettingsManager::Get().Save();
            }
            ImGui::CloseCurrentPopup();
        };

        if (s_Index == kSlideCount - 1) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.66f, 0.40f, 1.0f));
            if (ImGui::Button("Entendido", ImVec2(110, 34)))
                closeCarousel();
            ImGui::PopStyleColor();
        } else {
            if (ImGui::Button("Siguiente >", ImVec2(110, 34))) s_Index++;
        }

        ImGui::SameLine();
        if (ImGui::Button("Cerrar", ImVec2(70, 34)))
            closeCarousel();

        ImGui::EndPopup();
    }

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

bool Hub::Render() {
    if (!m_Open) return false;

    const auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - m_LastFrameTime).count();
    m_LastFrameTime = now;
    dt = std::min(dt, 0.05f);

    m_Time += dt;

    static int fpsFrames = 0;
    static float fpsAccum = 0.0f;
    fpsFrames++;
    fpsAccum += dt;
    if (fpsAccum >= 0.5f) {
        const int fps = std::max(1, static_cast<int>(fpsFrames / fpsAccum));
        ProyecThor::UI::RecordPerformanceSample(fps);
        fpsFrames = 0;
        fpsAccum = 0.0f;
    }

    UpdateAnimations(dt);

    ImGuiViewport* vp = ImGui::GetMainViewport();

    UpdateBgParticles(dt, vp->WorkSize.x - HUB_SIDEBAR_W, vp->WorkSize.y);
    UpdateNebulas(dt, vp->WorkSize.x - HUB_SIDEBAR_W, vp->WorkSize.y);

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

    // Fade-in real al abrir el Hub: m_AppearProgress ya se calculaba en
    // UpdateAnimations pero antes no se usaba en ningun lado.
    const float appearA = EaseOut(m_AppearProgress);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, appearA);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2      wp = ImGui::GetWindowPos();

    dl->AddRectFilled(wp,
        ImVec2(wp.x + HUB_SIDEBAR_W, wp.y + vp->WorkSize.y), ColAf(HT::BgSidebar, appearA));
    dl->AddRectFilled(
        ImVec2(wp.x + HUB_SIDEBAR_W, wp.y),
        ImVec2(wp.x + vp->WorkSize.x, wp.y + vp->WorkSize.y), ColAf(HT::BgMain, appearA));

    dl->AddLine(
        ImVec2(wp.x + HUB_SIDEBAR_W, wp.y),
        ImVec2(wp.x + HUB_SIDEBAR_W, wp.y + vp->WorkSize.y),
        ColAf(HT::Divider, appearA), 1.0f);

    RenderSidebar(HUB_SIDEBAR_W, vp->WorkSize.y);
    ImGui::SameLine(0.0f, 0.0f);
    RenderMainContent(vp->WorkSize.x - HUB_SIDEBAR_W, vp->WorkSize.y);

    ImGui::PopStyleVar(); // Alpha
    ImGui::End();
    ImGui::PopStyleVar(2);

    RenderWhatsNewIfNeeded();

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
                    ColA(HT::AccentSoft, glowAlpha), "Thor");
            }
        }
        for (int ox = -1; ox <= 1; ox++) {
            for (int oy = -1; oy <= 1; oy++) {
                if (ox == 0 && oy == 0) continue;
                dl->AddText(font, logoFontSize,
                    ImVec2(posThor.x + static_cast<float>(ox),
                           posThor.y + static_cast<float>(oy)),
                    ColA(HT::AccentSoft, 35), "Thor");
            }
        }

        dl->AddText(font, logoFontSize, posProyec, HT::TextPri, "Proyec");
        dl->AddText(font, logoFontSize, posThor,   HT::AccentSoft, "Thor");

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
    ImGui::PushStyleColor(ImGuiCol_Text, HT::OnAccent);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, HT::RadiusMd);

    if (ImGui::Button("Empezar a proyectar", ImVec2(w - 60.0f, 45.0f)))
        m_LaunchRequested = true;

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    ImGui::SetCursorPosX(30.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        HT::Surface);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HT::SurfaceHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  HT::SurfaceActive);
    ImGui::PushStyleColor(ImGuiCol_Text,          HT::TextPri);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, HT::RadiusMd);

    if (ImGui::Button("Abrir configuración", ImVec2(w - 60.0f, 36.0f)))
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
    ImGui::TextUnformatted("Accesos rápidos");
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    auto QuickBtn = [&](const char* icon, const char* label, int settingsTab) {
        ImGui::SetCursorPosX(30.0f);
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HT::SurfaceHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  HT::SurfaceActive);
        ImGui::PushStyleColor(ImGuiCol_Text,          HT::TextPri);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, HT::RadiusSm);

        char id[64];
        snprintf(id, sizeof(id), "%s  %s##qb%d", icon, label, settingsTab);

        if (ImGui::Button(id, ImVec2(w - 60.0f, 32.0f))) {
            m_ActiveTab             = settingsTab;
            m_OpenSettingsRequested = true;
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
    };

    // Indices de k_Categories en SettingsPanel.cpp (0=Apariencia,
    // 1=Proyeccion, 2=Stage, 3=Audio, 4=Canciones, 5=Teclas, 6=Idioma,
    // 7=Actualizaciones). "General" se quito del todo (pedido explicito, no
    // se usaba), de ahi que ya no aparezca aca.
    QuickBtn("", "Apariencia",      0);
    QuickBtn("", "Proyección",      1);
    QuickBtn("", "Stage",           2);
    QuickBtn("", "Idioma",          6);
    QuickBtn("", "Actualizaciones", 7);

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

void Hub::InitNebulas(float w, float h) {
    std::mt19937 rng(static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(m_Nebulas.data()) ^ 0x9E3779B9u));
    auto frand = [&](float lo, float hi) -> float {
        return lo + (hi - lo) * (static_cast<float>(rng()) / static_cast<float>(rng.max()));
    };
    for (auto& n : m_Nebulas) {
        n.x  = frand(0.0f, w);
        n.y  = frand(0.0f, h);
        n.r  = frand(160.0f, 320.0f);
        n.vx = frand(-0.04f, 0.04f);
        n.vy = frand(-0.03f, 0.03f);
    }
    m_NebulasInit = true;
}

void Hub::UpdateNebulas(float dt, float w, float h) {
    for (auto& n : m_Nebulas) {
        n.x += n.vx * dt * 60.0f;
        n.y += n.vy * dt * 60.0f;
        if (n.x < -n.r) n.x = w + n.r;
        if (n.x > w + n.r) n.x = -n.r;
        if (n.y < -n.r) n.y = h + n.r;
        if (n.y > h + n.r) n.y = -n.r;
    }
}

void Hub::RenderBgCanvas(ImDrawList* dl, ImVec2 origin, float w, float h) {
    using ProyecThor::Settings::ThemePreset;
    const bool isGalaxy = ProyecThor::Settings::SettingsManager::Get().GetSettings().theme.preset
        == ThemePreset::Galaxy;

    // ── Nebulosas (solo Galaxia) — se dibujan primero, detras de todo ────────
    if (isGalaxy) {
        if (!m_NebulasInit) InitNebulas(w, h);

        ImVec4 accentV = ImGui::ColorConvertU32ToFloat4(HT::AccentBlue);
        ImVec4 softV   = ImGui::ColorConvertU32ToFloat4(HT::AccentSoft);
        for (int i = 0; i < NEBULA_COUNT; i++) {
            const auto& n     = m_Nebulas[i];
            const ImVec4& tint = (i % 2 == 0) ? accentV : softV;
            // Varios circulos concentricos con alpha decreciente = glow suave
            // sin textura ni assets externos (mismo truco que DrawSoftShadow).
            for (int layer = 4; layer >= 1; layer--) {
                float t     = (float)layer / 4.0f;
                float rad   = n.r * t;
                float alpha = 0.030f * (1.0f - t * 0.6f);
                dl->AddCircleFilled(ImVec2(origin.x + n.x, origin.y + n.y), rad,
                    ImGui::ColorConvertFloat4ToU32(ImVec4(tint.x, tint.y, tint.z, alpha)), 40);
            }
        }
    }

    // Grilla: solo en los temas "normales" — en Galaxia una cuadricula
    // geometrica desentona con el look de nebulosa/estrellas.
    if (!isGalaxy) {
        // Tinte muy tenue del color de texto primario: se ve sutil tanto en
        // temas oscuros (linea clara) como claros (linea oscura).
        const ImU32 gridCol = ColA(HT::TextPri, 6);
        for (float x = 0.0f; x < w; x += BG_GRID_SIZE)
            dl->AddLine(ImVec2(origin.x + x, origin.y), ImVec2(origin.x + x, origin.y + h), gridCol, 0.5f);
        for (float y = 0.0f; y < h; y += BG_GRID_SIZE)
            dl->AddLine(ImVec2(origin.x, origin.y + y), ImVec2(origin.x + w, origin.y + y), gridCol, 0.5f);
    }

    // ── Particulas — en Galaxia se ven como estrellas: mas grandes, con mas
    //    contraste de brillo (centelleo) y un halo suave en el pico del
    //    "parpadeo" en vez del punto chico y parejo de las demas paletas.
    for (const auto& p : m_BgParticles) {
        const float sinVal = sinf(m_Time * 0.75f + p.phase);
        const float alpha  = isGalaxy ? (0.32f + 0.34f * sinVal) : (0.18f + 0.14f * sinVal);
        const ImU32 col    = p.isCyan
            ? ColAf(HT::ParticleA, alpha)
            : ColAf(HT::ParticleB, alpha);
        const float r = isGalaxy ? p.r * 1.5f : p.r;
        const ImVec2 pos = ImVec2(origin.x + p.x, origin.y + p.y);
        dl->AddCircleFilled(pos, r, col, 8);

        if (isGalaxy && sinVal > 0.80f) {
            const float haloAlpha = (sinVal - 0.80f) * 0.9f;
            dl->AddCircleFilled(pos, r * 3.2f,
                ColAf(p.isCyan ? HT::ParticleA : HT::ParticleB, haloAlpha * 0.20f), 12);
        }
    }

    // Lineas de conexión tipo "red/constelación": se sacan en Galaxia (se ve
    // mas a cableado de red que a cielo estrellado sin ellas).
    if (!isGalaxy) {
        for (int i = 0; i < BG_PARTICLE_COUNT; i++) {
            for (int j = i + 1; j < BG_PARTICLE_COUNT; j++) {
                const float dx   = m_BgParticles[i].x - m_BgParticles[j].x;
                const float dy   = m_BgParticles[i].y - m_BgParticles[j].y;
                const float dist = sqrtf(dx * dx + dy * dy);
                if (dist < BG_CONNECT_DIST) {
                    const float t       = 1.0f - (dist / BG_CONNECT_DIST);
                    const float alpha   = t * t * 0.09f;
                    const ImU32 lineCol = ColAf(HT::ParticleB, alpha);
                    dl->AddLine(
                        ImVec2(origin.x + m_BgParticles[i].x, origin.y + m_BgParticles[i].y),
                        ImVec2(origin.x + m_BgParticles[j].x, origin.y + m_BgParticles[j].y),
                        lineCol, 0.5f);
                }
            }
        }
    }
}

void Hub::RenderMainContent(float w, float h) {
    static GLuint bgTex             = 0;
    static bool   texLoaded         = false;
    static bool   isUpdateModalOpen = false;
    static int    selectedUpdateVer = 12; // id de kUpdateRegistry (12 = v0.5.1, la mas reciente)

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
                ImVec2(0,0), ImVec2(1,1), ColAf(IM_COL32_WHITE, HT::BgImageAlpha));
    }

    const float marginX       = 50.0f;
    const float marginTop     = 40.0f;
    const float spacingX      = 40.0f;
    const float totalWidth    = w - (marginX * 2.0f);
    const float leftColWidth  = totalWidth * 0.55f;
    const float rightColWidth = totalWidth * 0.45f - spacingX;

    ImGui::SetCursorPos(ImVec2(marginX, marginTop));

    // Encabezado de sección con una linea sutil debajo (mismo estilo "Cat()"
    // que ya usa el modal de actualizacion), para dar jerarquia visual
    // consistente entre ambas columnas.
    auto SectionHeader = [&](const char* title, float width) {
        ImGui::SetWindowFontScale(1.3f);
        ImGui::PushStyleColor(ImGuiCol_Text, HT::TextPri);
        ImGui::Text("%s", title);
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(1.0f);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddLine(ImVec2(p.x, p.y + 2.0f), ImVec2(p.x + width, p.y + 2.0f), HT::BorderFaint);
        ImGui::Dummy(ImVec2(0.0f, 13.0f));
    };

    // ── Columna izquierda ─────────────────────────────────────────────────────
    ImGui::BeginGroup();

    SectionHeader("Actualizaciones", leftColWidth);

    // Altura del bloque de acciones que va debajo de la lista (botón "Buscar
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

        ImGui::PushStyleColor(ImGuiCol_ChildBg, HT::CardAlt);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, HT::RadiusMd);
        ImGui::BeginChild(info.version, ImVec2(leftColWidth, 140.0f), false, ImGuiWindowFlags_NoScrollbar);

        ImVec2 cardStartPos = ImGui::GetCursorScreenPos();
        ImVec2 cardEndPos   = ImVec2(cardStartPos.x + leftColWidth, cardStartPos.y + 140.0f);
        const bool cardHovered = ImGui::IsMouseHoveringRect(cardStartPos, cardEndPos);
        const float hoverT = HubHoverLerp(ImGui::GetID(info.version), cardHovered);

        ImGui::SetCursorPos(ImVec2(10.0f, 10.0f));
        ImGui::BeginGroup();

        const float thumbW = 180.0f, thumbH = 120.0f;
        if (cardCover.id != 0) {
            const ImVec2 thumbMin = ImGui::GetCursorScreenPos();
            const ImVec2 thumbMax = ImVec2(thumbMin.x + thumbW, thumbMin.y + thumbH);

            char stateKey[96];
            snprintf(stateKey, sizeof(stateKey), "card_%s", info.version);

            DrawCoverImageCover(ImGui::GetWindowDrawList(), cardCover.id, cardCover.width, cardCover.height,
                thumbMin, thumbMax, HT::RadiusMd, ImDrawFlags_RoundCornersAll,
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
        ImGui::PushStyleColor(ImGuiCol_Text, HT::TextPri);
        ImGui::Text("Versión v%s", info.version);
        ImGui::PopStyleColor();
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
        if (ImGui::IsItemHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        // Realce de hover suavizado (en vez de un rect plano on/off): fondo
        // tenue + barra de acento a la izquierda que crece con hoverT.
        ImDrawList* cardDl = ImGui::GetWindowDrawList();
        if (hoverT > 0.001f) {
            cardDl->AddRectFilled(cardStartPos, cardEndPos,
                ColAf(HT::TextPri, 0.05f * hoverT), HT::RadiusMd);
            cardDl->AddRectFilled(cardStartPos, ImVec2(cardStartPos.x + 3.0f, cardEndPos.y),
                ColAf(HT::AccentBlue, hoverT), HT::RadiusMd, ImDrawFlags_RoundCornersLeft);
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
    ImGui::PushStyleColor(ImGuiCol_Button,        HT::Surface);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HT::SurfaceHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  HT::SurfaceActive);
    ImGui::PushStyleColor(ImGuiCol_Text,          HT::TextPri);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, HT::RadiusSm);
    if (ImGui::Button("Buscar actualizaciones", ImVec2(180.0f, actionsRowH))) {
        m_ActiveTab = 7; m_OpenSettingsRequested = true; // 7 = Actualizaciones (ver QuickBtn arriba)
    }
    ImGui::SameLine(0.0f, 15.0f);
    if (ImGui::Button("Foro / Soporte", ImVec2(180.0f, actionsRowH)))
        ProyecThor::External::OpenURL("https://github.com/TheVixcho/ProyecThor/discussions");
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    ImGui::EndGroup();

    ImGui::EndGroup();

    // ── Columna derecha ───────────────────────────────────────────────────────
    ImGui::SameLine(0.0f, spacingX);
    ImGui::BeginGroup();

    SectionHeader("Resumen local", rightColWidth);

    const auto topSongs = ProyecThor::UI::GetTopSongPlayStats(5);
    const int totalProjections = ProyecThor::UI::GetTotalSongProjections();
    const auto perfSummary = ProyecThor::UI::GetPerformanceSummary();
    const auto perfHistory = ProyecThor::UI::GetRecentPerformanceHistory(8);

    auto DrawMetricCard = [&](const char* label, const std::string& value, const char* hint,
                               ImU32 color, const std::vector<int>* spark = nullptr) {
        const bool  hasSpark = spark && spark->size() >= 2;
        const float cardH    = hasSpark ? 96.0f : 70.0f;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, HT::Card);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, HT::RadiusMd);
        ImGui::BeginChild(label, ImVec2(rightColWidth - 8.0f, cardH), false);

        const ImVec2 cMin     = ImGui::GetWindowPos();
        const ImVec2 cMax     = ImVec2(cMin.x + rightColWidth - 8.0f, cMin.y + cardH);
        const float  hoverT   = HubHoverLerp(ImGui::GetID(label), ImGui::IsWindowHovered());
        if (hoverT > 0.001f)
            ImGui::GetWindowDrawList()->AddRectFilled(cMin, cMax, ColAf(HT::TextPri, 0.04f * hoverT), HT::RadiusMd);

        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(value.c_str());
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, HT::TextMuted);
        ImGui::TextUnformatted(label);
        ImGui::TextDisabled("%s", hint);
        ImGui::PopStyleColor();

        // Mini sparkline con el historial reciente (ya se pedia via
        // GetRecentPerformanceHistory pero nunca se dibujaba).
        if (hasSpark) {
            const ImVec2 sMin = ImVec2(cMin.x + 12.0f, cMax.y - 30.0f);
            const ImVec2 sMax = ImVec2(cMax.x - 12.0f, cMax.y - 10.0f);

            int lo = spark->front(), hi = spark->front();
            for (int v : *spark) { lo = std::min(lo, v); hi = std::max(hi, v); }
            if (hi == lo) hi = lo + 1;

            std::vector<ImVec2> pts(spark->size());
            for (size_t i = 0; i < spark->size(); ++i) {
                const float tx = static_cast<float>(i) / static_cast<float>(spark->size() - 1);
                const float ty = static_cast<float>((*spark)[i] - lo) / static_cast<float>(hi - lo);
                pts[i] = ImVec2(sMin.x + tx * (sMax.x - sMin.x), sMax.y - ty * (sMax.y - sMin.y));
            }

            ImDrawList* sdl = ImGui::GetWindowDrawList();
            std::vector<ImVec2> fillPts = pts;
            fillPts.push_back(ImVec2(sMax.x, sMax.y));
            fillPts.push_back(ImVec2(sMin.x, sMax.y));
            sdl->AddConvexPolyFilled(fillPts.data(), static_cast<int>(fillPts.size()), ColAf(color, 0.16f));
            sdl->AddPolyline(pts.data(), static_cast<int>(pts.size()), color, 0, 1.6f);
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
    };

    std::vector<int> fpsSpark;
    fpsSpark.reserve(perfHistory.size());
    for (const auto& [sampleLabel, fps] : perfHistory)
        fpsSpark.push_back(fps);

    const std::string fpsHint =
        std::string("Ultimos registros del Hub  ·  pico ") + std::to_string(perfSummary.second) + " fps";

    DrawMetricCard("Proyecciones totales", std::to_string(totalProjections), "Cuentas locales registradas", HT::Success);
    DrawMetricCard("FPS promedio", std::to_string(perfSummary.first), fpsHint.c_str(), HT::AccentBlue, &fpsSpark);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, HT::Card);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, HT::RadiusMd);
    ImGui::BeginChild("##SongStats", ImVec2(rightColWidth, 240.0f), false);

    ImGui::PushStyleColor(ImGuiCol_Text, HT::TextPri);
    ImGui::Text("Canciones más proyectadas");
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    if (topSongs.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, HT::TextMuted);
        ImGui::TextWrapped("Aún no hay estadísticas locales. Proyecta 2 versos o más de una canción para empezar.");
        ImGui::PopStyleColor();
    } else {
        for (size_t i = 0; i < topSongs.size(); ++i) {
            const auto& [title, count] = topSongs[i];
            const std::string childId = "##songStat" + std::to_string(i);

            ImGui::PushStyleColor(ImGuiCol_ChildBg, HT::CardAlt);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, HT::RadiusSm);
            ImGui::BeginChild(childId.c_str(), ImVec2(rightColWidth - 10.0f, 48.0f), false);

            const ImVec2 rMin    = ImGui::GetWindowPos();
            const ImVec2 rMax    = ImVec2(rMin.x + rightColWidth - 10.0f, rMin.y + 48.0f);
            const float  hoverT  = HubHoverLerp(ImGui::GetID(childId.c_str()), ImGui::IsWindowHovered());
            if (hoverT > 0.001f)
                ImGui::GetWindowDrawList()->AddRectFilled(rMin, rMax, ColAf(HT::TextPri, 0.05f * hoverT), HT::RadiusSm);

            // Columna derecha (contador + "proyecciones") con ancho fijo
            // reservado segun su propio contenido; el titulo se trunca con
            // elipsis para no invadirla en canciones con nombres largos
            // (antes se dibujaba sin clip y se superponia con el contador).
            const std::string countStr  = std::to_string(count);
            const float countColW   = std::max(ImGui::CalcTextSize(countStr.c_str()).x,
                                                ImGui::CalcTextSize("proyecciones").x);
            const float rightColX   = (rightColWidth - 10.0f) - countColW - 14.0f;
            const float titleMaxW   = rightColX - 12.0f;

            std::string displayTitle = title;
            if (ImGui::CalcTextSize(displayTitle.c_str()).x > titleMaxW) {
                while (!displayTitle.empty() &&
                       ImGui::CalcTextSize((displayTitle + "...").c_str()).x > titleMaxW) {
                    displayTitle.pop_back();
                }
                // Evita cortar a mitad de un caracter UTF-8 multibyte
                // (tildes/ñ) dejando un byte de continuacion colgante.
                while (!displayTitle.empty() &&
                       (static_cast<unsigned char>(displayTitle.back()) & 0xC0) == 0x80) {
                    displayTitle.pop_back();
                }
                displayTitle += "...";
            }

            ImGui::PushStyleColor(ImGuiCol_Text, HT::TextPri);
            ImGui::TextUnformatted(displayTitle.c_str());
            ImGui::PopStyleColor();
            ImGui::SameLine(rightColX);
            ImGui::BeginGroup();
            ImGui::TextUnformatted(countStr.c_str());
            ImGui::PushStyleColor(ImGuiCol_Text, HT::TextMuted);
            ImGui::TextDisabled("proyecciones");
            ImGui::PopStyleColor();
            ImGui::EndGroup();

            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            if (i + 1 < topSongs.size()) {
                ImGui::Dummy(ImVec2(0.0f, 6.0f));
            }
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    ImGui::EndGroup();
    ImGui::EndChild();

    // ── Modal Universal de Actualizacion ──────────────────────────────────────
    // s_ModalAnim se aproxima a 1 mientras isUpdateModalOpen y decae a 0 al
    // cerrar; el modal sigue dibujandose (con escala/alpha decrecientes)
    // hasta que la animacion termina, en vez de desaparecer de golpe.
    static float s_ModalAnim = 0.0f;
    {
        const float target = isUpdateModalOpen ? 1.0f : 0.0f;
        s_ModalAnim += (target - s_ModalAnim) * std::min(1.0f, parallaxDt * 10.0f);
        s_ModalAnim = std::clamp(s_ModalAnim, 0.0f, 1.0f);
        if (s_ModalAnim < 0.001f) s_ModalAnim = 0.0f;
    }

    if (isUpdateModalOpen || s_ModalAnim > 0.0f) {
        const UpdateVersionInfo* selInfo = FindUpdateVersion(selectedUpdateVer);
        const GLTextureInfo modalCover = selInfo ? GetCoverTexture(selInfo->coverFile) : GLTextureInfo{};

        ImGuiViewport* vp     = ImGui::GetMainViewport();
        const float     fadeA = EaseOut(s_ModalAnim);

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, fadeA);

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

        // Leve "pop" de escala al abrir (0.96 -> 1.0) con la misma curva.
        const float scale         = 0.96f + 0.04f * fadeA;
        const float modalW        = 780.0f * scale, modalH = 660.0f * scale;
        const float headerH       = 200.0f * scale, footerH = 62.0f * scale;
        const float modalRounding = HT::RadiusLg;

        ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(modalW, modalH), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ColA(HT::Card, 255));
        ImGui::PushStyleColor(ImGuiCol_Border,   HT::Divider);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   modalRounding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0.0f, 0.0f));

        bool vis = ImGui::Begin("##UpdateModal", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);

        if (vis) {
            ImDrawList* dl     = ImGui::GetWindowDrawList();
            ImVec2      winP   = ImGui::GetWindowPos();
            const ImU32 modalBg = ColA(HT::Card, 255);

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
                dl->AddRectFilled(headerMin, headerMax, ColA(HT::CardAlt, 255),
                    modalRounding, ImDrawFlags_RoundCornersTop);
                ImGui::Dummy(ImVec2(modalW, headerH));
            }

            dl->AddRectFilledMultiColor(
                ImVec2(winP.x, winP.y+headerH-60), ImVec2(winP.x+modalW, winP.y+headerH),
                ColA(modalBg, 0), ColA(modalBg, 0), modalBg, modalBg);

            ImGui::SetCursorPos(ImVec2(0, headerH));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0,0,0,0));
            ImGui::BeginChild("##ModalScroll", ImVec2(modalW, modalH-headerH-footerH), false);

            const float mg = 36.0f, cw = modalW - mg*2;
            ImGui::SetCursorPos(ImVec2(mg, 18.0f));
            ImGui::BeginGroup();

            // Badge tipo "pill": mide el texto real y dibuja el padding con
            // el rect, en vez del hack anterior de espacios embebidos en el
            // string (" ACTUALIZACIÓN MAYOR ") para simular relleno.
            auto DrawPillBadge = [&](const char* text) {
                ImGui::SetWindowFontScale(0.8f);
                const ImVec2 bs = ImGui::CalcTextSize(text);
                ImGui::SetWindowFontScale(1.0f);
                const ImVec2 pad(8.0f, 3.0f);
                const ImVec2 bp = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddRectFilled(
                    ImVec2(bp.x - pad.x, bp.y - pad.y), ImVec2(bp.x + bs.x + pad.x, bp.y + bs.y + pad.y),
                    HT::AccentBlue, HT::RadiusSm);
                ImGui::Dummy(ImVec2(pad.x, 0.0f));
                ImGui::SameLine(0.0f, 0.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, HT::OnAccent);
                ImGui::SetWindowFontScale(0.8f); ImGui::Text("%s", text); ImGui::SetWindowFontScale(1.0f);
                ImGui::PopStyleColor();
                ImGui::SameLine(0.0f, pad.x);
            };
            DrawPillBadge(selInfo ? selInfo->modalBadge : "ACTUALIZACIÓN");
            ImGui::SameLine(0, 40);

            ImGui::SetWindowFontScale(0.8f);
            ImGui::PushStyleColor(ImGuiCol_Text, HT::TextMuted);
            ImGui::Text("HISTORIAL DE VERSIONES");
            ImGui::PopStyleColor();
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Dummy(ImVec2(0,6));

            ImGui::SetWindowFontScale(1.7f);
            ImGui::PushStyleColor(ImGuiCol_Text, HT::TextPri);
            ImGui::Text("Actualización v%s", selInfo ? selInfo->version : "?");
            ImGui::PopStyleColor();
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Dummy(ImVec2(0,20));

            auto Cat = [&](const char* t) {
                ImGui::PushStyleColor(ImGuiCol_Text, HT::TextPri);
                ImGui::SetWindowFontScale(1.05f); ImGui::Text("%s",t); ImGui::SetWindowFontScale(1.0f);
                ImGui::PopStyleColor();
                ImVec2 p = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddLine(
                    ImVec2(p.x,p.y+1), ImVec2(p.x+cw,p.y+1), HT::BorderFaint);
                ImGui::Dummy(ImVec2(0,10));
            };
            auto Bul = [&](const char* t) {
                ImVec2 bp = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddCircleFilled(
                    ImVec2(bp.x+6, bp.y+ImGui::GetTextLineHeight()*0.5f), 2.5f, HT::AccentBlue);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX()+18);
                ImGui::PushStyleColor(ImGuiCol_Text, HT::TextMuted);
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+cw-22);
                ImGui::TextWrapped("%s",t);
                ImGui::PopTextWrapPos(); ImGui::PopStyleColor();
                ImGui::Dummy(ImVec2(0,4));
            };

            if (selectedUpdateVer == 12) { // v0.5.1
                Cat("Contadores (antes \"Reloj y Contadores\")");
                Bul("Se acorto el nombre de la sección a secas \"Contadores\" en el sidebar de Home.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Reloj dentro de Overlays");
                Bul("Nuevo cuadro de Reloj en el editor de Overlays (botón junto a Texto/Forma/Imagen): lo arrastras, le das tamaño y estilo de texto (fuente, color, sombra, contorno, fondo) una sola vez, como una capa más.");
                Bul("Ese cuadro es solo un marcador de posición: al proyectar el overlay que lo contiene, se reemplaza en vivo por la hora o el cronómetro activo — nunca queda \"horneado\" como texto fijo en el overlay guardado.");
                Bul("La transmisión a pantalla del reloj ya no es un modo aparte a elegir: aparece automáticamente si el overlay que tenes activo incluye un cuadro de Reloj. El panel de Contadores muestra un aviso si el overlay activo no tiene uno.");
                Bul("La transmisión a dispositivos en red (LAN) sigue siendo un interruptor propio (Apagado / Solo LAN), independiente del overlay.");
                Bul("Se agregaron overlays de reloj predeterminados (barra inferior, esquina y centrado) listos para probar de una, sin tener que armar uno desde cero.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Editor de Overlays");
                Bul("Las capas ahora se pueden reordenar (subir/bajar) desde la lista lateral, para elegir cual queda encima de cual.");
                Bul("Encabezado del editor más plano y compacto (se saco el degradado de color) y menos relleno en los margenes, para un look más minimalista.");
                Bul("Corregido: el cuadriculado que indica \"sin fondo\" en el editor podia terminar guardado como fondo opaco (gris/negro) en el PNG del overlay en vez de quedarse transparente, sobre todo en overlays sin capas de imagen.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Correcciones en Contadores");
                Bul("El aviso de \"overlay activo sin cuadro de reloj\" y otros textos largos ya no se cortaban contra el borde del panel: ahora se ajustan en varias lineas.");
                Bul("Corregido un icono roto en el botón \"Avanzar\" del título/mensaje del reloj.");
                ImGui::Dummy(ImVec2(0,12));
            } else if (selectedUpdateVer == 11) { // v0.5.0
                Cat("Ajustes reorganizado");
                Bul("Cada configuración ahora es su propia página: al elegir una subcategoria en el menu de la izquierda, se ve sola en vez de tener que scrollear una lista larga con todo junto.");
                Bul("Nuevo buscador arriba del menu de Ajustes, para encontrar una configuración por nombre sin tener que navegar categoría por categoría.");
                Bul("Proyección y Pantallas ahora estan agrupadas juntas en el menu, y Red, Mobile, Streaming y OSC pasaron a vivir DENTRO de Proyección en vez de tener su propia categoría aparte.");
                Bul("Se saco la categoría General (Inicio, Guardado automático, Carpetas por defecto): no se usaba.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Fondos: bucle falso");
                Bul("Nueva opción en Ajustes > Proyección > Fondos: en vez de cortar siempre al mismo frame inicial al repetir, el fondo reproduce hacia adelante y despues \"hacia atras\", dando sensacion de bucle continuo sin el salto de siempre.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Biblioteca > Render");
                Bul("El conversor de formato tiene un diseño más moderno, con el texto que antes se cortaba contra el borde del panel ahora bien acomodado.");
                Bul("Se saco el botón de Reloj del sidebar de Biblioteca: ya estaba disponible en la barra inferior de Vista en Vivo, quedaba duplicado.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Nueva sección: Overlays");
                Bul("Crea overlays (textos, formas e imágenes) en un editor nuevo a pantalla completa, desde Biblioteca > Overlay.");
                Bul("Un overlay se guarda como imagen PNG con transparencia real: al mostrarlo, se proyecta como una capa aparte ENCIMA del fondo y la letra, dejando ver lo que haya debajo — antes, por error, lo reemplazaba todo como si fuera un fondo más.");
                Bul("El editor tiene una barra flotante para agregar texto, formas o imágenes, lista de capas, y botón de Eliminar para la capa seleccionada.");
                Bul("Acceso rápido tambien desde Vista en Vivo (botón Overlays de la barra inferior), con galeria de miniaturas para aplicar uno sin salir de la pantalla.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Vista en Vivo renovada");
                Bul("Reproductor más simple: se saco el encabezado \"PROGRAM - ON AIR\" y los botones de transporte pasaron a iconos chicos y planos, más parecidos al resto de apps de proyección.");
                Bul("Overlays, Chat, Pads y Reloj ahora se abren DENTRO del mismo panel de Vista en Vivo (con scroll propio si hay mucho contenido), en vez de ventanas flotantes sueltas que quedaban desconectadas del botón que las abria.");
                Bul("La barra de botones de abajo quedo pegada justo debajo del reproductor, sin espacio vacio en el medio, y con los botones más parejos entre si.");
                Bul("Se saco la tira de Stage que aparecia arriba del video: quedaba duplicada con el botón que ya permite alternar toda la vista entre Público y Stage.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Nueva sección: Pantallas");
                Bul("La configuración de Stage (que monitor usa, si es por red, que muestra cada pantalla) ahora tiene su propio menu \"Pantallas\" arriba de todo, en vez de estar mezclada con Proyección.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Correcciones de tema y apariencia");
                Bul("Varias ventanas y menus (el menu superior, los popups de Estilos y el selector rápido Alt+Espacio, el Monitor de Control) ignoraban el tema elegido en Ajustes > Apariencia y se quedaban siempre con los mismos colores fijos — ahora todos respetan el tema.");
                Bul("Los fondos de los paneles eran levemente transparentes y dejaban ver lo que hubiera atras, dando un aspecto \"lavado\" o inconsistente segun el tema — ahora son solidos.");
                ImGui::Dummy(ImVec2(0,12));
            } else if (selectedUpdateVer == 10) { // v0.4.3
                Cat("Nueva sección: Conexiones");
                Bul("Toolbar nueva arriba de todo (Hub / Proyector / Conexiones / Biblioteca / Biblia) para saltar entre secciones completas de la app, opcional segun Vista.");
                Bul("OSC: enviar mensajes a luces/controladores externos con dirección IP y puerto configurables, más \"Aprender\" (OSC Learn) para vincular un fader externo a parámetros en vivo como opacidad, velocidad, escala, color o intensidad de los shaders.");
                Bul("Red y Chat, disponibles ahora en dos lugares a la vez (Conexiones y su ubicacion original en Biblioteca/Herramientas): es la misma conexión y el mismo chat, no hay que elegir uno.");
                Bul("Streaming en vivo real por RTMP (Twitch, YouTube, Facebook, etc.), con captura de camara/pantalla, preview y control de capas tipo OBS, todo integrado en el mismo rail.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Nueva sección: Biblioteca");
                Bul("Ver, renombrar y borrar tus archivos de Video, Imagen y Audio ya importados, separado de Vista en Vivo para no arriesgar nada de lo que este proyectando.");
                Bul("Nuevo panel \"Render\": convierte tus videos y audios a otros formatos aprovechando ffmpeg, sin instalar nada aparte.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Biblia a pantalla completa");
                Bul("El mismo buscador de Biblia de siempre, ahora tambien como su propia sección a pantalla completa: libros/capítulos a la izquierda, texto grande a la derecha.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Selector rápido y novedades");
                Bul("Alt+Espacio abre un selector para saltar entre Hub, Conexiones, Biblioteca y Biblia con el teclado, sin tocar el mouse.");
                Bul("Al abrir una versión nueva de ProyecThor aparece un carrusel de novedades en el Hub, en vez de tener que buscarlas en esta misma pantalla.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Monitor de Vista en Vivo, más compacto");
                Bul("El panel de Preview del Monitor ocupaba mucho más alto del que en realidad necesitaba: se redujo para darle bastante más espacio al video.");
                Bul("El botón de Play/Pausa se integro en la misma fila que Inicio / -10s / +10s / Detener, en vez de tener su propia fila completa aparte.");
                Bul("Botones e iconos del Preview más chicos y prolijos; la columna central (Transmitir/Loop) ahora se achica sola si el espacio disponible es menor al habitual, en vez de cortarse.");
                Bul("Sacado el botón de Contener/Estirar de esa columna: ya estaba disponible a la derecha de Vista en Vivo, no hacia falta duplicarlo.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Editor de Estilos renovado");
                Bul("Se le bajo el tono \"arcoiris\" que tenia (cada pestaña/tarjeta con un color distinto) a favor de un solo acento consistente con el resto de la app.");
                Bul("Encabezado, bordes y esquinas más sobrios y rectos, en linea con el resto de los paneles en vez de un look aparte tipo Canva.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Nuevo instalador para Windows");
                Bul("ProyecThor ahora se instala con un instalador moderno: más rápido, más prolijo y con menos falsos positivos de antivirus.");
                Bul("Si ya tenias ProyecThor instalado con una versión anterior (aunque sea de un instalador viejo), no hace falta que la desinstales a mano: el instalador nuevo la detecta y la reemplaza solo, sin dejar archivos sueltos de la versión vieja.");
                Bul("Corregido: el icono de la aplicacion no se veia bien (aparecia en blanco) en el acceso directo y en el instalador.");
                Bul("Las actualizaciones automáticas de esta pantalla tambien se actualizaron para descargar el instalador nuevo correctamente.");
                Bul("Nuevo aviso en Ajustes > Actualizaciones, con un icono de información que te recuerda revisar \"Agregar o quitar programas\" si sospechas que quedo más de una versión instalada.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Biblioteca con contenido de entrada");
                Bul("Canciones y Biblias ya no arrancan vacias en una instalación nueva: se cargan solas una canción de bienvenida y varias Biblias (español, inglés y portugués) para tener algo con que probar de una.");
                Bul("Corregido: al ponerle Título a una canción nueva (o cambiarselo a una ya existente) desde el editor, ahora se ve reflejado en la lista, el buscador y las playlists — antes quedaba guardado por dentro pero la Biblioteca seguia mostrando el nombre viejo (\"Nueva canción\").");
                Bul("Corregido: renombrar una canción desde el menu contextual ya no le hace perder el autor, las etiquetas, el estilo/fondo preferido ni las playlists en las que estaba.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Correcciones de estabilidad");
                Bul("Corregido un cierre inesperado de la app relacionado con ffmpeg: antes podia abrir brevemente una consola negra y cerrarse sin avisar el motivo; ahora corre oculto y muestra el error real si algo falla (por ejemplo, al convertir un video en Biblioteca > Render).");
                ImGui::Dummy(ImVec2(0,12));
            } else if (selectedUpdateVer == 9) { // v0.4.2
                Cat("Pads de Vista en Vivo");
                Bul("Corregido el problema por el cual guardar un pad (click derecho > Guardar aquí) podia no aplicar nada al presionarlo despues: ahora siempre captura estilo, fondo y captura de pantalla tal cual estan en pantalla.");
                Bul("El panel de Pads se reorganizo en dos secciones: \"General\" (los pads de siempre) y \"Captura\", que ahora muestra las mismas escenas rápidas del panel Captura, sincronizadas — guardar o aplicar una desde cualquiera de los dos lados es lo mismo.");
                Bul("El texto de ayuda de \"Escenas rápidas\" se reemplazo por un icono de información, para no saturar el panel de letra.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Transporte y volumen de Vista en Vivo");
                Bul("Los botones de Play/Pausa, Retroceder, Avanzar y Detener ahora son pads de colores tipo controlador MIDI, con el botón de reproducción iluminado en rojo mientras esta en vivo.");
                Bul("El control de volumen pasa a ser un fader horizontal estilo consola de sonido en vez del slider de siempre.");
                Bul("Corregido un icono roto en el botón de silenciar (mute) de Vista en Vivo.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Biblia: buscador por palabras");
                Bul("Nuevo botón (lupa + \"Aa\") junto al buscador rápido: permite escribir una o más palabras y muestra todos los versiculos de la Biblia activa que las contienen, para cuando no te acordas la cita exacta.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Editor de Estilos renovado");
                Bul("El editor de un estilo ya no abre una ventana flotante encima de todo: ahora se muestra acoplado dentro de Home, ocupando todo ese espacio, como una sección más de la Biblioteca.");
                Bul("El selector de fuente pasa de una lista de texto a una grilla con la vista previa real de cada tipografia.");
                Bul("Nueva pestaña \"Efectos\": fondo, borde, sombra, aberración cromática, glow (bloom), neon y subrayado, todo configurable por separado para el texto proyectado.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Shaders: muchos efectos nuevos");
                Bul("NIS: escalador alternativo a FSR, exclusivo para placas NVIDIA (se detecta automáticamente).");
                Bul("VHS: sangrado de color, scanlines, bamboleo y ruido de estatica, como una cinta de video vieja.");
                Bul("Cine: gradacion de color tipo cine, con tinte a elegir entre rojo, verde o azul.");
                Bul("Contraste y Luminosidad: ajuste directo de contraste y brillo de la salida en vivo.");
                Bul("Blur, Sharpen, Bloom y Aberración cromática: desenfoque, nitidez, resplandor de brillos y desfase de color, respectivamente.");
                Bul("TAA (antialiasing temporal): suaviza bordes mezclando con el frame anterior, a costa de un poco de desenfoque de movimiento.");
                ImGui::Dummy(ImVec2(0,12));
            } else if (selectedUpdateVer == 8) { // v0.4.1
                Cat("Editor de canciones (rediseño total)");
                Bul("Editar una canción ya no abre una ventana flotante encima: el mismo panel de Canciones pasa a modo edicion, con letra a la izquierda (mucho más grande) y preview de las diapositivas a la derecha.");
                Bul("Título y Autor quedan siempre a la vista; Nota, Derechos de autor y Extra se movieron detras de un botón de información para no restarle espacio a la letra.");
                Bul("Todo se guarda solo mientras se escribe (sin botón Guardar), con indicador de estado y botones de Deshacer/Rehacer del último cambio.");
                Bul("Nuevo filtro de \"Lineas por diapositiva\" (1/2/3): separa la letra de verdad, insertando lineas en blanco reales dentro de cada estrofa, para que la division se vea en el propio texto y no solo en el preview.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Menu principal reorganizado");
                Bul("Nuevo menu \"ProyecThor\" (primero, a la izquierda) con Preferencias y Salir.");
                Bul("Archivo ahora es la categoría Importar, con una opción nueva: \"Importar canción desde portapapeles\" (crea la canción y pega el contenido del portapapeles de una).");
                Bul("\"Base de datos\" y \"Wiki\" se movieron al menu Ayuda.");
                Bul("Nuevo menu \"Ventana\" con Pantalla completa (tambien con la tecla F11).");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Efectos de video (rediseñado + nuevos)");
                Bul("El panel de Shaders (al lado de Overlays, en Diseño) ahora se ve como tarjetas con icono, descripcion y control de intensidad propio para cada efecto, en vez de una lista de switches.");
                Bul("Dos efectos nuevos: Saturación (colores más vivos o hasta blanco y negro) y Vinetado (oscurece los bordes para enfocar el centro), sumados a FSR, CRT, grano de pelicula y FXAA.");
                Bul("Nuevo efecto \"Rellenado\" (recomendado): llena las barras negras de letterbox/pillarbox con el mismo fondo, estirado y muy desenfocado, en vez de dejarlas negras — el efecto tipo Spotify Canvas / Smart TV.");
                Bul("Cada efecto se prende o apaga por separado y se ve reflejado al instante en la salida en vivo.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Biblioteca > Videos");
                Bul("Los videos ahora muestran una miniatura real (un frame del video), igual que ya pasaba con los Fondos.");
                Bul("Nuevo botón para alternar entre vista en lista y vista en grilla con miniaturas grandes, más un control para agrandar o achicar las miniaturas.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Biblioteca > Playlists");
                Bul("El panel de \"Agregar canciones\" a una playlist es más grande y las canciones se listan en orden alfabetico, con un botón \"+\" bien visible para agregar y una insignia verde \"Agregada\" para las que ya estan.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Captura (camara / pantalla)");
                Bul("Nuevas \"Escenas rápidas\": 8 botones de color donde guardar una fuente + recuadro + opacidad ya armados, para saltar entre encuadres con un solo click durante el evento.");
                Bul("Click derecho sobre un botón para guardar la posición libre actual ahi o borrarla; quedan guardadas entre sesiones.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Ajustes > Apariencia");
                Bul("Nueva fuente de interfaz personalizable: se puede importar una tipografia propia (.ttf/.otf/.ttc) además de elegir entre las que ya trae la app, con reinicio guiado para aplicarla.");
                Bul("El menu de Ajustes se reordeno con iconos por categoría y subcategorías navegables, para ubicar cada opción más rápido.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Nuevo motor de video (experimental)");
                Bul("En Ajustes > Proyección, opción para elegir el motor con el que se reproducen los Videos: el de siempre (OpenGL) o uno nuevo (libvlc) que usa una ventana propia con reproducción acelerada.");
                Bul("Pensado para equipos con poca placa de video — los Fondos (loops decorativos) siempre siguen mostrandose como hasta ahora, con overlays y texto encima.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Estabilidad");
                Bul("Corregido un problema por el cual el video de fondo podia irse desincronizando del audio con el correr de los minutos en computadoras más lentas.");
                Bul("Corregido: el control de FSR en Ajustes > Proyección y el del panel de Shaders podian mostrar y guardar valores distintos entre si.");
                ImGui::Dummy(ImVec2(0,12));
            } else if (selectedUpdateVer == 7) { // v0.4.0
                Cat("Cola de videos y video en vivo");
                Bul("La cola de videos es mucho más confiable: los clips pasan de uno a otro sin cortes ni pantallas de carga de por medio.");
                Bul("Corregido: la app ya no se traba si hacias clic varias veces seguidas sobre el mismo video.");
                Bul("Los videos de la cola ahora siempre arrancan desde el principio, nunca aparecen a mitad de camino.");
                Bul("Corregido un cierre inesperado de la app en Windows al usar la Vista Previa mientras habia algo en vivo.");
                Bul("La Vista Previa de la Biblioteca ya no puede trabar ni afectar al video que esta en vivo para el público.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Nuevo panel de Rendimiento");
                Bul("Panel opcional (menu Vista > Rendimiento) que muestra en vivo el uso de CPU, memoria RAM y los FPS de la app — útil para saber si la computadora esta exigida durante un evento.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Overlays (nuevo)");
                Bul("Nueva sección para crear tus propios overlays: imágenes con texto que podes acomodar libremente arrastrandolo por la pantalla.");
                Bul("Guardá tus overlays y usalos despues con un solo clic, igual que un fondo.");
                Bul("Podes editar o borrar los overlays guardados desde un menu rápido.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Vista en Vivo");
                Bul("Nuevos botones rápidos al costado de Vista en Vivo para limpiar el texto, quitar el fondo, ajustar la proporcion o silenciar el audio sin buscar en menus.");
                Bul("El panel de Control quedo más simple: solo iniciar/detener la proyección y elegir la pantalla.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Fondos y Estilos");
                Bul("Los Fondos ahora se organizan en carpetas, más fáciles de navegar.");
                Bul("Nuevo control para agrandar o achicar las miniaturas y ver más fondos o estilos a la vez.");
                Bul("Animaciones más suaves al pasar el mouse y cambiar de sección.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Interfaz general");
                Bul("Los 4 menus de iconos (Biblioteca, Control, Home y Diseño) se ven más prolijos y del mismo tamaño entre si.");
                Bul("Podes ocultar los titulos debajo de los iconos (menu Vista) para ganar espacio en pantalla.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Biblioteca y fuentes");
                Bul("Corregido: al importar una fuente nueva la app se ponia en negro y habia que reiniciarla para que se viera.");
                Bul("Al cambiar de categoría en la Biblioteca (Letra, Video, Biblia, etc.) la busqueda se limpia sola, para que un resultado vacio no se confunda con contenido que desaparecio.");
                Bul("El fondo de cada canción ahora se elige de tu biblioteca de Fondos en vez de buscar un archivo suelto en la computadora.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Monitor de Control (Stage)");
                Bul("Nuevo botón en Vista en Vivo para alternar la previsualizacion entre Público y Stage, y tener a la vista ambas salidas sin un segundo monitor.");
                Bul("[Experimental] Opción para que el Monitor de Control muestre exactamente lo mismo que ve el operador en Vista en Vivo, en vez de la grilla de reloj/texto.");
                ImGui::Dummy(ImVec2(0,12));
            } else if (selectedUpdateVer == 6) { // v0.3.5 — version estable, changelog consolidado
                Cat("Audio");
                Bul("Sonido renovado: nueva pantalla de audio, portada por canción, ecualizador y control de volumen.");
                Bul("Ahora podes asignar autores a las canciones.");
                Bul("Cambiar de canción es más rápido y con menos cortes de audio.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Reproducción y previsualizacion");
                Bul("La Vista Previa y el video en vivo ahora son totalmente independientes: uno ya no afecta al otro.");
                Bul("Corregidas las pantallas negras en el segundo monitor y videos con la proporcion incorrecta.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Cola de reproducción");
                Bul("La cola avanza de forma más confiable entre videos, incluso si hay algun archivo eliminado o roto.");
                Bul("Corregidos casos donde la cola podia desincronizarse de lo que realmente se estaba mostrando.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Biblioteca");
                Bul("Biblioteca renovada, con listas y playlists más fáciles de usar.");
                Bul("Nuevo sistema de etiquetas de colores para organizar tus canciones.");
                Bul("Busqueda mejorada y navegación con las flechas del teclado más prolija.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Biblia");
                Bul("Nuevos atajos de teclado para buscar libro, capítulo o versiculo más rápido (Ctrl+F, Ctrl y Alt).");
                Bul("Nueva sección en Ajustes con todos los atajos disponibles.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Control de proyección");
                Bul("Mejor soporte para varios monitores (proyector y stage).");
                Bul("Panel de control más simple, todo en una sola fila de botones.");
                Bul("El mute y el volumen ahora se mantienen sincronizados entre el control y el monitor.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Red local y streaming");
                Bul("Transmisión por red local (LAN) más estable, con menos cortes.");
                Bul("Corregidos errores de imagen y de marca de agua en la transmisión.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Estadisticas locales");
                Bul("Nuevo resumen en el Hub con el total de proyecciones y las canciones más usadas.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Soporte para Linux");
                Bul("ProyecThor ahora funciona de forma nativa en Linux, probado en Arch Linux y derivados.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Atajos de teclado globales");
                Bul("Ctrl+P, F1 y Alt+F4 ahora funcionan desde cualquier pantalla de la app (Preferencias, Ayuda y Cerrar).");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Interfaz y experiencia");
                Bul("Nuevo logo y mejoras visuales en varias secciones de la app.");
                Bul("Animaciones más fluidas en el Hub principal.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Sistema y ajustes");
                Bul("Tus ajustes y preferencias se guardan y cargan correctamente entre sesiones.");
                Bul("Podes personalizar el idioma y la apariencia de la app.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Estabilidad general");
                Bul("Multiples correcciones para evitar que la app se cuelgue en biblioteca, streaming y multi-monitor.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Soporte y comunidad");
                Bul("Canal oficial de comunicación y soporte en WhatsApp y Discord.");
            } else { // v0.3.0
                Cat("General");
                Bul("Nuevo Hub central para administrar la app.");
                Bul("Código QR automático para ver la transmisión desde el celular.");
                Bul("Nuevas pantallas de bienvenida al iniciar la app.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Multimedia y Streaming");
                Bul("Mejoras en la transmisión LAN y en la conexión de dispositivos.");
                Bul("Estilos de letras predeterminados segun el tipo de lista.");
                Bul("Reproducción de video más fluida.");
                Bul("Nueva opción para transmitir fondos con la orientacion correcta.");
                Bul("Mejor rendimiento en la biblioteca y la vista previa.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Soporte y Estabilidad");
                Bul("Mejor manejo de archivos y más estabilidad general.");
                Bul("Podes editar canciones sin perder el foco en pantalla.");
                Bul("Correcciones en la cola de reproducción y en las transiciones.");
                Bul("Varias correcciones para evitar que la app se cuelgue.");
            }

            ImGui::Dummy(ImVec2(0,24));
            ImGui::EndGroup();
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::SetCursorPos(ImVec2(0, modalH-footerH));
            ImVec2 flp = ImGui::GetCursorScreenPos();
            dl->AddLine(ImVec2(flp.x,flp.y), ImVec2(flp.x+modalW,flp.y), HT::Divider, 1.0f);

            const float bw=130, bh=34;
            ImGui::SetCursorPos(ImVec2((modalW-bw)*0.5f, (modalH-footerH)+(footerH-bh)*0.5f));
            ImGui::PushStyleColor(ImGuiCol_Button,        HT::Surface);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HT::SurfaceHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  HT::SurfaceActive);
            ImGui::PushStyleColor(ImGuiCol_Text,          HT::TextPri);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, HT::RadiusSm);
            if (ImGui::Button("Cerrar", ImVec2(bw, bh)))
                isUpdateModalOpen = false;
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
        }

        ImGui::End();
        ImGui::PopStyleVar(4); // Alpha, WindowRounding, WindowBorderSize, WindowPadding
        ImGui::PopStyleColor(2);
    }
}

} // namespace ProyecThor::UI