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
#include <fstream>
#include <filesystem>
#include "settings/SettingsManager.h"
#include "../external/tools/OpenURL.h"
#include "Version.h"
#include "DesignSystem.h"
#include "HubTheme.h"
#include "SongPlayStats.h"
#include "FilePicker.h"

extern GLuint LoadTextureFromFile(const char* filename);

static void RenderSplashScreen(GLFWwindow *splashWindow, const std::string &status, float progress, GLuint logoTexture, GLuint bgTexture, ImFont *titleFont, ImFont *regularFont, ImFont *smallFont, const std::string &creditText, const ProyecThor::Settings::ThemeSettings &theme);

static constexpr float HUB_APPEAR_SPD  = 3.0f;

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
        13, "0.6.0",
        "GRAN ACTUALIZACION", "GRAN ACTUALIZACION",
        "splash_bg5.png",  // TODO: reemplazar por portada propia cuando este lista
        "Editor de overlays completo en la app movil (mover, redimensionar, rotar, "
        "seleccion multiple con guias de iman, Borrador y Degradado, exportar y "
        "enviar a la PC), panel Render renovado en Biblioteca (codecs H.264/H.265/"
        "VP9/AV1, control de compresion, cancelar a mitad de camino, barra de "
        "progreso real, estimacion y comparacion de peso, elegir donde guardar), "
        "soporte real para Linux/CachyOS (paquete de Arch validado por CI, Wayland "
        "via XWayland) y la app ahora respeta el escalado de pantalla de Windows "
        "(150%, etc). Actualizacion grande todavia en curso: revisa el detalle "
        "completo antes de considerarla cerrada."
    },
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

// Textura de fondo de la tarjeta "Abrir configuracion" -- reusa el cache +
// ancho/alto de GetCoverTexture (necesarios para el recorte tipo "cover" de
// DrawTiltTextureCard, ver mas abajo).
static const char* kConfigCardTextureFile = "bin/assets/ui/textures/20260524_104505.jpg";

