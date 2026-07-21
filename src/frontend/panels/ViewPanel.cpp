#include "ViewPanel.h"
#include "UIManager.h"
#include "DesignSystem.h"
#include "frontend/ui/bin/StyleGeneralApp.h"
#include "backend/core/PresentationCore.h"
#include "backend/settings/SettingsManager.h"
#include "frontend/views/Announcements.h"
#include "frontend/views/OClock.h"
#include "capture/CapturePanel.h"
#include "MonitorTheme.h"
#include "MonitorDesign.h"
#include "MonitorUIHelpers.h"
#include "frontend/ui/LoadingSpinner.h"
#include "frontend/ui/AppIcons.h"
#include "frontend/panels/home/HomeIcons.h"
#include "frontend/ui/IconRail.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <iostream>
#include <GLFW/glfw3.h>

namespace ProyecThor::UI {

static constexpr float kQuickActionsRailW = 40.0f;
// Alto reservado para el transporte del player "general" (ver
// RenderLiveTransport) debajo del video — mismo contenido que tenia
// MonitorLiveControls, reflowado a un layout ancho/bajo en vez de
// angosto/alto para que quepa junto al video en vez de a un costado.
static constexpr float kLiveTransportH = 180.0f;

namespace {

namespace MT = MonitorTheme;
using namespace Design;
using namespace Components;

// Copia de MonitorView::DrawIconButton — no se puede reusar el metodo
// privado de esa clase desde aca, y es lo bastante chico (busca la textura
// en StyleGeneralApp::Icons y la dibuja centrada con feedback de "hundido"
// al mantener presionado) como para no justificar extraerlo a un helper
// compartido todavia.
bool DrawIconButton(const char* iconName, float size,
                    ImVec4 bgCol, ImVec4 hov, ImVec4 act,
                    ImVec2 btnSize, bool isActiveState = false)
{
    ImTextureID tex = (ImTextureID)0;
    auto it = StyleGeneralApp::Icons.find(iconName);
    if (it != StyleGeneralApp::Icons.end() && it->second.textureID)
        tex = (ImTextureID)(intptr_t)it->second.textureID;

    ImVec4 finalBg = isActiveState ? act : bgCol;

    ImGui::PushStyleColor(ImGuiCol_Button,        finalBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  act);

    bool pressed = ImGui::Button("", btnSize);
    bool isHeld  = ImGui::IsItemActive();

    ImVec2 p = ImGui::GetItemRectMin();
    ImVec2 s = ImGui::GetItemRectSize();

    float offsetY = isHeld ? 2.0f : 0.0f;
    ImU32 tintCol = isHeld
        ? ImGui::GetColorU32(ImVec4(0.8f, 0.8f, 0.8f, 1.0f))
        : ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

    ImGui::GetWindowDrawList()->AddImage(
        tex,
        { p.x + (s.x - size) * 0.5f, p.y + (s.y - size) * 0.5f + offsetY },
        { p.x + (s.x + size) * 0.5f, p.y + (s.y + size) * 0.5f + offsetY },
        ImVec2(0, 0), ImVec2(1, 1), tintCol);

    ImGui::PopStyleColor(3);
    return pressed;
}

ImVec4 ToVec4(ImU32 col) { return ImGui::ColorConvertU32ToFloat4(col); }
ImVec4 Brighten(const ImVec4& c, float amount)
{
    return ImVec4(
        std::clamp(c.x + amount, 0.0f, 1.0f),
        std::clamp(c.y + amount, 0.0f, 1.0f),
        std::clamp(c.z + amount, 0.0f, 1.0f),
        c.w);
}

// Disco/vinilo — para "Detener disco en vivo" (no hay textura "album" cargada
// en StyleGeneralApp::Icons; se dibuja a mano con el mismo estilo geometrico
// que HomeIcons::DrawIcon_Clock/DrawIcon_Broadcast en vez de agregar un PNG).
void DrawIcon_Disc(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    ImVec2 center = { o.x + sz * 0.5f, o.y + sz * 0.5f };
    dl->AddCircle(center, sz * 0.40f, col, 24, sz * 0.06f);
    dl->AddCircle(center, sz * 0.24f, col, 20, sz * 0.035f);
    dl->AddCircleFilled(center, sz * 0.07f, col, 12);
}

// Engranaje — para "Ajustes". Mismo motivo que DrawIcon_Disc: sin textura
// "settings" cargada, y el glifo de fallback ("...") no es un icono real.
void DrawIcon_Gear(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    ImVec2 center = { o.x + sz * 0.5f, o.y + sz * 0.5f };
    float  bodyR  = sz * 0.26f;
    dl->AddCircle(center, bodyR, col, 24, sz * 0.10f);
    dl->AddCircleFilled(center, sz * 0.07f, col, 10);

    const int   teeth   = 8;
    const float toothLen = sz * 0.12f;
    const float toothW   = sz * 0.09f;
    for (int i = 0; i < teeth; i++)
    {
        float  a    = (IM_PI * 2.0f / teeth) * (float)i;
        ImVec2 dir  = { cosf(a), sinf(a) };
        ImVec2 perp = { -dir.y, dir.x };
        ImVec2 base = { center.x + dir.x * bodyR, center.y + dir.y * bodyR };
        ImVec2 tip  = { center.x + dir.x * (bodyR + toothLen), center.y + dir.y * (bodyR + toothLen) };
        ImVec2 p0   = { base.x + perp.x * toothW * 0.5f, base.y + perp.y * toothW * 0.5f };
        ImVec2 p1   = { base.x - perp.x * toothW * 0.5f, base.y - perp.y * toothW * 0.5f };
        ImVec2 p2   = { tip.x  - perp.x * toothW * 0.35f, tip.y  - perp.y * toothW * 0.35f };
        ImVec2 p3   = { tip.x  + perp.x * toothW * 0.35f, tip.y  + perp.y * toothW * 0.35f };
        dl->AddQuadFilled(p0, p3, p2, p1, col);
    }
}

// Botón de celda plano — sin esquinas redondeadas, ancho completo del riel y
// separador inferior de 1px: da el efecto de "grilla" tipo hoja de cálculo
// (Excel) / toolbar de Holyrics-ProPresenter en vez de tarjetas vistosas.
//
// iconKey busca una textura en StyleGeneralApp::Icons; si no hay ninguna
// registrada con ese nombre, se usa vectorIcon (dibujado a mano, ver
// AppIcons.h/HomeIcons.h/DrawIcon_Disc/DrawIcon_Gear) en su lugar — el
// pedido explicito fue "no quiero letras", asi que el glifo de texto ya no
// se usa como ultimo recurso salvo que ninguno de los dos este disponible.
bool QuickActionButton(const char* id, const char* iconKey, DrawIconFn vectorIcon,
                       const char* fallbackGlyph, const char* tooltip, ImVec2 size,
                       ImVec4 bgColor, ImVec4 hoverColor, ImVec4 activeColor, ImVec4 tint,
                       bool toggledOn)
{
    ImVec4 restColor = toggledOn ? activeColor : bgColor;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        restColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  activeColor);
    ImGui::PushStyleColor(ImGuiCol_Text,          tint);

    auto it = StyleGeneralApp::Icons.find(iconKey);
    bool hasTexture = (it != StyleGeneralApp::Icons.end() && it->second.textureID != nullptr);
    bool hasIcon    = hasTexture || vectorIcon != nullptr;
    std::string label = (hasIcon ? "" : std::string(fallbackGlyph)) + "##" + id;

    bool clicked = ImGui::Button(label.c_str(), size);

    ImVec2 bMin = ImGui::GetItemRectMin();
    ImVec2 bMax = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (hasTexture)
    {
        const float iconSide = std::min(size.x, size.y) * 0.42f;
        const ImVec2 center  = { (bMin.x + bMax.x) * 0.5f, (bMin.y + bMax.y) * 0.5f };
        const ImVec2 pMin    = { center.x - iconSide * 0.5f, center.y - iconSide * 0.5f };
        const ImVec2 pMax    = { center.x + iconSide * 0.5f, center.y + iconSide * 0.5f };

        dl->AddImage(it->second.textureID, pMin, pMax,
            ImVec2(0, 0), ImVec2(1, 1),
            ImGui::ColorConvertFloat4ToU32(tint));
    }
    else if (vectorIcon)
    {
        const float iconSide = std::min(size.x, size.y) * 0.48f;
        const ImVec2 center  = { (bMin.x + bMax.x) * 0.5f, (bMin.y + bMax.y) * 0.5f };
        const ImVec2 origin  = { center.x - iconSide * 0.5f, center.y - iconSide * 0.5f };
        vectorIcon(dl, origin, iconSide, ImGui::ColorConvertFloat4ToU32(tint));
    }

    // Línea fina de "celda" — misma idea que los bordes de una hoja de cálculo.
    dl->AddLine({ bMin.x, bMax.y }, { bMax.x, bMax.y }, IM_COL32(0, 0, 0, 120), 1.0f);

    // Barra izquierda delgada cuando el estado está activo/encendido.
    if (toggledOn)
    {
        ImU32 accent = ImGui::ColorConvertFloat4ToU32(tint);
        dl->AddRectFilled({ bMin.x, bMin.y + 3.0f }, { bMin.x + 2.0f, bMax.y - 3.0f }, accent);
    }

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();

    if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        ImGui::SetTooltip("%s", tooltip);

    return clicked;
}

} // namespace

void ViewPanel::Render()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.04f, 0.04f, 0.06f, 1.0f));

    bool visible = ImGui::Begin("Vista en Vivo");

    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(1);

    if (visible)
    {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        // Riel opcional desde Vista > "Botones de limpieza (Vista en Vivo)"
        // — apagado, el video se queda con todo el ancho.
        const bool  showQuickActions = ProyecThor::Settings::SettingsManager::Get()
                                            .GetSettings().general.showViewQuickActions;
        const float railW    = showQuickActions ? kQuickActionsRailW : 0.0f;
        const float contentW = std::max(0.0f, avail.x - railW);

        // Children con padding cero — el estilo global usa WindowPadding
        // (22,18), que aquí sólo recortaría el video y el riel angosto.
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        if (contentW > 8.0f && avail.y > 8.0f)
        {
            ImGui::BeginChild("##viewVideoArea", ImVec2(contentW, avail.y), false,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

            // Puntos "Público"/"Stage" arriba de todo — reemplazan el boton
            // "Iniciar proyección" de ControlPanel (eliminado, ver
            // ToggleAudience/ToggleStageQuick).
            const float dotsH = 28.0f;
            RenderStatusDots(contentW);

            // FIX: antes el video se quedaba con "lo que sobraba" despues
            // de reservarle a Transport/Control Overlays un piso fijo, asi
            // que en un panel angosto y alto el video terminaba con MAS
            // alto reservado del que en realidad necesita para su relacion
            // de aspecto (RenderContent letterboxea puertas adentro), y esa
            // franja de negro "desperdiciada" no se le devolvia a los
            // paneles de abajo, que quedaban apretados. Ahora se calcula
            // primero cuanto alto necesita el video para llenar el ancho
            // disponible sin barras (su relacion de aspecto real, la del
            // monitor de salida), y lo que sobra se reparte generosamente
            // entre Transport y Control Overlays.
            float srcAspect = 1920.0f / 1080.0f;
            {
                int monitorCount = 0;
                GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
                auto state = Core::PresentationCore::Get().GetState();
                if (monitors && monitorCount > 0 &&
                    state.targetMonitorIndex >= 0 && state.targetMonitorIndex < monitorCount) {
                    if (const GLFWvidmode* mode = glfwGetVideoMode(monitors[state.targetMonitorIndex]);
                        mode && mode->width > 0 && mode->height > 0) {
                        srcAspect = (float)mode->width / (float)mode->height;
                    }
                }
            }

            const float remain2   = std::max(0.0f, avail.y - dotsH);
            const float minVideoH = 40.0f;

            // El video nunca se lleva mas del 65% de lo que queda, aunque
            // "quisiera" mas (relacion de aspecto muy vertical) — asi el
            // transporte siempre conserva un piso usable. std::clamp() en
            // este libstdc++ hace assert si hi < lo, y con remain2 chico
            // (panel muy bajo) "remain2*0.65f" puede quedar por debajo de
            // minVideoH — de ahi el std::max() en cada limite superior, para
            // que el clamp nunca reciba un rango invertido pase lo que pase
            // con el alto disponible.
            //
            // FIX: antes esta cuenta tambien le reservaba un piso fijo a
            // "Control Overlays" (kControlOverlaysH) dentro de este mismo
            // panel — se movio a ViewToolsPanel (panel propio debajo de
            // este), asi que ese espacio vuelve integro al video/transporte.
            float naturalVideoH = contentW / std::max(0.1f, srcAspect);
            float videoH     = std::clamp(naturalVideoH, minVideoH, std::max(minVideoH, remain2 * 0.65f));
            float transportH = std::clamp(remain2 - videoH, 60.0f, std::max(60.0f, kLiveTransportH * 1.6f));
            // Reajuste final: cualquier resto (por los clamps de arriba)
            // vuelve al video en vez de perderse como espacio muerto.
            videoH = std::max(0.0f, remain2 - transportH);

            RenderContent(contentW, videoH);

            // RenderContent centra el video (letterbox) y puede dejar el
            // cursor antes de videoH — se fuerza la posicion para que cada
            // seccion arranque justo donde corresponde, sin importar cuanto
            // del alto reservado ocupo el letterbox.
            ImGui::SetCursorPosY(dotsH + videoH);
            RenderLiveTransport(contentW, transportH);

            ImGui::EndChild();
        }

        if (showQuickActions)
        {
            ImGui::SameLine(0.0f, 0.0f);

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.06f, 0.09f, 1.0f));
            ImGui::BeginChild("##viewQuickActions", ImVec2(railW, avail.y), false,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            RenderQuickActions(railW, avail.y);
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

        ImGui::PopStyleVar();
    }

    ImGui::End();
}