// Tarjeta con inclinacion 3D al estilo "tilt" de sitios web: en reposo
// queda plana, y solo mientras el mouse esta encima las esquinas se
// distorsionan en perspectiva segun la posicion del cursor dentro de la
// tarjeta (con una sombra que se despega y un brillo diagonal que sigue la
// inclinacion). Devuelve el estado de hover/click via los punteros -- el
// llamador dibuja su propio contenido (texto, overlay) encima.
static void DrawTiltTextureCard(ImDrawList* dl, GLuint texId, int texW, int texH, ImVec2 pMin, ImVec2 pMax,
                                 ImGuiID id, bool* outHovered, bool* outClicked) {
    const ImVec2 size = ImVec2(pMax.x - pMin.x, pMax.y - pMin.y);

    ImGui::SetCursorScreenPos(pMin);
    ImGui::InvisibleButton("##tiltCardHit", size);
    const bool hovered = ImGui::IsItemHovered();
    if (outHovered) *outHovered = hovered;
    if (outClicked) *outClicked = ImGui::IsItemClicked();
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    ImGuiStorage* storage = ImGui::GetStateStorage();
    float* tiltX = storage->GetFloatRef(id ^ 0x54494C31u, 0.0f); // "TIL1"
    float* tiltY = storage->GetFloatRef(id ^ 0x54494C32u, 0.0f); // "TIL2"
    float* lift  = storage->GetFloatRef(id ^ 0x54494C33u, 0.0f); // "TIL3"

    const ImVec2 center = ImVec2(pMin.x + size.x * 0.5f, pMin.y + size.y * 0.5f);
    const ImVec2 mouse  = ImGui::GetIO().MousePos;
    const float nx = hovered ? std::clamp((mouse.x - center.x) / (size.x * 0.5f), -1.0f, 1.0f) : 0.0f;
    const float ny = hovered ? std::clamp((mouse.y - center.y) / (size.y * 0.5f), -1.0f, 1.0f) : 0.0f;

    // Interpolacion lenta a proposito (pedido explicito: "mucho mas suave,
    // mas sutil") -- un factor de suavizado chico en vez de que el tilt
    // "salte" a la posicion del mouse casi de golpe.
    const float speed = std::min(1.0f, ImGui::GetIO().DeltaTime * 4.5f);
    *tiltX += (nx - *tiltX) * speed;
    *tiltY += (ny - *tiltY) * speed;
    *lift  += ((hovered ? 1.0f : 0.0f) - *lift) * speed;

    const float maxAngle = 0.055f; // ~3 grados -- apenas perceptible, no un "volanteo"
    const float rotY =  (*tiltX) * maxAngle;
    const float rotX = -(*tiltY) * maxAngle;
    const float focal = 900.0f; // mas alto = menos distorsion de perspectiva

    const float halfW = size.x * 0.5f, halfH = size.y * 0.5f;
    const ImVec2 local[4] = {
        ImVec2(-halfW, -halfH), ImVec2(halfW, -halfH),
        ImVec2(halfW,   halfH), ImVec2(-halfW,  halfH),
    };
    ImVec2 screen[4];
    for (int i = 0; i < 4; i++) {
        const float x = local[i].x, y = local[i].y, z = 0.0f;
        const float x1 =  x * std::cos(rotY) + z * std::sin(rotY);
        const float z1 = -x * std::sin(rotY) + z * std::cos(rotY);
        const float y2 =  y * std::cos(rotX) - z1 * std::sin(rotX);
        const float z2 =  y * std::sin(rotX) + z1 * std::cos(rotX);
        const float persp = focal / (focal + z2);
        screen[i] = ImVec2(center.x + x1 * persp, center.y + y2 * persp);
    }

    // Base solida detras del quad inclinado: al rotar en "3D falso" las
    // esquinas del quad ya no coinciden exactamente con el rectangulo
    // original -- sin este relleno, los huecos dejaban ver el fondo oscuro
    // del Hub detras de la tarjeta en vez del color del panel.
    dl->AddRectFilled(pMin, pMax, ColA(HT::CardAlt, 255), HT::RadiusMd);

    // Sombra que se despega debajo de la tarjeta al inclinarse -- desplazamiento
    // reducido, apenas insinuado en vez de un salto notorio.
    const ImVec2 shadowCenter = ImVec2(center.x + (*tiltX) * 3.0f, center.y + halfH * 0.72f + (*lift) * 4.0f);
    dl->AddEllipseFilled(shadowCenter, ImVec2(halfW * 0.94f, halfH * 0.14f + (*lift) * 2.0f),
                          IM_COL32(0, 0, 0, (int)(50 + (*lift) * 35)), 0.0f, 24);

    if (texId != 0 && texW > 0 && texH > 0) {
        // Recorte tipo "cover" (igual que DrawCoverImageCover): la imagen
        // llena el rectangulo sin deformarse -- una tarjeta ancha y baja
        // como esta forzaria un stretch feo si se mapeara el UV 0..1 entero.
        const float boxAspect = size.x / size.y;
        const float imgAspect = static_cast<float>(texW) / static_cast<float>(texH);
        float baseUW, baseUH;
        if (imgAspect > boxAspect) { baseUH = 1.0f; baseUW = boxAspect / imgAspect; }
        else                       { baseUW = 1.0f; baseUH = imgAspect / boxAspect; }
        const float u0 = (1.0f - baseUW) * 0.5f, u1 = u0 + baseUW;
        const float v0 = (1.0f - baseUH) * 0.5f, v1 = v0 + baseUH;

        dl->AddImageQuad((ImTextureID)(intptr_t)texId,
            screen[0], screen[1], screen[2], screen[3],
            ImVec2(u0, v0), ImVec2(u1, v0), ImVec2(u1, v1), ImVec2(u0, v1),
            IM_COL32(255, 255, 255, 255));
    } else {
        dl->AddQuadFilled(screen[0], screen[1], screen[2], screen[3], ColA(HT::CardAlt, 255));
    }

    dl->AddQuad(screen[0], screen[1], screen[2], screen[3],
        ColAf(HT::TextPri, 0.12f + (*lift) * 0.14f), 1.5f);
}

Hub::Hub() : m_LastFrameTime(std::chrono::steady_clock::now()) {
    const auto& settings = ProyecThor::Settings::SettingsManager::Get().GetSettings();
    m_SelectedMonitor = settings.projection.targetMonitor;
}

Hub::~Hub() {
    // Puede bloquear un instante si una descarga de subtitulos seguia en
    // curso -- preferible a std::terminate() por destruir un std::thread
    // todavia joinable (mismo criterio que UIManager::Shutdown()).
    if (m_DownloadSubsThread.joinable())
        m_DownloadSubsThread.join();
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
          "Este es un resumen rapido de lo nuevo en esta version. Recorrelo con los botones o los puntos de abajo." },
        { "Ajustes reorganizado",
          "Cada configuracion ahora es su propia pagina, con buscador incluido. Proyeccion y Pantallas quedaron agrupadas juntas, y Red, Mobile, Streaming y OSC pasaron a vivir dentro de Proyeccion en vez de tener su propia categoria aparte." },
        { "Fondos: bucle falso",
          "Nueva opcion en Ajustes > Proyeccion > Fondos: el video reproduce hacia adelante y despues \"hacia atras\" en vez de cortar siempre al mismo frame, dando sensacion de bucle continuo." },
        { "Overlays",
          "Crea textos, formas e imagenes en un editor a pantalla completa y proyectalos como una capa transparente encima del fondo y la letra, desde Biblioteca > Overlay o directo desde Vista en Vivo." },
        { "Vista en Vivo renovada",
          "Reproductor mas simple: Overlays, Chat, Pads y Reloj ahora se abren dentro del mismo panel en vez de ventanas flotantes sueltas." },
        { "Nueva seccion: Pantallas",
          "La configuracion de Stage ahora tiene su propio menu \"Pantallas\" arriba de todo, en vez de estar mezclada con Proyeccion." },
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

    bool vis = ImGui::Begin("##NovedadesPanel", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoMove);

    if (vis) {
        if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            m_NovedadesOpen = false;

        const float headerW = ImGui::GetContentRegionAvail().x;

        ImGui::SetWindowFontScale(1.3f);
        ImGui::PushStyleColor(ImGuiCol_Text, HT::TextPri);
        ImGui::TextUnformatted("Novedades");
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

        if (ImGui::Button("Configuracion inicial", ImVec2(170, 34))) {
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
            ImGui::SetWindowFontScale(1.1f);
            ImGui::PushStyleColor(ImGuiCol_Text, HT::TextPri);
            ImGui::Text("Version v%s", info.version);
            ImGui::PopStyleColor();
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, HT::TextMuted);
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + headerW - (cardCover.id != 0 ? 220.0f : 30.0f));
            ImGui::TextWrapped("%s", info.summary);
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
            ImGui::EndGroup();

            ImGui::EndGroup();

            ImGui::SetCursorScreenPos(cardStartPos);
            if (ImGui::InvisibleButton(info.version, ImVec2(headerW, 140.0f))) {
                m_SelectedUpdateVer = info.id;
                m_IsUpdateModalOpen = true;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

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
            ImGui::Dummy(ImVec2(0.0f, 15.0f));
        };

        const float historyH = ImGui::GetContentRegionAvail().y;
        ImGui::BeginChild("##NovedadesHistory", ImVec2(headerW, historyH), false);
        for (const auto& info : kUpdateRegistry)
            RenderUpdateCard(info);
        ImGui::EndChild();
    }

    ImGui::End();
    ImGui::PopStyleVar(4); // Alpha, WindowRounding, WindowBorderSize, WindowPadding
    ImGui::PopStyleColor(2);
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

    // Abrir/cerrar el panel "Novedades" con N -- mismo criterio que el resto
    // del codebase (cada panel/pantalla chequea sus propias teclas
    // localmente, no hay un archivo central de input). Se ignora mientras el
    // modal de detalle esta abierto para que una sola tecla no controle los
    // dos a la vez.
    if (!m_IsUpdateModalOpen && ImGui::IsKeyPressed(ImGuiKey_N, false))
        m_NovedadesOpen = !m_NovedadesOpen;

    ImGuiViewport* vp = ImGui::GetMainViewport();

    if (!m_BgParticlesInit)
        InitBgParticles(vp->WorkSize.x, vp->WorkSize.y);

    UpdateBgParticles(dt, vp->WorkSize.x, vp->WorkSize.y);
    UpdateNebulas(dt, vp->WorkSize.x, vp->WorkSize.y);

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

    // El canvas de fondo (grilla/particulas/nebulosas + imagen splash_bg2)
    // ahora cubre TODA la ventana -- antes quedaba solo a la derecha del
    // sidebar, con un relleno solido separado a la izquierda y una linea
    // divisoria entre ambos. Las tres columnas de abajo dibujan su propia
    // tarjeta semi-translucida encima de este mismo fondo compartido, en vez
    // de paneles opacos aparte.
    dl->AddRectFilled(wp, ImVec2(wp.x + vp->WorkSize.x, wp.y + vp->WorkSize.y), ColAf(HT::BgMain, appearA));
    RenderBgCanvas(dl, wp, vp->WorkSize.x, vp->WorkSize.y);

    static GLuint s_HubBgTex      = 0;
    static bool   s_HubBgTexTried = false;
    if (!s_HubBgTexTried) {
        s_HubBgTexTried = true;
        s_HubBgTex      = LoadTextureFromFile("splash_bg2.png");
    }
    if (s_HubBgTex != 0)
        dl->AddImage((ImTextureID)(intptr_t)s_HubBgTex, wp, ImVec2(wp.x + vp->WorkSize.x, wp.y + vp->WorkSize.y),
            ImVec2(0, 0), ImVec2(1, 1), ColAf(IM_COL32_WHITE, HT::BgImageAlpha));

    RenderContent(vp->WorkSize.x, vp->WorkSize.y);

    ImGui::PopStyleVar(); // Alpha
    ImGui::End();
    ImGui::PopStyleVar(2);

    RenderNovedadesPanel();
    RenderUpdateDetailModal();
    RenderDownloadSubtitlesPanel();

    if (m_LaunchRequested || m_LibraryOnlyRequested) {
        m_LaunchRequested = false;
        // m_LibraryOnlyRequested se deja tal cual -- UIManager lo lee (para
        // saber a que fue que se salio del Hub) y lo limpia el mismo via
        // ClearLibraryOnlyRequest(), mismo patron que SettingsRequested().
        m_Open            = false;
        return true;
    }

    return false;
}