void ViewPanel::RenderQuickActions(float railW, float railH)
{
    (void)railH;
    auto& core = Core::PresentationCore::Get();
    bool stretchOn = core.GetStretchToFill();
    bool isMuted   = core.GetLiveMute();

    auto* audioPanel   = core.GetAudioPanelRef();
    auto* announcements= core.GetAnnouncementsRef();
    auto* oclock       = core.GetOClockRef();
    auto* capturePanel = core.GetCapturePanelRef();

    // Cada uno de estos refleja si TODAVIA hay algo de ESE tipo especifico
    // para limpiar — el boton se resalta (amarillo) mientras la funcion que
    // "elimina" sigue activa, y se apaga solo apenas se limpia. En vez de
    // resaltar por hover como el resto de la app, ver pedido original.
    bool showText  = core.GetState().showText;
    bool discLive  = core.GetState().bgType == Core::PresentationState::BackgroundType::Audio;
    bool bgLive    = core.GetState().bgType != Core::PresentationState::BackgroundType::SolidColor;
    bool imgLive   = core.IsOverlayActive();
    bool annLive   = announcements && announcements->IsLive();
    bool clockLive = oclock && oclock->IsLive();
    bool capLive   = capturePanel && capturePanel->IsLive();

    ImVec4 baseFill     = ToVec4(DS::BtnDefaultFill);
    ImVec4 hoverClear   = ToVec4(DS::AccentColorDim);
    ImVec4 activeStretch= ToVec4((DS::AccentColor & 0x00FFFFFFu) | (140u << 24));
    ImVec4 hoverMute    = ToVec4(DS::DangerColor);
    ImVec4 activeMute   = Brighten(ToVec4(DS::DangerColor), 0.12f);
    ImVec4 activeContent= Design::k_EQ_Yellow;
    ImVec4 textPrimary  = ToVec4(DS::TextPrimary);
    ImVec4 textDanger   = ToVec4(DS::DangerColor);
    ImVec4 tintOnYellow = ImVec4(0.10f, 0.09f, 0.06f, 1.0f);

    struct ActionDef {
        const char* id;
        const char* icon;          // clave en StyleGeneralApp::Icons (textura), o "" si no hay
        DrawIconFn  vectorIcon;    // dibujado a mano — se usa si icon no tiene textura cargada
        const char* fallbackGlyph; // ultimo recurso si ninguno de los dos aplica (no deberia pasar)
        const char* tooltip;
        ImVec4      hoverColor;
        ImVec4      activeColor;
        bool        toggledOn;
        ImVec4      tint;
    };

    // Los primeros 7 son "Limpiar <tipo especifico>" (uno por cada capa de
    // contenido que puede estar en vivo), y despues del espaciador quedan
    // las utilidades (proporcion/mute/ajustes) que ya estaban. Pedido
    // explicito: solo iconos, nada de letras — donde no habia una textura ya
    // cargada (album/imagen/campana/reloj/camara/engranaje) se reusan los
    // iconos vectoriales ya dibujados a mano en otras partes de la app
    // (AppIcons.h/HomeIcons.h) o se agregan nuevos chicos aca mismo (Disco,
    // Engranaje) — ver DrawIcon_Disc/DrawIcon_Gear arriba.
    ActionDef actions[10] = {
        { "vaClearText", "", AppIcons::DrawIcon_TextAa, "Aa", "Limpiar texto",
          hoverClear, activeContent, showText,  showText  ? tintOnYellow : textPrimary },
        { "vaClearDisc", "", DrawIcon_Disc, "Dsc", "Detener disco en vivo",
          hoverClear, activeContent, discLive,  discLive  ? tintOnYellow : textPrimary },
        { "vaClearBg",   "delete", nullptr, "BG",  "Quitar fondo",
          hoverClear, activeContent, bgLive,    bgLive    ? tintOnYellow : textPrimary },
        { "vaClearImg",  "", AppIcons::DrawIcon_Overlay, "Img", "Overlays",
          hoverClear, activeContent, imgLive,   imgLive   ? tintOnYellow : textPrimary },
        { "vaClearAnn",  "", HomeIcons::DrawIcon_Megaphone, "Anc", "Detener anuncios",
          hoverClear, activeContent, annLive,   annLive   ? tintOnYellow : textPrimary },
        { "vaClearClock","", HomeIcons::DrawIcon_Clock, "Rlj", "Quitar reloj",
          hoverClear, activeContent, clockLive, clockLive ? tintOnYellow : textPrimary },
        { "vaClearCap",  "", HomeIcons::DrawIcon_Camera, "Cap", "Detener captura",
          hoverClear, activeContent, capLive,   capLive   ? tintOnYellow : textPrimary },

        { "vaStretch",   stretchOn ? "original_screen" : "fit_screen", nullptr, stretchOn ? "1:1" : "Fit",
          "Alternar proporción", hoverClear, activeStretch, stretchOn, textPrimary },
        { "vaMute",      isMuted ? "no_sound" : "volume_up", nullptr, isMuted ? "Mute" : "Vol",
          "Mutear / Desmutear audio vivo", isMuted ? hoverMute : hoverClear, activeMute, isMuted,
          isMuted ? textDanger : textPrimary },
        { "vaPrefs",     "", DrawIcon_Gear, "...", "Ajustes",
          hoverClear, baseFill, false, textPrimary },
    };

    // Celdas de ancho completo, pegadas unas a otras (separadas solo por la
    // línea de 1px que dibuja QuickActionButton) — look de toolbar plano,
    // no de tarjetas sueltas.
    const ImVec2 cellSize(railW, 34.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::Dummy(ImVec2(railW, 1.0f));

    for (int i = 0; i < 10; i++)
    {
        if (i == 7) ImGui::Dummy(ImVec2(railW, 10.0f)); // separa utilidades de los "Limpiar"

        if (QuickActionButton(actions[i].id, actions[i].icon, actions[i].vectorIcon, actions[i].fallbackGlyph,
                               actions[i].tooltip, cellSize, baseFill, actions[i].hoverColor,
                               actions[i].activeColor, actions[i].tint, actions[i].toggledOn))
        {
            if (i == 0)      core.ClearLayer2();
            else if (i == 1) core.StopBackgroundMedia();
            else if (i == 2) core.StopBackgroundMedia();
            else if (i == 3) core.StopOverlayMedia();
            else if (i == 4 && announcements) announcements->SetLive(false);
            else if (i == 5 && oclock)        oclock->StopTransmitting();
            else if (i == 6 && capturePanel)  capturePanel->Stop();
            else if (i == 7) core.SetStretchToFill(!stretchOn);
            else if (i == 8) core.SetLiveMute(!isMuted);
            else if (i == 9 && m_UIManager) m_UIManager->RequestSettings();
        }
    }

    ImGui::PopStyleVar();
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderStatusDots — "Público" y "Stage", arriba del video. Reemplazan el
//  boton "Iniciar proyección" que vivia en ControlPanel (eliminado: su
//  configuracion — pantalla/calidad — ya estaba duplicada en Ajustes >
//  Proyección) y el "ACTIVAR STAGE" de StageDisplayPanel (ahora una
//  categoria de Ajustes). Cargar contenido (fondo/cancion/video) nunca
//  prende esto solo — el operador decide con estos dos puntos.
// ─────────────────────────────────────────────────────────────────────────────
static bool StatusDotToggle(ImDrawList* dl, const char* id, const char* label, bool on, ImVec4 onColor, float rowH)
{
    const float dotR = 5.0f;
    ImVec2 textSz = ImGui::CalcTextSize(label);
    float itemW = dotR * 2.0f + 6.0f + textSz.x + 14.0f;

    ImVec2 p0 = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(id, ImVec2(itemW, rowH));
    bool hovered = ImGui::IsItemHovered();

    ImVec2 center = { p0.x + dotR + 4.0f, p0.y + rowH * 0.5f };
    ImVec4 offColor = { 0.42f, 0.44f, 0.50f, 1.0f };
    DrawStatusDot(dl, center, dotR, on ? onColor : offColor, on);

    ImVec4 textCol = on ? onColor : ImVec4(0.75f, 0.76f, 0.80f, hovered ? 1.0f : 0.85f);
    dl->AddText({ center.x + dotR + 6.0f, p0.y + (rowH - textSz.y) * 0.5f },
               ImGui::ColorConvertFloat4ToU32(textCol), label);

    return clicked;
}

void ViewPanel::RenderStatusDots(float w)
{
    auto& core = Core::PresentationCore::Get();
    auto& sd   = ProyecThor::Settings::SettingsManager::Get().GetSettings().stageDisplay;

    const bool audienceOn = core.IsProjecting();
    const bool stageOn    = sd.useLAN ? core.IsStreamingNet() : core.IsStaging();
    const float rowH      = 28.0f;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::SetCursorPosX(10.0f);

    if (StatusDotToggle(dl, "##dotAudience", "Público", audienceOn, MT::k_LiveAccent, rowH))
        ToggleAudience(!audienceOn);

    ImGui::SameLine(0.0f, 14.0f);

    if (StatusDotToggle(dl, "##dotStage", "Stage", stageOn, MT::k_PrevAccent, rowH))
        ToggleStageQuick(!stageOn);

    // ── "Borrar Todo" ────────────────────────────────────────────────────
    // Pedido explicito: los botones especificos ("Limpiar texto/disco/
    // fondo/imagen/anuncios/reloj/captura") viven en el riel angosto a la
    // derecha del video (ver RenderQuickActions); este limpia TODO de una
    // — se pone del lado del video (columna izquierda) para que no se
    // confunda con esos botones especificos.
    {
        const char* label   = "Borrar Todo";
        ImVec2      textSz  = ImGui::CalcTextSize(label);
        const float iconSz  = rowH * 0.55f;
        const float iconGap = 8.0f;
        float       groupW  = iconSz + iconGap + textSz.x;
        float       btnW    = groupW + 24.0f;
        float       btnX    = std::max(ImGui::GetCursorPosX(), w - btnW - 10.0f);
        ImGui::SameLine(btnX);

        ImGui::PushStyleColor(ImGuiCol_Button,       ToVec4((DS::DangerColor & 0x00FFFFFFu) | (40u  << 24)));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToVec4((DS::DangerColor & 0x00FFFFFFu) | (90u  << 24)));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ToVec4((DS::DangerColor & 0x00FFFFFFu) | (140u << 24)));
        ImGui::PushStyleColor(ImGuiCol_Border,        ToVec4((DS::DangerColor & 0x00FFFFFFu) | (100u << 24)));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rowH * 0.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        bool clicked = ImGui::Button("##borrarTodo", ImVec2(btnW, rowH));

        ImVec2 bMin = ImGui::GetItemRectMin();
        ImVec2 bMax = ImGui::GetItemRectMax();
        float  startX  = bMin.x + ((bMax.x - bMin.x) - groupW) * 0.5f;
        float  centerY = (bMin.y + bMax.y) * 0.5f;
        ImU32  dangerCol = ImGui::ColorConvertFloat4ToU32(ToVec4(DS::DangerColor));

        ImDrawList* dl = ImGui::GetWindowDrawList();
        auto it = StyleGeneralApp::Icons.find("cleaning_services");
        if (it != StyleGeneralApp::Icons.end() && it->second.textureID)
        {
            dl->AddImage(it->second.textureID,
                { startX, centerY - iconSz * 0.5f }, { startX + iconSz, centerY + iconSz * 0.5f },
                ImVec2(0, 0), ImVec2(1, 1), dangerCol);
        }
        dl->AddText({ startX + iconSz + iconGap, centerY - textSz.y * 0.5f }, dangerCol, label);

        if (clicked)
        {
            core.ClearLayer2();
            core.StopBackgroundMedia();
            core.StopOverlayMedia();
            if (auto* a   = core.GetAnnouncementsRef())  a->SetLive(false);
            if (auto* clk = core.GetOClockRef())          clk->StopTransmitting();
            if (auto* cap = core.GetCapturePanelRef())    cap->Stop();
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);
    }
}