// Flujo unico de paneles, centrado y usando todo el ancho del Hub --
// reemplaza el viejo layout de 3 columnas fijas (izquierda/centro/derecha).
// Cada seccion (logo, hero "Empezar a proyectar", config con textura,
// Biblioteca/Novedades, accesos rapidos, resumen local) es su propia
// tarjeta redondeada apilada verticalmente, con scroll si no entra todo en
// alto -- ya no hay bloques de fondo solido por columna.
void Hub::RenderContent(float w, float h) {
    // Sin scroll a proposito (pedido explicito): todas las secciones de
    // abajo estan dimensionadas para entrar juntas en una ventana normal de
    // Hub sin necesitar desplazarse.
    ImGui::BeginChild("##HubContent", ImVec2(w, h), false, ImGuiWindowFlags_NoScrollbar);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Ancho maximo de contenido acotado a proposito: dejar que los paneles
    // se estiren al ancho completo de la ventana los deja viendose como
    // barras chatas y desproporcionadas (pedido explicito: "menos anchos").
    const float margin   = 50.0f;
    const float contentW = std::min(720.0f, std::max(320.0f, w - margin * 2.0f));
    const float contentX = (w - contentW) * 0.5f;

    ImGui::SetCursorPos(ImVec2(contentX, 24.0f));
    ImGui::BeginGroup();

    // ── Logo + version, centrados arriba de todo ────────────────────────
    {
        ImFont*     font         = ImGui::GetFont();
        const float logoFontSize = ImGui::GetFontSize() * 1.25f;

        const ImVec2 sizeProyec = font->CalcTextSizeA(logoFontSize, FLT_MAX, 0.0f, "Proyec");
        const ImVec2 sizeThor   = font->CalcTextSizeA(logoFontSize, FLT_MAX, 0.0f, "Thor");
        const float  totalW     = sizeProyec.x + sizeThor.x;

        ImGui::SetCursorPosX(contentX + (contentW - totalW) * 0.5f);
        const ImVec2 logoScreenPos = ImGui::GetCursorScreenPos();
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

        ImGui::Dummy(ImVec2(totalW, logoFontSize));

        const std::string verText = std::string("v") + PROYECTHOR_VERSION_STRING;
        const ImVec2      vSize   = ImGui::CalcTextSize(verText.c_str());
        ImGui::SetCursorPosX(contentX + (contentW - vSize.x) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, HT::TextMuted);
        ImGui::TextUnformatted(verText.c_str());
        ImGui::PopStyleColor();
    }

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

    if (ImGui::Button("Abrir configuracion", ImVec2(w - 60.0f, 36.0f)))
        m_OpenSettingsRequested = true;

        // Degradado oscuro abajo (mismo recurso que el hero de Novedades)
        // para que el texto se lea encima de la foto.
        dl->AddRectFilledMultiColor(
            ImVec2(cfgMin.x, cfgMin.y + cfgH * 0.25f), cfgMax,
            IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 200), IM_COL32(0, 0, 0, 200));

        ImGui::SetCursorScreenPos(ImVec2(cfgMin.x + 18.0f, cfgMax.y - 36.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, HT::TextPri);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextUnformatted("Abrir configuracion");
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, ColA(0xFFFFFFFFu, 190));
        ImGui::TextUnformatted("Apariencia, Proyeccion, Stage y mas");
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(ImVec2(cfgMin.x, cfgMax.y));
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
    }

    ImGui::Dummy(ImVec2(0.0f, 14.0f));

    // Tarjeta chica clickeable generica -- usada para Biblioteca y
    // Novedades, misma altura, dos por fila.
    auto PanelButtonCard = [&](float pw, float ph, const char* title, const std::string& subtitle) -> bool {
        ImGui::PushID(title);
        const ImVec2 pMin = ImGui::GetCursorScreenPos();
        const ImVec2 pMax = ImVec2(pMin.x + pw, pMin.y + ph);

        ImGui::InvisibleButton("##hit", ImVec2(pw, ph));
        const bool  hovered = ImGui::IsItemHovered();
        if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        const bool  clicked = ImGui::IsItemClicked();
        const float hoverT  = HubHoverLerp(ImGui::GetID("##hit"), hovered);

        dl->AddRectFilled(pMin, pMax, ColAf(HT::CardAlt, 0.95f), HT::RadiusLg);
        dl->AddRect(pMin, pMax, ColAf(HT::AccentBlue, 0.10f + hoverT * 0.35f), HT::RadiusLg, 0, 1.0f + hoverT);
        if (hoverT > 0.001f)
            dl->AddRectFilled(pMin, pMax, ColAf(HT::TextPri, 0.04f * hoverT), HT::RadiusLg);

        ImGui::SetCursorScreenPos(ImVec2(pMin.x + 18.0f, pMin.y + 14.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, HT::AccentBlue);
        ImGui::SetWindowFontScale(1.08f);
        ImGui::TextUnformatted(title);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
        ImGui::SetCursorScreenPos(ImVec2(pMin.x + 18.0f, pMin.y + 40.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, HT::TextMuted);
        ImGui::TextUnformatted(subtitle.c_str());
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(ImVec2(pMin.x, pMax.y));
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
        ImGui::PopID();
        return clicked;
    };

    // ── Fila: Biblioteca + Novedades + Descargar subtitulos ──────────────
    {
        const float rowGap = 16.0f;
        const float thirdW = (contentW - rowGap * 2.0f) / 3.0f;
        const float rowH   = 62.0f;

        ImGui::SetCursorPosX(contentX);
        const ImVec2 rowStart = ImGui::GetCursorScreenPos();

        // Acceso rapido a Biblioteca: entra al workspace pero mostrando
        // solo el panel de Biblioteca (con Render incluido, ya es una
        // pestaña de ese mismo panel), sin Home/Vista en Vivo/Diseño
        // alrededor.
        if (PanelButtonCard(thirdW, rowH, "Biblioteca", "Solo el panel de Biblioteca, con Render incluido"))
            m_LibraryOnlyRequested = true;

        ImGui::SetCursorScreenPos(ImVec2(rowStart.x + thirdW + rowGap, rowStart.y));
        const UpdateVersionInfo* latestForRow = kUpdateRegistry.empty() ? nullptr : &kUpdateRegistry[0];
        const std::string novSub = std::string("v") +
            (latestForRow ? latestForRow->version : PROYECTHOR_VERSION_STRING) + " disponible  -  tecla N";
        if (PanelButtonCard(thirdW, rowH, "Novedades", novSub))
            m_NovedadesOpen = true;

        ImGui::SetCursorScreenPos(ImVec2(rowStart.x + (thirdW + rowGap) * 2.0f, rowStart.y));
        if (PanelButtonCard(thirdW, rowH, "Descargar subtitulos", "Bajalos como .txt desde una URL"))
            m_DownloadSubsOpen = true;

        ImGui::SetCursorScreenPos(ImVec2(rowStart.x, rowStart.y + rowH));
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
    }

    ImGui::Dummy(ImVec2(0.0f, 14.0f));

    ImGui::SetCursorPosX(30.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, HT::TextMuted);
    ImGui::TextUnformatted("Accesos rapidos");
    ImGui::PopStyleColor();

        // Indices de k_Categories en SettingsPanel.cpp (0=Apariencia,
        // 1=Proyeccion, 2=Stage, 3=Audio, 4=Canciones, 5=Teclas, 6=Idioma,
        // 7=Actualizaciones). "General" se quito del todo (pedido
        // explicito, no se usaba), de ahi que ya no aparezca aca.
        struct QuickItem { const char* label; int tab; };
        static const QuickItem items[] = {
            { "Apariencia",      0 },
            { "Proyección",      1 },
            { "Stage",           2 },
            { "Idioma",          6 },
            { "Actualizaciones", 7 },
        };
        const int   count  = (int)(sizeof(items) / sizeof(items[0]));
        const float btnGap = 10.0f;
        const float btnW   = (contentW - 36.0f - btnGap * (count - 1)) / count;

        ImGui::SetCursorScreenPos(ImVec2(qMin.x + 18.0f, qMin.y + 30.0f));
        ImGui::PushStyleColor(ImGuiCol_Button,        HT::Surface);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HT::SurfaceHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  HT::SurfaceActive);
        ImGui::PushStyleColor(ImGuiCol_Text,          HT::TextPri);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, HT::RadiusSm);
        for (int i = 0; i < count; i++) {
            if (i > 0) ImGui::SameLine(0.0f, btnGap);
            char id[64];
            snprintf(id, sizeof(id), "%s##qb%d", items[i].label, items[i].tab);
            if (ImGui::Button(id, ImVec2(btnW, 26.0f))) {
                m_ActiveTab             = items[i].tab;
                m_OpenSettingsRequested = true;
            }
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);

        ImGui::SetCursorScreenPos(ImVec2(qMin.x, qMax.y));
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
    }

    ImGui::Dummy(ImVec2(0.0f, 14.0f));

    // ── Resumen local -- un solo panel compacto (pedido explicito: no
    // mostrar todo de una con tarjetas y lista de canciones expandidas) ──
    RenderResumenLocalSection(contentW);

    ImGui::EndGroup();
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

    // Encabezado de seccion con una linea sutil debajo (mismo estilo "Cat()"
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
        ImGui::Text("Version v%s", info.version);
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
        ImGui::SetCursorScreenPos(ImVec2(x, pMin.y + 52.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, HT::TextMuted);
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
    };

    const float colW = (w - 36.0f) / 3.0f;
    Stat(pMin.x + 18.0f,               "Proyecciones totales", std::to_string(totalProjections), HT::Success);
    Stat(pMin.x + 18.0f + colW,        "FPS promedio",         std::to_string(perfSummary.first), HT::AccentBlue);

    std::string topLabel = "Cancion mas proyectada";
    std::string topValue = "Sin datos aun";
    if (!topSongs.empty()) {
        topValue = topSongs[0].first;
        if (topValue.size() > 20) {
            topValue.resize(20);
            // Evita cortar a mitad de un caracter UTF-8 multibyte (tildes/ñ).
            while (!topValue.empty() && (static_cast<unsigned char>(topValue.back()) & 0xC0) == 0x80)
                topValue.pop_back();
            topValue += "...";
        }
        topLabel = "Mas proyectada (" + std::to_string(topSongs[0].second) + ")";
    }
    Stat(pMin.x + 18.0f + colW * 2.0f, topLabel.c_str(), topValue, HT::TextPri);

    ImGui::SetCursorScreenPos(ImVec2(pMin.x, pMax.y));
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
}

// "Descargar subtitulos" -- utilidad standalone del Hub: pega una URL,
// se bajan sus subtitulos (mismo fetch que "Importar desde URL" del menu
// Archivo, ver SubtitleImporter.h) y se guardan como .txt suelto, sin crear
// una cancion. "Guardar en" sigue el mismo patron de Biblioteca > Render
// (preguntar cada vez via dialogo nativo, o una carpeta fija).
void Hub::RenderDownloadSubtitlesPanel() {
    // Se consume el resultado (y se une el hilo) apenas esta listo, SIEMPRE
    // -- mismo motivo que UIManager::RenderUrlImportModal: si no, un
    // intento nuevo mas tarde pisaria con "=" un std::thread todavia no
    // unido y std::terminate() explota.
    bool resultReady = false;
    ProyecThor::Core::SubtitleFetchResult resultCopy;
    {
        std::lock_guard<std::mutex> lk(m_DownloadSubsMutex);
        if (m_DownloadSubsResult.has_value() && !m_DownloadSubsRunning) {
            resultCopy  = *m_DownloadSubsResult;
            resultReady = true;
            m_DownloadSubsResult.reset();
        }
    }
    if (resultReady) {
        if (m_DownloadSubsThread.joinable())
            m_DownloadSubsThread.join();

        if (resultCopy.success) {
            std::string savePath;
            if (m_DownloadSubsAskEachTime || m_DownloadSubsPresetFolder.empty()) {
                std::string suggested = resultCopy.title.empty() ? "subtitulos" : resultCopy.title;
                savePath = ProyecThor::UI::PickSaveTextPath(
                    (m_DownloadSubsPresetFolder.empty() ? suggested : (m_DownloadSubsPresetFolder + "/" + suggested)) + ".txt");
            } else {
                // Carpeta fija: nombre automatico a partir del titulo, con
                // el mismo criterio anti-colision que ya usa el conversor
                // de Render (agrega " (2)", " (3)"... si ya existe).
                std::string base = resultCopy.title.empty() ? "subtitulos" : resultCopy.title;
                std::string candidate = m_DownloadSubsPresetFolder + "/" + base + ".txt";
                int suffix = 2;
                while (std::filesystem::exists(candidate)) {
                    candidate = m_DownloadSubsPresetFolder + "/" + base + " (" + std::to_string(suffix) + ").txt";
                    ++suffix;
                }
                savePath = candidate;
            }

            if (!savePath.empty()) {
                std::ofstream f(savePath, std::ios::binary);
                if (f.is_open()) {
                    f << "\xEF\xBB\xBF" << resultCopy.lyrics;
                    f.close();
                    m_DownloadSubsSavedPath = savePath;
                    m_DownloadSubsLastError.clear();
                } else {
                    m_DownloadSubsLastError = "No se pudo escribir el archivo en esa ubicacion.";
                }
            }
            // savePath vacio == el operador cancelo el dialogo -- no es un
            // error, simplemente no se guarda nada y queda listo para
            // reintentar sin perder el texto ya descargado.
        } else {
            m_DownloadSubsLastError = resultCopy.error;
        }
    }

    if (!m_DownloadSubsOpen) return;

    const ImVec2 baseSize(480.0f, 260.0f);
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 workCenter(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f);
    ImGui::SetNextWindowPos(workCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(baseSize, ImGuiCond_Appearing);

    ImGuiWindowClass floatingClass;
    floatingClass.DockingAllowUnclassed = false;
    ImGui::SetNextWindowClass(&floatingClass);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 16.0f));
    bool open = ImGui::Begin("Descargar subtitulos", &m_DownloadSubsOpen,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_AlwaysAutoResize);

    if (open) {
        ImGui::TextWrapped("Pega el link de un video. Se buscan sus subtitulos (español primero, si "
                            "no ingles) y se guardan como un .txt suelto -- no crea una cancion.");
        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        ImGui::BeginDisabled(m_DownloadSubsRunning);
        ImGui::SetNextItemWidth(-1.0f);
        bool enterPressed = ImGui::InputTextWithHint("##dlSubsUrl", "https://www.youtube.com/watch?v=...",
            m_DownloadSubsUrlBuf, sizeof(m_DownloadSubsUrlBuf), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::EndDisabled();

        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        // ── Guardar en -- mismo patron que Biblioteca > Render ───────────
        ImGui::PushStyleColor(ImGuiCol_Text, HT::TextMuted);
        ImGui::TextUnformatted("Guardar en");
        ImGui::PopStyleColor();
        if (ImGui::RadioButton("Preguntar cada vez", m_DownloadSubsAskEachTime))
            m_DownloadSubsAskEachTime = true;
        if (ImGui::RadioButton("Carpeta fija", !m_DownloadSubsAskEachTime))
            m_DownloadSubsAskEachTime = false;

        if (!m_DownloadSubsAskEachTime) {
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            char folderBuf[512];
            std::snprintf(folderBuf, sizeof(folderBuf), "%s",
                m_DownloadSubsPresetFolder.empty() ? "Sin elegir..." : m_DownloadSubsPresetFolder.c_str());
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 96.0f);
            ImGui::InputText("##dlSubsPresetFolder", folderBuf, sizeof(folderBuf), ImGuiInputTextFlags_ReadOnly);
            ImGui::SameLine();
            if (ImGui::Button("Elegir...", ImVec2(86.0f, 0.0f))) {
                std::string chosen = ProyecThor::UI::PickFolder("Elegir carpeta para subtitulos descargados");
                if (!chosen.empty()) m_DownloadSubsPresetFolder = chosen;
            }
        }

        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        bool wantStart = false;
        if (m_DownloadSubsRunning) {
            ImGui::TextColored(ImVec4(0.6f, 0.75f, 0.9f, 1.0f), "Buscando subtitulos...");
        } else {
            if (ImGui::Button("Descargar", ImVec2(120.0f, 32.0f)))
                wantStart = true;
            if (enterPressed)
                wantStart = true;
            ImGui::SameLine();
            if (ImGui::Button("Cerrar", ImVec2(100.0f, 32.0f))) {
                m_DownloadSubsOpen = false;
                m_DownloadSubsLastError.clear();
            }
        }

        if (!m_DownloadSubsLastError.empty()) {
            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.93f, 0.35f, 0.35f, 1.0f));
            ImGui::TextWrapped("%s", m_DownloadSubsLastError.c_str());
            ImGui::PopStyleColor();
        }
        if (!m_DownloadSubsSavedPath.empty()) {
            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, HT::Success);
            ImGui::TextWrapped("Guardado en: %s", m_DownloadSubsSavedPath.c_str());
            ImGui::PopStyleColor();
        }

        if (wantStart && !m_DownloadSubsRunning && m_DownloadSubsUrlBuf[0] != '\0') {
            if (m_DownloadSubsThread.joinable()) m_DownloadSubsThread.join(); // por si quedo un intento anterior sin unir
            m_DownloadSubsLastError.clear();
            m_DownloadSubsSavedPath.clear();
            m_DownloadSubsRunning = true;
            {
                std::lock_guard<std::mutex> lk(m_DownloadSubsMutex);
                m_DownloadSubsResult.reset();
            }
            std::string urlCopy = m_DownloadSubsUrlBuf;
            m_DownloadSubsThread = std::thread([this, urlCopy]() {
                ProyecThor::Core::SubtitleFetchResult res = ProyecThor::Core::FetchSubtitlesAsLyrics(urlCopy);
                std::lock_guard<std::mutex> lk(m_DownloadSubsMutex);
                m_DownloadSubsResult  = std::move(res);
                m_DownloadSubsRunning = false;
            });
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

// Modal universal de detalle de actualizacion -- sin cambios de logica
// respecto a la version anterior, solo movido a su propio metodo (antes
// vivia al final de RenderMainContent) y usando m_SelectedUpdateVer /
// m_IsUpdateModalOpen (miembros) en vez de estaticos locales, para que tanto
// el hero como la lista de historial dentro de RenderNovedadesPanel puedan
// abrirlo.
void Hub::RenderUpdateDetailModal() {
    // selectedUpdateVer queda como alias de solo lectura del miembro: el
    // resto de este metodo (el gran if/else por version) lo referencia tal
    // cual estaba antes, sin necesidad de tocar ese bloque.
    const int selectedUpdateVer = m_SelectedUpdateVer;

    // m_UpdateModalAnim se aproxima a 1 mientras m_IsUpdateModalOpen y decae
    // a 0 al cerrar; el modal sigue dibujandose (con escala/alpha
    // decrecientes) hasta que la animacion termina, en vez de desaparecer de
    // golpe.
    {
        const float target = m_IsUpdateModalOpen ? 1.0f : 0.0f;
        m_UpdateModalAnim += (target - m_UpdateModalAnim) * std::min(1.0f, ImGui::GetIO().DeltaTime * 10.0f);
        m_UpdateModalAnim = std::clamp(m_UpdateModalAnim, 0.0f, 1.0f);
        if (m_UpdateModalAnim < 0.001f) m_UpdateModalAnim = 0.0f;
    }

    if (m_IsUpdateModalOpen || m_UpdateModalAnim > 0.0f) {
        const UpdateVersionInfo* selInfo = FindUpdateVersion(selectedUpdateVer);
        const GLTextureInfo modalCover = selInfo ? GetCoverTexture(selInfo->coverFile) : GLTextureInfo{};

        ImGuiViewport* vp     = ImGui::GetMainViewport();
        const float     fadeA = EaseOut(m_UpdateModalAnim);

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
                    stateKey, ImGui::GetIO().DeltaTime, headerHovered, 1.06f);
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

            if (selectedUpdateVer == 13) { // v0.6.0
                Cat("App movil: editor de overlays");
                Bul("Edicion completa de overlays desde el celular: mover, redimensionar y rotar capas con gestos, igual que en la PC.");
                Bul("Seleccion multiple con recuadro de arrastre (rubber-band), guias de iman para alinear capas entre si, y una barra con el tamaño en pixeles mientras moves o redimensionas.");
                Bul("Panel de capas reordenable arrastrando (igual que en la PC), y dos herramientas nuevas: Borrador y Degradado, con edicion real de pixeles.");
                Bul("Boton \"Enviar al PC\": exporta el overlay y lo sube directo a la app de escritorio sin pasar por USB ni un cable.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Biblioteca > Render");
                Bul("Eleccion de codec de video al convertir (H.264, H.265, VP9 o AV1) y un control deslizante de compresion.");
                Bul("Boton para cancelar una conversion a mitad de camino, con una barra de progreso real en vez de una animacion generica.");
                Bul("Estimacion del peso final antes de convertir, y comparacion exacta de antes/despues una vez termina.");
                Bul("Podes elegir si guardar siempre en una carpeta fija o que te pregunte cada vez, con el mismo dialogo nativo de siempre.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Soporte real para Linux y CachyOS");
                Bul("ProyecThor ahora compila y corre en Linux de verdad: paquete para Arch/CachyOS validado automaticamente en cada version.");
                Bul("Funciona tanto en X11 como en Wayland (via XWayland), incluyendo en escritorios como el de CachyOS.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Windows: escalado de pantalla (DPI)");
                Bul("La app ahora respeta el porcentaje de escalado de Windows (125%, 150%, etc.): en laptops con pantallas de alta densidad, la letra y los botones ya no se ven diminutos.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Aviso");
                Bul("Esta es una actualizacion grande y todavia esta en beta / en construccion: pueden aparecer ajustes y correcciones adicionales en las proximas versiones menores.");
                ImGui::Dummy(ImVec2(0,12));
            } else if (selectedUpdateVer == 12) { // v0.5.1
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
                m_IsUpdateModalOpen = false;
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
        }

        ImGui::End();
        ImGui::PopStyleVar(4); // Alpha, WindowRounding, WindowBorderSize, WindowPadding
        ImGui::PopStyleColor(2);
    }
}

} // namespace ProyecThor::UI