// Portado de ControlPanel::ToggleSecondaryDisplay (eliminado). Usa la
// pantalla configurada en Ajustes > Proyección, o la secundaria (indice 1)
// por defecto — nunca la principal.
//
// FIX: antes esto creaba una ventana GLFW nativa propia (CreateProjectorWindow/
// SecondaryOutputWindow, GLFW_FLOATING=true) para el proyector, que corria
// EN PARALELO con el viewport ImGui "ProjectorLive" (UIManager.cpp,
// ImGuiViewportFlags_TopMost) — las dos posicionadas exactamente sobre el
// mismo monitor, ambas pidiendo estar siempre encima. El publico terminaba
// viendo cualquiera de las dos ventanas segun quien ganara el z-order en
// cada instante, y la nativa ni siquiera dibujaba el texto en vivo. Ya se
// habia migrado el Stage a este mismo esquema (ver el comentario en
// UIManager.cpp junto a "StageLive") — ahora el Proyector sigue el mismo
// patron: SOLO existe "ProjectorLive", gateado por isProjecting/
// targetMonitorIndex, sin ventana nativa que le compita el z-order.
void ViewPanel::ToggleAudience(bool active)
{
    auto& core = Core::PresentationCore::Get();

    if (active) {
        auto& settings = ProyecThor::Settings::SettingsManager::Get().GetSettings();
        int monitorCount = 0;
        glfwGetMonitors(&monitorCount);
        int monitorIndex = std::clamp(
            settings.projection.targetMonitor < 0 ? 1 : settings.projection.targetMonitor,
            0, std::max(0, monitorCount - 1));

        core.SetTargetMonitor(monitorIndex);
        std::cout << "[ViewPanel] Proyección iniciada en monitor " << monitorIndex << ".\n";
    } else {
        std::cout << "[ViewPanel] Proyección detenida.\n";
    }

    core.SetProjecting(active);
}

// Portado de StageDisplayPanel::ToggleStageDisplay, ahora leyendo
// pantalla/LAN/puerto desde Settings (ver Settings::StageDisplaySettings)
// en vez de miembros efimeros — esta clase no tiene (ni necesita) una
// instancia de StageDisplayPanel.
void ViewPanel::ToggleStageQuick(bool active)
{
    auto& core = Core::PresentationCore::Get();
    auto& sd   = ProyecThor::Settings::SettingsManager::Get().GetSettings().stageDisplay;

    if (active) {
        if (sd.useLAN) {
            core.ToggleNetworkStream(true, sd.lanPort);
            return;
        }

        int monitorCount = 0;
        glfwGetMonitors(&monitorCount);
        if (monitorCount < 2) {
            std::cerr << "[ViewPanel] No hay suficientes monitores para activar el stage.\n";
            return;
        }

        int stageMonitorIndex = std::clamp(sd.monitorIndex < 0 ? 1 : sd.monitorIndex, 0, monitorCount - 1);
        sd.monitorIndex = stageMonitorIndex;
        ProyecThor::Settings::SettingsManager::Get().Save();
        core.SetStaging(true, stageMonitorIndex);
    } else {
        if (sd.useLAN) core.ToggleNetworkStream(false);
        else           core.SetStaging(false);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderLiveTransport — transporte + VU meters del player "general" (bg,
//  el que va a público). Portado de MonitorLiveControls::RenderLiveControls
//  (ver historial de Monitor), reflowado de una columna angosta/alta a una
//  barra ancha/baja para vivir debajo del video en vez de al costado.
// ─────────────────────────────────────────────────────────────────────────────
void ViewPanel::RenderLiveTransport(float w, float h)
{
    auto& core = Core::PresentationCore::Get();
    Core::VLCBasePlayer* bg = core.GetBackgroundPlayer();

    m_LiveMuted   = core.GetLiveMute();
    m_LiveVolume  = static_cast<float>(core.GetLiveVolume()) * 0.01f;
    m_LivePlaying = bg && !bg->IsPaused();

    int64_t liveLenMs = bg ? bg->GetLength() : 0;

    if (m_LivePlaying && bg && liveLenMs > 0)
    {
        int64_t curMs = bg->GetTime();
        float   fpos  = (liveLenMs > 0)
            ? static_cast<float>(curMs) / static_cast<float>(liveLenMs)
            : 0.0f;

        if (fpos >= 0.995f && core.GetLiveLoop())
        {
            bg->SetPosition(0.0f);
            bg->SetPause(false);
        }
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { MT::k_PadLg, MT::k_Pad });
    ImGui::PushStyleColor(ImGuiCol_ChildBg, MT::k_Bg1);
    ImGui::PushStyleColor(ImGuiCol_Border,  MT::k_BorderSubtle);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   MT::k_R);

    ImGui::BeginChild("##viewLiveTransport", { w, h }, true, ImGuiWindowFlags_NoScrollbar);

    const float innerW = w - MT::k_PadLg * 2.0f;

    // ── Cabecera PGM ──────────────────────────────────────────────────────────
    {
        ImVec2 headerPos = ImGui::GetCursorScreenPos();
        DrawStatusDot(
            ImGui::GetWindowDrawList(),
            { headerPos.x + 7.0f, headerPos.y + 9.0f },
            4.5f, MT::k_LiveAccent, m_LivePlaying);

        ImGui::SetCursorPosX(MT::k_PadLg + 20.0f);
        ImGui::PushStyleColor(ImGuiCol_Text,
            m_LivePlaying ? MT::k_LiveAccent : MT::k_TextSecondary);
        ImGui::TextUnformatted(m_LivePlaying ? "PROGRAM  —  ON AIR" : "PROGRAM");
        ImGui::PopStyleColor();
    }

    DrawAccentLine(innerW, MT::k_LiveAccentDim, 1.0f);
    ImGui::Spacing();

    // ── Barra de progreso ─────────────────────────────────────────────────────
    int64_t liveCurMs = bg ? bg->GetTime()   : 0;
    int64_t liveLen   = bg ? bg->GetLength() : 0;
    float   livePos   = (liveLen > 0)
        ? std::clamp(static_cast<float>(liveCurMs) / static_cast<float>(liveLen), 0.0f, 1.0f)
        : 0.0f;

    ImGui::SetCursorPosX(MT::k_PadLg);
    float displayPos = livePos;
    if (BMSlider("##vp_tl_live", &displayPos, 0.0f, 1.0f, "",
                 MT::k_LiveTrack, MT::k_LiveGrab,
                 { MT::k_LiveGrab.x * 1.1f, MT::k_LiveGrab.y * 1.1f, MT::k_LiveGrab.z * 1.1f, 1.0f },
                 innerW))
    {
        core.SetLivePosition(displayPos);
        liveCurMs = static_cast<int64_t>(displayPos * static_cast<float>(liveLen));
    }

    DrawTimeRow(innerW, MT::k_PadLg, liveCurMs, liveLen);
    ImGui::Spacing();

    // ── Fila unica: transporte + volumen (todo inline, hay ancho de sobra) ────
    const float gap      = MT::k_Gap;
    const float btnH     = MT::k_TransportH;
    const float navBtnW  = 34.0f;
    const float iconSize = 15.0f;

    ImGui::SetCursorPosX(MT::k_PadLg);

    ImGui::PushID("vp_btn_replay");
    if (DrawIconButton("replay_10", iconSize, MT::k_NeutBtn, MT::k_NeutBtnHov, MT::k_NeutBtnAct, {navBtnW, btnH})) {
        float np = livePos - (liveLen > 0 ? 10000.0f / static_cast<float>(liveLen) : 0.0f);
        core.SetLivePosition(std::max(0.0f, np));
    }
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    const char* mainIcon = m_LivePlaying ? "pause" : "play";
    ImGui::PushID("vp_btn_main_transport");
    if (DrawIconButton(mainIcon, 20.0f, MT::k_LiveBtn, MT::k_LiveBtnHov, MT::k_LiveBtnAct, {navBtnW * 1.6f, btnH}, m_LivePlaying)) {
        if (bg) {
            if (m_LivePlaying) {
                bg->SetPause(true);
            } else {
                core.SetLiveMute(m_LiveMuted);
                core.SetLiveVolume(m_LiveMuted ? 0 : static_cast<int>(m_LiveVolume * 100.0f));
                bg->SetPause(false);
            }
        }
    }
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    ImGui::PushID("vp_btn_fwd");
    if (DrawIconButton("forward_10", iconSize, MT::k_NeutBtn, MT::k_NeutBtnHov, MT::k_NeutBtnAct, {navBtnW, btnH})) {
        float np = livePos + (liveLen > 0 ? 10000.0f / static_cast<float>(liveLen) : 0.0f);
        core.SetLivePosition(std::min(1.0f, np));
    }
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    ImGui::PushID("vp_btn_stop");
    if (DrawIconButton("stop", iconSize, MT::k_NeutBtn, MT::k_NeutBtnHov, MT::k_NeutBtnAct, {navBtnW, btnH})) {
        core.SetLivePosition(0.0f);
        if (bg) { bg->SetPosition(0.0f); bg->SetPause(true); }
    }
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap * 2.0f);

    bool isDanger = (m_LiveVolume > 1.0f);
    ImVec4 volBtnBg = isDanger ? ImVec4(0.36f, 0.08f, 0.08f, 1.0f) : MT::k_NeutBtn;
    const char* volIcon = m_LiveMuted ? "no_sound" : "volume_up";

    ImGui::PushID("vp_btn_mute");
    if (DrawIconButton(volIcon, iconSize, volBtnBg, MT::k_NeutBtnHov, MT::k_NeutBtnAct, {navBtnW, btnH}, m_LiveMuted)) {
        m_LiveMuted = !m_LiveMuted;
        core.SetLiveMute(m_LiveMuted);
        core.SetLiveVolume(m_LiveMuted ? 0 : static_cast<int>(m_LiveVolume * 100.0f));
    }
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    const float volSliderW = 130.0f;
    ImVec4 slBg   = isDanger ? ImVec4(0.36f, 0.08f, 0.08f, 1.0f) : MT::k_NeutBtn;
    ImVec4 slGrab = isDanger ? ImVec4(0.92f, 0.20f, 0.20f, 1.0f) : MT::k_LiveGrab;
    ImVec4 slAct  = isDanger ? ImVec4(1.00f, 0.30f, 0.30f, 1.0f) : ImVec4(MT::k_LiveGrab.x * 1.1f, MT::k_LiveGrab.y * 1.1f, MT::k_LiveGrab.z * 1.1f, 1.0f);

    if (BMSlider("##vp_vol_l", &m_LiveVolume, 0.0f, 2.0f, "", slBg, slGrab, slAct, volSliderW)) {
        core.SetLiveVolume(m_LiveMuted ? 0 : static_cast<int>(m_LiveVolume * 100.0f));
    }

    // Los medidores VU ya no van aca abajo (le comian ~48px fijos a este
    // panel, siempre a lo ancho completo): ahora se dibujan chicos, pegados
    // al borde izquierdo del video en RenderContent, mas comodos y sin
    // robarle alto al transporte. m_AudioMeters.Update() se llama desde ahi.

    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

// "Control Overlays" se movio a ViewToolsPanel (nuevo panel debajo de
// "Vista en Vivo", junto con Red/Notas/Reloj) — ver ViewToolsPanel.cpp.

void ViewPanel::RenderContent(float panelW, float panelH)
{
    auto& core  = ProyecThor::Core::PresentationCore::Get();
    auto  state = core.GetState();

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // ── 1. Resolución de referencia del proyector ────────────────────────
    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

    float srcW = 1920.0f;
    float srcH = 1080.0f;

    if (monitors && monitorCount > 0 && state.targetMonitorIndex >= 0 &&
        state.targetMonitorIndex < monitorCount)
    {
        const GLFWvidmode* mode = glfwGetVideoMode(monitors[state.targetMonitorIndex]);
        if (mode && mode->width > 0 && mode->height > 0)
        {
            srcW = (float)mode->width;
            srcH = (float)mode->height;
        }
    }

    // ── 2. Calcular "Lo justo y necesario" ────────────────────────────────
    float srcRatio = srcW / srcH;
    float drawW = panelW;
    float drawH = panelW / srcRatio;

    // Si el alto calculado supera el alto disponible, ajustamos en base al alto
    if (drawH > panelH)
    {
        drawH = panelH;
        drawW = panelH * srcRatio;
    }

    // Centrar horizontal y verticalmente desplazando el cursor interno de ImGui
    float offsetX = (panelW - drawW) * 0.5f;
    float offsetY = (panelH - drawH) * 0.5f;

    if (offsetX > 0.0f) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
    }
    if (offsetY > 0.0f) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offsetY);
    }

    // Puntos exactos del área de dibujo
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1 = ImVec2(p0.x + drawW, p0.y + drawH);

    // ── 3. Fondo de video / Estado Inactivo ───────────────────────────────
    if (!state.isProjecting)
    {
        dl->AddRectFilled(p0, p1, IM_COL32(8, 9, 16, 255));

        const char* msg     = "Sin proyeccion activa";
        ImVec2      msgSize = ImGui::CalcTextSize(msg);
        dl->AddText(
            ImVec2(p0.x + (drawW - msgSize.x) * 0.5f,
                   p0.y + (drawH - msgSize.y) * 0.5f),
            IM_COL32(60, 65, 90, 255),
            msg);

        dl->AddRect(p0, p1, IM_COL32(40, 44, 64, 255), 0.0f, 0, 1.0f);

        // REGISTRAMOS SOLO EL ESPACIO QUE USAMOS
        ImGui::Dummy(ImVec2(drawW, drawH));
        return;
    }

    // Si está proyectando
    if (state.bgType == Core::PresentationState::BackgroundType::Video)
    {
        void* texID = core.GetProcessedBackgroundTexture((int)drawW, (int)drawH);
        dl->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, 255));
        if (texID)
        {
            // Mismo ajuste de aspecto que el output real (ver
            // BackgroundLayer::Render): antes esto solo estiraba la textura
            // a TODO el rect del panel, que respeta el aspecto del MONITOR
            // pero no el del video en si. Si el video no tenia el mismo AR
            // que el monitor (y stretch-to-fill estaba apagado), el
            // publico veia letterbox/pillarbox y el preview no — no
            // coincidian.
            ImVec2 vp0 = p0, vp1 = p1;
            int vw = 0, vh = 0;
            Core::VLCBasePlayer* bgPlayer = core.GetBackgroundPlayer();
            if (bgPlayer) bgPlayer->GetVideoSize(vw, vh);

            if (vw > 0 && vh > 0 && !core.GetStretchToFill())
            {
                float videoRatio  = (float)vw / (float)vh;
                float screenRatio = drawW / drawH;

                if (videoRatio > screenRatio + 0.001f)
                {
                    float fitH = drawW / videoRatio;
                    float offY = (drawH - fitH) * 0.5f;
                    vp0 = { p0.x, p0.y + offY };
                    vp1 = { p1.x, p0.y + offY + fitH };
                }
                else if (videoRatio < screenRatio - 0.001f)
                {
                    float fitW = drawH * videoRatio;
                    float offX = (drawW - fitW) * 0.5f;
                    vp0 = { p0.x + offX, p0.y };
                    vp1 = { p0.x + offX + fitW, p1.y };
                }
            }

            dl->AddImage(texID, vp0, vp1, ImVec2(0, 0), ImVec2(1, 1));
        }
    }
    else {
         // Fondo base si proyecta algo que no es video (como imágenes o color sólido)
         dl->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, 255));
    }

    // Overlay (logos, videos de overlay, etc.) — el output real siempre lo
    // dibuja encima del fondo (ver PresentationCore::RenderProjectorWindow:
    // background.Render() + overlay.Render()), pero el preview nunca lo
    // mostraba: cualquier cosa activa en la pestaña Overlays estaba al aire
    // pero invisible aca.
    if (core.IsOverlayActive())
    {
        if (void* overlayTex = core.GetOverlayTexture())
            dl->AddImage(overlayTex, p0, p1, ImVec2(0, 0), ImVec2(1, 1));
    }

    // ── 4. Texto proyectado ───────────────────────────────────────────────
    if (state.showText && !state.currentText.empty())
    {
        // FIX: los margenes/tamano de texto estan definidos en unidades de
        // referencia sobre un lienzo de 1920px (ver DrawTextBlock en
        // UIManager.cpp, que es lo que realmente se dibuja en la pantalla
        // al publico: usa screenScale = anchoRealDelMonitor / 1920). Este
        // preview usaba drawW/srcW (ancho del panel / ancho real del
        // monitor), que NO es lo mismo salvo que el monitor real mida
        // exactamente 1920px de ancho: con cualquier otra resolucion
        // (1366, 2560, 3840...) los margenes quedaban mal escalados y el
        // texto se recortaba en el preview de forma distinta a como se ve
        // realmente en la pantalla. Como drawW ya representa el ancho
        // COMPLETO del monitor real dentro del panel, la conversion
        // correcta de "unidades de 1920" a "pixeles de preview" es
        // simplemente drawW/1920, sin pasar por el ancho real del monitor.
        float scale = drawW / 1920.0f;

        float marginL = state.margins[0] * scale;
        float marginT = state.margins[1] * scale;
        float marginR = state.margins[2] * scale;
        float marginB = state.margins[3] * scale;

        float boxW = std::max(10.0f, drawW - marginL - marginR);
        float boxH = std::max(10.0f, drawH - marginT - marginB);
        
        // Usamos p0.x y p0.y en lugar del drawX/drawY antiguo
        float boxX = p0.x + marginL;
        float boxY = p0.y + marginT;

        float fontSize = state.textSize * scale;

        std::string fontName = core.GetActiveFontName();
        ImFont* font = core.GetImGuiFont(fontName, fontSize);
        if (!font) font = ImGui::GetFont();

        if (state.autoScale)
        {
            while (fontSize > 4.0f)
            {
                ImVec2 ts = font->CalcTextSizeA(
                    fontSize, FLT_MAX, boxW, state.currentText.c_str());
                if (ts.y <= boxH) break;
                fontSize -= 1.0f;
            }
        }

        ImVec2 textBlock = font->CalcTextSizeA(
            fontSize, FLT_MAX, boxW, state.currentText.c_str());

        float textX = boxX;
        if (state.textAlignment == 1)
            textX += (boxW - textBlock.x) * 0.5f;
        else if (state.textAlignment == 2)
            textX += (boxW - textBlock.x);

        float textY = boxY;
        if (state.vAlignment == 1)
            textY += (boxH - textBlock.y) * 0.5f;
        else if (state.vAlignment == 2)
            textY += (boxH - textBlock.y);

        dl->PushClipRect(p0, p1, true);

        ImU32 shadowCol = IM_COL32(0, 0, 0, 180);
        ImU32 textCol   = ImGui::ColorConvertFloat4ToU32(
            ImVec4(state.textColor[0], state.textColor[1],
                   state.textColor[2], state.textColor[3]));

        bool isSong = (core.PeekSelection().type == Core::ItemType::Song);
        if (isSong && state.textAlignment == 1)
        {
            float lineH = font->CalcTextSizeA(fontSize, FLT_MAX, boxW, "A").y;

            float startY = boxY;
            if (state.vAlignment == 1)
                startY += (boxH - textBlock.y) * 0.5f;
            else if (state.vAlignment == 2)
                startY += (boxH - textBlock.y);

            float  curY     = startY;
            size_t startPos = 0;
            size_t endPos   = state.currentText.find('\n');

            while (startPos != std::string::npos)
            {
                std::string line =
                    state.currentText.substr(startPos, endPos - startPos);
                if (!line.empty() && line.back() == '\r') line.pop_back();

                if (!line.empty())
                {
                    ImVec2 lSize =
                        font->CalcTextSizeA(fontSize, FLT_MAX, boxW, line.c_str());
                    float lx = boxX + (boxW - lSize.x) * 0.5f;

                    dl->AddText(font, fontSize,
                        ImVec2(lx + 2.0f * scale, curY + 2.0f * scale),
                        shadowCol, line.c_str());
                    dl->AddText(font, fontSize,
                        ImVec2(lx, curY), textCol, line.c_str());
                }

                curY += lineH;
                if (endPos == std::string::npos) break;
                startPos = endPos + 1;
                endPos   = state.currentText.find('\n', startPos);
            }
        }
        else
        {
            dl->AddText(font, fontSize,
                ImVec2(textX + 2.0f * scale, textY + 2.0f * scale),
                shadowCol, state.currentText.c_str(), nullptr, boxW);
            dl->AddText(font, fontSize,
                ImVec2(textX, textY), textCol,
                state.currentText.c_str(), nullptr, boxW);
        }

        dl->PopClipRect();
    }

    // ── 5. Borde y UI adicional ───────────────────────────────────────────
    dl->AddRect(p0, p1, IM_COL32(50, 55, 80, 180), 0.0f, 0, 1.0f);

    // ── 5b. Medidor VU chico, pegado al borde izquierdo del video ─────────
    // Antes vivia en RenderLiveTransport como una barra horizontal fija de
    // 48px de alto x todo el ancho, debajo del video. Se movio aca, chico y
    // vertical, para no robarle alto al transporte y quedar "encima" del
    // visor como en un mixer, sin estorbar.
    if (state.isProjecting)
    {
        Core::VLCBasePlayer* liveBg = core.GetBackgroundPlayer();
        bool liveMuted   = core.GetLiveMute();
        float liveVolume = static_cast<float>(core.GetLiveVolume()) * 0.01f;
        bool livePlaying = liveBg && !liveBg->IsPaused();

        m_AudioMeters.Update(liveBg, true, livePlaying, liveMuted, liveVolume);

        const float meterW = 22.0f;
        const float meterPad = 6.0f;
        float meterH = std::min(drawH - meterPad * 2.0f, 110.0f);
        if (meterH > 20.0f)
        {
            ImVec2 meterPos = { p0.x + meterPad, p0.y + meterPad };
            m_AudioMeters.RenderVertical(dl, meterPos, meterW, meterH);
        }
    }

    // ── 5c. Indicador de carga (solo operador) ─────────────────────────────
    // Chico, esquina inferior derecha del preview: NUNCA se muestra en la
    // salida real al publico (ver PresentationCore::ShouldShowLoadingScreen
    // para el equivalente que si se ve el publico, con el logo configurado
    // en Ajustes > Proyeccion). Este es solo feedback para el operador de
    // que un fondo/video esta cargando en standby.
    if (core.IsBackgroundSwapPending())
    {
        const float spinR = 11.0f;
        ImVec2 spinCenter = { p1.x - spinR - 14.0f, p1.y - spinR - 14.0f };
        DrawLoadingSpinner(dl, spinCenter, spinR);

        char etaBuf[32];
        snprintf(etaBuf, sizeof(etaBuf), "~%.1fs", core.GetBackgroundSwapEta());
        ImVec2 etaSz = ImGui::CalcTextSize(etaBuf);
        ImVec2 etaPos = { spinCenter.x - spinR - 6.0f - etaSz.x, spinCenter.y - etaSz.y * 0.5f };
        dl->AddRectFilled({ etaPos.x - 5.0f, etaPos.y - 3.0f }, { etaPos.x + etaSz.x + 5.0f, etaPos.y + etaSz.y + 3.0f },
                          IM_COL32(0, 0, 0, 150), 4.0f);
        dl->AddText(etaPos, IM_COL32(230, 230, 235, 230), etaBuf);
    }

    // ── 6. Indicador de Red (Solo cuando transmite) ───────────────────────
    if (state.isProjecting && state.isStreamingNet)
    {
        // Puedes cambiar "WIFI" por un icono de FontAwesome si tu proyecto lo soporta (ej. u8"\uf1eb")
        const char* wifiStr = "online"; 
        ImVec2 wifiSize = ImGui::CalcTextSize(wifiStr);
        
        // Posicionado en la esquina superior derecha del área de proyección
        ImVec2 wifiPos = ImVec2(p1.x - wifiSize.x - 12.0f, p0.y + 8.0f);

        // Fondo oscuro semitransparente para que contraste con cualquier video/imagen de fondo
        dl->AddRectFilled(
            ImVec2(wifiPos.x - 6.0f, wifiPos.y - 4.0f),
            ImVec2(wifiPos.x + wifiSize.x + 6.0f, wifiPos.y + wifiSize.y + 4.0f),
            IM_COL32(0, 0, 0, 160), 4.0f);

        // Dibujar el icono
        dl->AddText(wifiPos, IM_COL32(0, 255, 100, 255), wifiStr);
    }

    // Registramos que solo consumimos el tamaño de la pantalla
    ImGui::Dummy(ImVec2(drawW, drawH));
}

} // namespace ProyecThor::UI