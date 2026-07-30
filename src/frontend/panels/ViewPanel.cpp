#include "ViewPanel.h"
#include "UIManager.h"
#include "DesignSystem.h"
#include "frontend/ui/bin/StyleGeneralApp.h"
#include "backend/core/PresentationCore.h"
#include "backend/settings/SettingsManager.h"
#include "backend/settings/ProjectionQualityPresets.h"
#include "frontend/views/Announcements.h"
#include "frontend/views/OClock.h"
#include "capture/CapturePanel.h"
#include "TeamChatPanel.h"
#include "MonitorTheme.h"
#include "MonitorDesign.h"
#include "MonitorUIHelpers.h"
#include "frontend/ui/LoadingSpinner.h"
#include "frontend/ui/AppIcons.h"
#include "frontend/panels/home/HomeIcons.h"
#include "frontend/ui/IconRail.h"
#include "frontend/ui/LiveContentRenderer.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <iostream>
#include <GLFW/glfw3.h>

namespace ProyecThor::UI {

static constexpr float kQuickActionsRailW = 40.0f;
// Franja horizontal de config (RenderQuickActionsConfig), abajo de todo el
// panel -- antes era un segundo riel vertical a la izquierda, movido para
// devolverle ese ancho al video (ver ViewPanel::Render).
static constexpr float kConfigStripH = 36.0f;
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

// ─────────────────────────────────────────────────────────────────────────────
//  DrawPadButton — pad cuadrado tipo controlador MIDI (Launchpad): color fijo
//  por accion en vez del esquema neutro/rojo de DrawIconButton, con un halo
//  de brillo cuando esta "encendido" (ej. Play mientras esta en vivo) para
//  que se sienta como un boton fisico iluminado en vez de un icono chico
//  sobre un rectangulo plano.
// ─────────────────────────────────────────────────────────────────────────────
bool DrawPadButton(const char* iconName, float iconSize, ImVec4 padColor, ImVec2 btnSize, bool lit,
                    DrawIconFn vectorIcon = nullptr)
{
    ImVec4 offCol  = ImVec4(padColor.x * 0.30f, padColor.y * 0.30f, padColor.z * 0.30f, 1.0f);
    ImVec4 baseCol = lit ? padColor : offCol;
    ImVec4 hovCol  = Brighten(baseCol, 0.12f);
    ImVec4 actCol  = Brighten(padColor, -0.10f);

    ImGui::PushStyleColor(ImGuiCol_Button,        baseCol);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  hovCol);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,   actCol);
    ImGui::PushStyleColor(ImGuiCol_Border,         ImVec4(1.0f, 1.0f, 1.0f, lit ? 0.40f : 0.10f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,   10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.3f);

    ImVec2 p0      = ImGui::GetCursorScreenPos();
    bool   pressed = ImGui::Button("", btnSize);
    bool   isHeld  = ImGui::IsItemActive();
    ImVec2 p1      = { p0.x + btnSize.x, p0.y + btnSize.y };

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Halo suave detras del icono cuando el pad esta prendido -- sensacion
    // de luz interna en vez de un simple resaltado de hover.
    if (lit)
    {
        ImVec2 center = { (p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f };
        dl->AddCircleFilled(center, btnSize.y * 0.55f,
            ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.10f)), 24);
    }

    auto it = StyleGeneralApp::Icons.find(iconName);
    bool hasTexture = (it != StyleGeneralApp::Icons.end() && it->second.textureID != nullptr);

    float  offsetY = isHeld ? 2.0f : 0.0f;
    ImVec2 center  = { (p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f + offsetY };

    // Guarda de textura: antes se le pasaba a AddImage un ImTextureID nulo
    // cuando la textura no estaba cargada, y algunos backends lo dibujan
    // como un icono de basura en vez de nada (ver captura del usuario en el
    // pad de mute). Con vectorIcon como respaldo -- mismo criterio que
    // QuickActionButton -- y si tampoco hay uno, se deja el pad solo con su
    // color, sin icono, que es preferible a mostrar basura.
    if (hasTexture)
    {
        dl->AddImage((ImTextureID)(intptr_t)it->second.textureID,
            { center.x - iconSize * 0.5f, center.y - iconSize * 0.5f },
            { center.x + iconSize * 0.5f, center.y + iconSize * 0.5f },
            ImVec2(0, 0), ImVec2(1, 1),
            IM_COL32(255, 255, 255, 255));
    }
    else if (vectorIcon)
    {
        ImVec2 origin = { center.x - iconSize * 0.5f, center.y - iconSize * 0.5f };
        vectorIcon(dl, origin, iconSize, IM_COL32(255, 255, 255, 255));
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    return pressed;
}

// ─────────────────────────────────────────────────────────────────────────────
//  HorizontalFader — deslizante estilo canal de mesa de sonido: surco angosto
//  con marcas de escala + un "cap" vertical que se arrastra, en vez de un
//  slider generico. Horizontal (no vertical): en este panel el ancho sobra
//  pero el alto es escaso (fila baja y ancha debajo del video), asi que una
//  columna vertical no entraba sin recortarse -- ver captura del usuario.
//  Click/arrastre mapea directo la posicion X del mouse al valor (mismo
//  criterio inmediato que DS::ModernSlider). Vive aca y no en
//  MonitorUIHelpers porque por ahora solo lo pide este panel.
// ─────────────────────────────────────────────────────────────────────────────
bool HorizontalFader(const char* id, float* value, float lo, float hi, ImVec2 size,
                      ImU32 trackCol, ImU32 fillCol, ImU32 capCol)
{
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, size);
    bool hovered = ImGui::IsItemHovered();
    bool active  = ImGui::IsItemActive();
    bool changed = false;

    const float capW       = 14.0f;
    const float trackLeft  = pos.x + capW * 0.5f;
    const float trackRight = pos.x + size.x - capW * 0.5f;
    const float trackWpx   = std::max(1.0f, trackRight - trackLeft);

    if (active && ImGui::IsMouseDown(ImGuiMouseButton_Left) && hi > lo)
    {
        float t = std::clamp((ImGui::GetIO().MousePos.x - trackLeft) / trackWpx, 0.0f, 1.0f);
        float newVal = lo + t * (hi - lo);
        if (newVal != *value) { *value = newVal; changed = true; }
    }

    float frac = (hi > lo) ? std::clamp((*value - lo) / (hi - lo), 0.0f, 1.0f) : 0.0f;
    float capX = trackLeft + frac * trackWpx;

    ImDrawList* dl      = ImGui::GetWindowDrawList();
    const float trackH  = 6.0f;
    float       cy      = pos.y + size.y * 0.5f;

    dl->AddRectFilled({ trackLeft, cy - trackH * 0.5f }, { trackRight, cy + trackH * 0.5f },
                       trackCol, trackH * 0.5f);

    if (capX - trackLeft > 0.5f)
        dl->AddRectFilled({ trackLeft, cy - trackH * 0.5f }, { capX, cy + trackH * 0.5f },
                           fillCol, trackH * 0.5f);

    // Marcas de escala, como en una consola real.
    for (int i = 0; i <= 4; i++)
    {
        float mx = trackLeft + trackWpx * (float)i / 4.0f;
        dl->AddLine({ mx, cy - size.y * 0.30f }, { mx, cy - trackH * 0.7f },
                     IM_COL32(255, 255, 255, 35), 1.0f);
    }

    float  capHalfH = size.y * 0.40f;
    ImVec2 capMin   = { capX - capW * 0.5f, cy - capHalfH };
    ImVec2 capMax   = { capX + capW * 0.5f, cy + capHalfH };
    ImU32  capBody  = (hovered || active)
        ? ImGui::GetColorU32(Brighten(ToVec4(capCol), 0.10f))
        : capCol;

    dl->AddRectFilled(capMin, capMax, capBody, 3.0f);
    dl->AddRect(capMin, capMax, IM_COL32(0, 0, 0, 110), 3.0f, 0, 1.2f);
    dl->AddLine({ capX, capMin.y + 4.0f }, { capX, capMax.y - 4.0f }, IM_COL32(0, 0, 0, 130), 1.5f);

    return changed;
}

// Altavoz — para el pad de Mute del transporte. No hay textura
// "volume_up"/"no_sound" cargada en StyleGeneralApp::Icons (el pad
// terminaba pasandole un ImTextureID nulo a AddImage, que en este backend
// se ve como basura -- ver captura del usuario). Caja + cono triangular a
// mano, mismo criterio que DrawIcon_Disc/DrawIcon_Gear; una linea diagonal
// en vez de las ondas de sonido cuando esta muteado.
void DrawSpeakerShape(ImDrawList* dl, ImVec2 o, float sz, ImU32 col, bool muted)
{
    ImVec2 c = { o.x + sz * 0.5f, o.y + sz * 0.5f };

    float  boxHalfH = sz * 0.16f;
    ImVec2 boxMin   = { c.x - sz * 0.42f, c.y - boxHalfH };
    ImVec2 boxMax   = { c.x - sz * 0.16f, c.y + boxHalfH };
    dl->AddRectFilled(boxMin, boxMax, col, 1.0f);

    ImVec2 apex    = { boxMax.x, c.y };
    ImVec2 baseTop = { c.x + sz * 0.16f, c.y - sz * 0.34f };
    ImVec2 baseBot = { c.x + sz * 0.16f, c.y + sz * 0.34f };
    dl->AddTriangleFilled(apex, baseTop, baseBot, col);

    if (muted)
    {
        dl->AddLine({ o.x + sz * 0.06f, o.y + sz * 0.94f },
                    { o.x + sz * 0.94f, o.y + sz * 0.06f }, col, sz * 0.09f);
    }
    else
    {
        for (int i = 1; i <= 2; i++)
        {
            float r = sz * (0.14f + 0.13f * (float)i);
            dl->PathArcTo({ c.x + sz * 0.10f, c.y }, r, -0.62f, 0.62f, 10);
            dl->PathStroke(col, 0, sz * 0.055f);
        }
    }
}
void DrawIcon_SpeakerOn(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)    { DrawSpeakerShape(dl, o, sz, col, false); }
void DrawIcon_SpeakerMuted(ImDrawList* dl, ImVec2 o, float sz, ImU32 col) { DrawSpeakerShape(dl, o, sz, col, true);  }

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

// Rayo — para el acceso rapido a "Calidad de salida" (Ajustes > Proyeccion).
// Mismo motivo que DrawIcon_Disc/DrawIcon_Gear: no hay textura cargada para
// esto y el glifo de fallback no es un icono real.
void DrawIcon_Bolt(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float  thick = sz * 0.14f;
    ImVec2 p0 = { o.x + sz * 0.58f, o.y + sz * 0.06f };
    ImVec2 p1 = { o.x + sz * 0.28f, o.y + sz * 0.54f };
    ImVec2 p2 = { o.x + sz * 0.50f, o.y + sz * 0.54f };
    ImVec2 p3 = { o.x + sz * 0.34f, o.y + sz * 0.94f };
    dl->AddLine(p0, p1, col, thick);
    dl->AddLine(p1, p2, col, thick);
    dl->AddLine(p2, p3, col, thick);
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

// ─────────────────────────────────────────────────────────────────────────────
//  Pads (ver RenderPadsPopup) — movido tal cual desde ViewToolsPanel (mismo
//  comportamiento). Tabla de iconos elegibles: reusa dibujos vectoriales ya
//  existentes (AppIcons/HomeIcons, misma firma en los dos headers), no hace
//  falta agregar assets nuevos. PadSettings::iconIndex es la posicion en
//  esta tabla (no un nombre), asi que el orden importa para la persistencia.
// ─────────────────────────────────────────────────────────────────────────────
using PadIconDrawFn = void(*)(ImDrawList*, ImVec2, float, ImU32);
struct PadIconEntry { const char* name; PadIconDrawFn draw; };

static const PadIconEntry kPadIcons[] = {
    { "Mixer",      AppIcons::DrawIcon_Mixer     },
    { "Monitor",    AppIcons::DrawIcon_Monitor   },
    { "Capas",      AppIcons::DrawIcon_Layers    },
    { "Paleta",     AppIcons::DrawIcon_Palette   },
    { "Overlay",    AppIcons::DrawIcon_Overlay   },
    { "Tipografia", AppIcons::DrawIcon_TextAa    },
    { "Transicion", AppIcons::DrawIcon_Swap      },
    { "Shader",     AppIcons::DrawIcon_Shader    },
    { "Home",       HomeIcons::DrawIcon_Home     },
    { "Reloj",      HomeIcons::DrawIcon_Clock    },
    { "Anuncios",   HomeIcons::DrawIcon_Megaphone},
    { "Notas",      HomeIcons::DrawIcon_Notepad  },
    { "Camara",     HomeIcons::DrawIcon_Camera   },
    { "Red",        HomeIcons::DrawIcon_Broadcast},
    { "Chat",       HomeIcons::DrawIcon_Chat     },
};
static constexpr int kPadIconCount = (int)(sizeof(kPadIcons) / sizeof(kPadIcons[0]));

const PadIconEntry& PadIconFor(int index)
{
    return kPadIcons[std::clamp(index, 0, kPadIconCount - 1)];
}

// Grilla de seleccion de icono, usada dentro del submenu "Elegir icono" del
// menu contextual de cada pad. Devuelve true si el usuario eligio uno nuevo.
bool RenderPadIconGrid(int& iconIndex)
{
    bool changed = false;
    const int   cols    = 5;
    const float cellSz  = 34.0f;
    const float spacing = 6.0f;

    for (int i = 0; i < kPadIconCount; i++)
    {
        if (i % cols != 0) ImGui::SameLine(0.0f, spacing);

        const bool sel = (i == iconIndex);
        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_Button, sel ? MT::k_PrevBtn : ImVec4(1,1,1,0.04f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, MT::k_PrevBtnHov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  MT::k_PrevBtnAct);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

        bool clicked = ImGui::Button("##padIcon", ImVec2(cellSz, cellSz));
        ImVec2 p = ImGui::GetItemRectMin();
        ImVec2 s = ImGui::GetItemRectSize();
        float  iconSz = cellSz * 0.55f;
        kPadIcons[i].draw(ImGui::GetWindowDrawList(),
                          { p.x + (s.x - iconSz) * 0.5f, p.y + (s.y - iconSz) * 0.5f }, iconSz, ImGui::GetColorU32(ImVec4(1,1,1,0.92f)));

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("%s", kPadIcons[i].name);

        if (clicked) { iconIndex = i; changed = true; }
        ImGui::PopID();
    }
    return changed;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Guardar/aplicar un Pad — junta las dos partes independientes (Captura,
//  Estilo+Fondo) via las APIs ya existentes de cada subsistema. Nunca toca
//  la letra/texto en pantalla (PresentationState::currentText) a proposito
//  -- eso es lo unico que un Pad no guarda.
// ─────────────────────────────────────────────────────────────────────────────
using PadSettings = ProyecThor::Settings::PadSettings;

void SavePad(PadSettings& pad)
{
    auto& core = Core::PresentationCore::Get();

    if (auto* cap = core.GetCapturePanelRef())
        pad.hasCapture = cap->SnapshotCurrentCapture(pad.capture);
    else
        pad.hasCapture = false;

    auto state = core.GetState();
    pad.hasStyle       = true;
    pad.styleSize      = state.textSize;
    for (int c = 0; c < 4; c++) pad.styleColor[c] = state.textColor[c];
    pad.styleHAlign    = state.textAlignment;
    pad.styleVAlign    = state.vAlignment;
    for (int c = 0; c < 4; c++) pad.styleMargins[c] = state.margins[c];
    pad.styleAutoScale = state.autoScale;
    pad.styleFontName  = state.selectedFont;
    pad.bgType = (int)state.bgType;
    pad.bgPath = state.bgPath;
    for (int c = 0; c < 3; c++) pad.bgColor[c] = state.bgColor[c];

    pad.assigned = pad.hasCapture || pad.hasStyle;
    ProyecThor::Settings::SettingsManager::Get().Save();
}

void ApplyPad(const PadSettings& pad)
{
    auto& core = Core::PresentationCore::Get();
    using BgType = Core::PresentationState::BackgroundType;

    if (pad.hasCapture) {
        if (auto* cap = core.GetCapturePanelRef())
            cap->ApplyCaptureScene(pad.capture);
    }

    if (pad.hasStyle) {
        Core::SavedStyle snap;
        snap.size      = pad.styleSize;
        for (int c = 0; c < 4; c++) snap.color[c] = pad.styleColor[c];
        snap.hAlign    = pad.styleHAlign;
        snap.vAlign    = pad.styleVAlign;
        for (int c = 0; c < 4; c++) snap.margins[c] = pad.styleMargins[c];
        snap.autoScale = pad.styleAutoScale;
        snap.fontName  = pad.styleFontName;
        core.ApplyStyleSnapshot(snap);

        switch ((BgType)pad.bgType) {
            case BgType::SolidColor:
                core.SetLayer0_Color(pad.bgColor[0], pad.bgColor[1], pad.bgColor[2]);
                break;
            case BgType::Video:
                core.SetBackgroundMedia(pad.bgPath, true, false);
                break;
            case BgType::Audio:
                core.SetBackgroundAudio();
                break;
        }
    }
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
        // Riel/franja opcional desde Vista > "Botones de limpieza (Vista en
        // Vivo)" — apagado, el video se queda con todo el espacio. Solo el
        // riel de "Limpiar <tipo>" es vertical (a la derecha, junto al
        // video); config es una franja horizontal abajo de TODO el panel,
        // asi no le resta ancho al video por los dos costados.
        const bool  showQuickActions = ProyecThor::Settings::SettingsManager::Get()
                                            .GetSettings().general.showViewQuickActions;
        const float railW      = showQuickActions ? kQuickActionsRailW : 0.0f;
        const float stripH     = showQuickActions ? kConfigStripH : 0.0f;
        const float topAreaH   = std::max(0.0f, avail.y - stripH);
        const float contentW   = std::max(0.0f, avail.x - railW);

        // Children con padding cero — el estilo global usa WindowPadding
        // (22,18), que aquí sólo recortaría el video y el riel angosto.
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        if (contentW > 8.0f && topAreaH > 8.0f)
        {
            ImGui::BeginChild("##viewVideoArea", ImVec2(contentW, topAreaH), false,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

            // Relacion de aspecto real de la salida (la del monitor
            // configurado en Ajustes > Proyeccion) — se calcula antes de
            // todo porque tanto el video principal como la tira de Stage
            // (mas abajo) letterboxean contra la MISMA proporcion, sea 16:9
            // o cualquier otra resolucion "rara" que use el operador.
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

            // Puntos "Publico"/"Stage" + "Borrar Todo" -- se mudaron a la
            // toolbar superior (ver UIManager::RenderModeToolbarStatusActions),
            // pedido explicito para liberarle este espacio a "Vista en Vivo".
            const float dotsH = 0.0f;

            // Tira de preview de Stage, opcional (ver "vaStageStrip" en
            // RenderQuickActionsConfig) — a diferencia de m_PreviewSource
            // (que reemplaza que se ve en el video principal), esto se ve EN
            // SIMULTANEO con Público, arriba del video. Letterboxeada a
            // srcAspect (no estirada 16:9 fijo) para que se vea igual de
            // proporcion que la salida real, sea la que sea.
            const float stageStripMaxH = 110.0f;
            float       stageStripH    = 0.0f;
            if (m_ShowStageStrip)
            {
                float stripW = contentW;
                float stripH = stripW / std::max(0.1f, srcAspect);
                if (stripH > stageStripMaxH) {
                    stripH = stageStripMaxH;
                    stripW = stripH * srcAspect;
                }
                stageStripH = stripH;

                ImGui::SetCursorPosY(dotsH);
                ImDrawList* dl     = ImGui::GetWindowDrawList();
                ImVec2      areaP0 = ImGui::GetCursorScreenPos();
                float       offX   = (contentW - stripW) * 0.5f;
                ImVec2      stripP0 = { areaP0.x + offX, areaP0.y };
                ImVec2      stripP1 = { stripP0.x + stripW, stripP0.y + stripH };

                dl->AddRectFilled(stripP0, stripP1, IM_COL32(10, 10, 14, 255));
                UI::DrawStageContent(dl, stripP0, stripP1);
                dl->AddRect(stripP0, stripP1, IM_COL32(90, 95, 110, 140), 0.0f, 0, 1.0f);

                ImVec2 lblSz = ImGui::CalcTextSize("STAGE");
                dl->AddRectFilled(stripP0, { stripP0.x + lblSz.x + 12.0f, stripP0.y + lblSz.y + 6.0f },
                                  IM_COL32(0, 0, 0, 160));
                dl->AddText({ stripP0.x + 6.0f, stripP0.y + 3.0f }, IM_COL32(255, 255, 255, 230), "STAGE");

                ImGui::SetCursorPosY(dotsH);
                ImGui::Dummy(ImVec2(contentW, stageStripH));
            }
            const float topReservedH = dotsH + stageStripH;

            const float remain2   = std::max(0.0f, topAreaH - topReservedH);
            const float minVideoH = 40.0f;

            // El video nunca se lleva mas del 65% de lo que queda, aunque
            // "quisiera" mas (relacion de aspecto muy vertical) — asi el
            // transporte siempre conserva un piso usable. std::clamp() en
            // este libstdc++ hace assert si hi < lo, y con remain2 chico
            // (panel muy bajo) "remain2*0.65f" puede quedar por debajo de
            // minVideoH — de ahi el std::max() en cada limite superior, para
            // que el clamp nunca reciba un rango invertido pase lo que pase
            // con el alto disponible.
            float naturalVideoH = contentW / std::max(0.1f, srcAspect);
            float videoH     = std::clamp(naturalVideoH, minVideoH, std::max(minVideoH, remain2 * 0.65f));
            // FIX: un panel angosto y muy alto (poco ancho -> poco alto
            // "natural" de 16:9, pero mucho remain2 vertical) hacia que ANTES
            // se le devolviera TODO el sobrante a videoH, mucho mas alla de
            // lo que su aspecto realmente necesita — RenderContent letterboxea
            // puertas adentro, asi que ese alto de mas no sumaba video, solo
            // franjas negras enormes arriba/abajo del recuadro real (el
            // operador lo veia como "espacio roto" entre el video y el
            // transporte). Ahora el sobrante, si lo hay, se le da al
            // transporte (hasta un tope razonable) en vez de al video; lo
            // que quede despues de eso se deja como aire al fondo del panel,
            // mucho menos llamativo que un video "flotando" en el medio de
            // una caja gigante.
            float transportH = std::clamp(remain2 - videoH, 60.0f, std::max(60.0f, kLiveTransportH * 1.3f));

            ImGui::SetCursorPosY(topReservedH);
            RenderContent(contentW, videoH);

            // RenderContent centra el video (letterbox) y puede dejar el
            // cursor antes de videoH — se fuerza la posicion para que cada
            // seccion arranque justo donde corresponde, sin importar cuanto
            // del alto reservado ocupo el letterbox.
            ImGui::SetCursorPosY(topReservedH + videoH);
            RenderLiveTransport(contentW, transportH);

            ImGui::EndChild();
        }

        if (showQuickActions)
        {
            ImGui::SameLine(0.0f, 0.0f);

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.06f, 0.09f, 1.0f));
            ImGui::BeginChild("##viewQuickActions", ImVec2(railW, topAreaH), false,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            RenderQuickActionsClear(railW);
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

        // Franja horizontal de config, ancho completo del panel (abajo del
        // video Y del riel derecho) -- ver comentario arriba.
        if (showQuickActions)
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.06f, 0.09f, 1.0f));
            ImGui::BeginChild("##viewQuickActionsConfigStrip", ImVec2(avail.x, stripH), false,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            RenderQuickActionsConfig(stripH);
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

        ImGui::PopStyleVar();
    }

    ImGui::End();
}

void ViewPanel::RenderQuickActionsClear(float railW)
{
    auto& core = Core::PresentationCore::Get();

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
    bool annLive   = announcements && announcements->IsLive();
    bool clockLive = oclock && oclock->IsLive();
    bool capLive   = capturePanel && capturePanel->IsLive();

    ImVec4 hoverClear   = ToVec4(DS::AccentColorDim);
    ImVec4 activeContent= Design::k_EQ_Yellow;
    ImVec4 textPrimary  = ToVec4(DS::TextPrimary);
    ImVec4 tintOnYellow = ImVec4(0.10f, 0.09f, 0.06f, 1.0f);
    ImVec4 baseFill     = ToVec4(DS::BtnDefaultFill);

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

    // "Limpiar <tipo especifico>" — uno por cada capa de contenido que puede
    // estar en vivo. Pedido explicito: solo iconos, nada de letras — donde
    // no habia una textura ya cargada (album/imagen/campana/reloj/camara) se
    // reusan los iconos vectoriales ya dibujados a mano en otras partes de
    // la app (AppIcons.h/HomeIcons.h) o se agregan nuevos chicos aca mismo
    // (Disco) — ver DrawIcon_Disc arriba.
    ActionDef actions[6] = {
        { "vaClearText", "", AppIcons::DrawIcon_TextAa, "Aa", "Limpiar texto",
          hoverClear, activeContent, showText,  showText  ? tintOnYellow : textPrimary },
        { "vaClearDisc", "", DrawIcon_Disc, "Dsc", "Detener disco en vivo",
          hoverClear, activeContent, discLive,  discLive  ? tintOnYellow : textPrimary },
        { "vaClearBg",   "delete", nullptr, "BG",  "Quitar fondo",
          hoverClear, activeContent, bgLive,    bgLive    ? tintOnYellow : textPrimary },
        { "vaClearAnn",  "", HomeIcons::DrawIcon_Megaphone, "Anc", "Detener anuncios",
          hoverClear, activeContent, annLive,   annLive   ? tintOnYellow : textPrimary },
        { "vaClearClock","", HomeIcons::DrawIcon_Clock, "Rlj", "Quitar reloj",
          hoverClear, activeContent, clockLive, clockLive ? tintOnYellow : textPrimary },
        { "vaClearCap",  "", HomeIcons::DrawIcon_Camera, "Cap", "Detener captura",
          hoverClear, activeContent, capLive,   capLive   ? tintOnYellow : textPrimary },
    };

    // Celdas de ancho completo, pegadas unas a otras (separadas solo por la
    // línea de 1px que dibuja QuickActionButton) — look de toolbar plano,
    // no de tarjetas sueltas.
    const ImVec2 cellSize(railW, 34.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::Dummy(ImVec2(railW, 1.0f));

    for (int i = 0; i < 6; i++)
    {
        if (QuickActionButton(actions[i].id, actions[i].icon, actions[i].vectorIcon, actions[i].fallbackGlyph,
                               actions[i].tooltip, cellSize, baseFill, actions[i].hoverColor,
                               actions[i].activeColor, actions[i].tint, actions[i].toggledOn))
        {
            if (i == 0)      core.ClearLayer2();
            else if (i == 1) core.StopBackgroundMedia();
            else if (i == 2) core.StopBackgroundMedia();
            else if (i == 3 && announcements) announcements->SetLive(false);
            else if (i == 4 && oclock)        oclock->StopTransmitting();
            else if (i == 5 && capturePanel)  capturePanel->Stop();
        }
    }

    ImGui::PopStyleVar();
}

void ViewPanel::RenderQuickActionsConfig(float stripH)
{
    auto& core = Core::PresentationCore::Get();
    bool stretchOn = core.GetStretchToFill();

    ImVec4 baseFill     = ToVec4(DS::BtnDefaultFill);
    ImVec4 hoverClear   = ToVec4(DS::AccentColorDim);
    ImVec4 activeStretch= ToVec4((DS::AccentColor & 0x00FFFFFFu) | (140u << 24));
    ImVec4 textPrimary  = ToVec4(DS::TextPrimary);

    struct ActionDef {
        const char* id;
        const char* icon;
        DrawIconFn  vectorIcon;
        const char* fallbackGlyph;
        const char* tooltip;
        ImVec4      hoverColor;
        ImVec4      activeColor;
        bool        toggledOn;
        ImVec4      tint;
    };

    // Utilidades de vista/configuracion -- separadas de "Limpiar <tipo>"
    // (riel derecho) a pedido explicito, para no mezclar accion destructiva
    // con ajuste de vista. Mute/Desmute se saco de aca (pedido explicito,
    // sobraba: el mismo control ya esta en RenderLiveTransport).
    const bool previewingStage = (m_PreviewSource == PreviewSource::Stage);

    ActionDef actions[7] = {
        { "vaStretch",   stretchOn ? "original_screen" : "fit_screen", nullptr, stretchOn ? "1:1" : "Fit",
          "Alternar proporción", hoverClear, activeStretch, stretchOn, textPrimary },
        { "vaPrefs",     "", DrawIcon_Gear, "...", "Ajustes",
          hoverClear, baseFill, false, textPrimary },
        { "vaPreviewSource", "", AppIcons::DrawIcon_Swap, "S/P",
          previewingStage ? "Viendo: Stage (click para ver Público)" : "Viendo: Público (click para ver Stage)",
          hoverClear, activeStretch, previewingStage, textPrimary },
        { "vaStageStrip", "", AppIcons::DrawIcon_Monitor, "Stg",
          m_ShowStageStrip ? "Ocultar tira de Stage" : "Mostrar tira de Stage arriba de Público",
          hoverClear, activeStretch, m_ShowStageStrip, textPrimary },
        { "vaQuality",   "", DrawIcon_Bolt, "Qty", "Calidad de salida (para PCs de bajos recursos)",
          hoverClear, activeStretch, false, textPrimary },
        { "vaChat",      "", HomeIcons::DrawIcon_Chat, "Cht", "Chat",
          hoverClear, activeStretch, false, textPrimary },
        { "vaPads",      "", AppIcons::DrawIcon_Pads, "Pds", "Pads",
          hoverClear, activeStretch, false, textPrimary },
    };

    constexpr int kCount   = 7;
    const float   totalW   = ImGui::GetContentRegionAvail().x;
    const float   cellW    = totalW / (float)kCount;
    const ImVec2  cellSize(cellW, stripH);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

    for (int i = 0; i < kCount; i++)
    {
        if (i > 0) ImGui::SameLine(0.0f, 0.0f);

        if (QuickActionButton(actions[i].id, actions[i].icon, actions[i].vectorIcon, actions[i].fallbackGlyph,
                               actions[i].tooltip, cellSize, baseFill, actions[i].hoverColor,
                               actions[i].activeColor, actions[i].tint, actions[i].toggledOn))
        {
            if (i == 0)      core.SetStretchToFill(!stretchOn);
            else if (i == 1 && m_UIManager) m_UIManager->RequestSettings();
            else if (i == 2) m_PreviewSource = previewingStage ? PreviewSource::Publico : PreviewSource::Stage;
            else if (i == 3) m_ShowStageStrip = !m_ShowStageStrip;
            else if (i == 4) ImGui::OpenPopup("##vaQualityPopup");
            else if (i == 5) ImGui::OpenPopup("##vaChatPopup");
            else if (i == 6) ImGui::OpenPopup("##vaPadsPopup");
        }
    }

    ImGui::PopStyleVar();

    RenderQualityPopup();
    RenderChatPopup();
    RenderPadsPopup();
}

void ViewPanel::RenderQualityPopup()
{
    if (!ImGui::BeginPopup("##vaQualityPopup"))
        return;

    using namespace ProyecThor::Settings;
    auto& projection = SettingsManager::Get().GetSettings().projection;
    auto  mode       = static_cast<OutputQualityMode>(projection.outputQualityMode);

    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::AccentColor));
    ImGui::TextUnformatted("CALIDAD DE SALIDA");
    ImGui::PopStyleColor();
    ImGui::Separator();

    bool changed = false;

    if (ImGui::Selectable("Auto", mode == OutputQualityMode::Auto)) {
        projection.outputQualityMode = static_cast<int>(OutputQualityMode::Auto);
        changed = true;
    }
    for (int i = 0; i < kQualityPresetCount; i++) {
        bool sel = (mode == OutputQualityMode::Preset && projection.outputPresetIndex == i);
        if (ImGui::Selectable(kQualityPresets[i].label, sel)) {
            projection.outputQualityMode = static_cast<int>(OutputQualityMode::Preset);
            projection.outputPresetIndex = i;
            changed = true;
        }
    }

    if (changed)
        SettingsManager::Get().Save();

    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextSecondary));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 220.0f);
    ImGui::TextWrapped("Baja la resolucion del video de fondo para aliviar PCs de bajos recursos. "
                       "El texto en vivo siempre se ve nitido.");
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();

    ImGui::EndPopup();
}

void ViewPanel::RenderChatPopup()
{
    // FIX: TeamChatPanel::RenderContent calcula el alto del log de mensajes
    // a partir de GetContentRegionAvail() (pensado para su hogar original,
    // un panel dockeado de alto fijo). Un popup normal se auto-ajusta al
    // contenido salvo que se le fuerce un tamaño -- con solo
    // ImGuiCond_Appearing (una vez, al abrir) el tamaño no queda fijo en los
    // frames siguientes, asi que avail crecia sin limite y con el retroalimentaba
    // el alto del log: mas contenido -> ventana mas alta -> avail mas grande
    // -> log mas alto -> ventana mas alta todavia. ImGuiCond_Always +
    // NoResize fuerza el mismo tamaño en todos los frames, como el panel
    // dockeado original.
    ImGui::SetNextWindowSize(ImVec2(380.0f, 460.0f), ImGuiCond_Always);
    if (!ImGui::BeginPopup("##vaChatPopup", ImGuiWindowFlags_NoResize))
        return;

    if (m_TeamChatPanelRef)
        m_TeamChatPanelRef->RenderContent();
    else
        ImGui::TextDisabled("Chat no disponible.");

    ImGui::EndPopup();
}

// Pads — movido tal cual desde ViewToolsPanel::RenderPads (mismo
// comportamiento, ver los helpers en el namespace anonimo de arriba).
void ViewPanel::RenderPadsPopup()
{
    ImGui::SetNextWindowSize(ImVec2(320.0f, 0.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopup("##vaPadsPopup"))
        return;

    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::AccentColor));
    ImGui::TextUnformatted("PADS");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextSecondary));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 290.0f);
    ImGui::TextWrapped("Click: aplicar. Click derecho: guardar lo que hay en pantalla "
                       "(captura + estilo/fondo + overlay activo, no la letra) o elegir icono.");
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    auto& padsArr = ProyecThor::Settings::SettingsManager::Get().GetSettings().pads.pads;

    const int   cols    = 4;
    const float btnSize = 56.0f;
    const float spacing = 10.0f;

    for (int i = 0; i < ProyecThor::Settings::kPadCount; i++)
    {
        if (i % cols != 0) ImGui::SameLine(0.0f, spacing);

        auto& pad = padsArr[i];
        const auto& icon = PadIconFor(pad.iconIndex);

        ImVec4 fillCol = pad.assigned ? MT::k_PrevBtn : ImVec4(MT::k_PrevBtn.x, MT::k_PrevBtn.y, MT::k_PrevBtn.z, 0.12f);
        ImVec4 bordCol = pad.assigned ? ImVec4(1.0f, 1.0f, 1.0f, 0.35f) : MT::k_BorderSubtle;

        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_Button,        fillCol);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  MT::k_PrevBtnHov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,   MT::k_PrevBtnAct);
        ImGui::PushStyleColor(ImGuiCol_Border,         bordCol);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,   10.0f);

        bool clicked = ImGui::Button("##pad", ImVec2(btnSize, btnSize));

        ImVec2 p       = ImGui::GetItemRectMin();
        ImVec2 s       = ImGui::GetItemRectSize();
        float  iconSz  = btnSize * 0.42f;
        ImU32  iconCol = ImGui::GetColorU32(pad.assigned ? ImVec4(1.0f, 1.0f, 1.0f, 0.92f) : MT::k_TextDim);
        icon.draw(ImGui::GetWindowDrawList(),
                  { p.x + (s.x - iconSz) * 0.5f, p.y + (s.y - iconSz) * 0.5f }, iconSz, iconCol);

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);

        if (clicked && pad.assigned) ApplyPad(pad);

        if (ImGui::BeginPopupContextItem("##padCtx")) {
            if (ImGui::MenuItem(pad.assigned ? "Guardar aqui (reemplazar)" : "Guardar aqui"))
                SavePad(pad);

            if (ImGui::BeginMenu("Elegir icono")) {
                if (RenderPadIconGrid(pad.iconIndex))
                    ProyecThor::Settings::SettingsManager::Get().Save();
                ImGui::EndMenu();
            }

            if (pad.assigned) {
                ImGui::Separator();
                if (ImGui::MenuItem("Borrar pad")) {
                    pad = PadSettings{};
                    ProyecThor::Settings::SettingsManager::Get().Save();
                }
            }
            ImGui::EndPopup();
        }

        if (pad.assigned && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
            std::string tip = "Pad " + std::to_string(i + 1);
            if (pad.hasCapture) tip += "\n- Captura";
            if (pad.hasStyle)   tip += "\n- Estilo y fondo";
            ImGui::SetTooltip("%s", tip.c_str());
        }

        ImGui::PopID();
    }

    if (auto* cap = Core::PresentationCore::Get().GetCapturePanelRef())
        cap->RenderSceneButtons();

    ImGui::EndPopup();
}

// NOTA: RenderStatusDots/StatusDotToggle/ToggleAudience/ToggleStageQuick y
// el boton "Borrar Todo" que vivian aca se mudaron a UIManager.cpp
// (RenderModeToolbarStatusActions), pedido explicito para subirlos a la
// toolbar superior y liberarle este espacio a "Vista en Vivo".

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

    // ── Transporte (pads MIDI) + fader horizontal de volumen, en una fila ────
    // Todo en una sola fila: pads de colores (estilo controlador MIDI, un
    // color fijo por accion) + mute + fader. Se probo con el fader en una
    // columna vertical a la derecha, pero en este panel el ancho sobra y el
    // alto es el que esta justo (fila baja debajo del video) -- una columna
    // vertical no entraba sin recortarse. Ademas, si el ancho disponible es
    // chico (panel angosto, riel de acciones activado), los pads y el fader
    // se ACHICAN en vez de cortarse: todo se calcula a partir de innerW en
    // vez de usar tamaños fijos.
    const float rowH      = std::clamp(ImGui::GetContentRegionAvail().y, 30.0f, 56.0f);
    const float gap       = MT::k_Gap * 1.5f;
    const float muteW     = std::clamp(rowH, 28.0f, 34.0f);
    const float minFaderW = 50.0f;
    const float minPad    = 26.0f;
    const float maxPad    = rowH;

    // 4 pads + mute + fader = 6 elementos => 5 espacios entre ellos.
    const float gapsTotal = gap * 5.0f;
    const float padSize   = std::clamp((innerW - gapsTotal - muteW - minFaderW) / 4.0f, minPad, maxPad);
    const float faderW    = std::max(minFaderW, innerW - gapsTotal - muteW - padSize * 4.0f);

    static const ImVec4 kAmber   = { 0.90f, 0.55f, 0.10f, 1.0f };
    // Rojo vivo para Play/Pausa -- coherente con el resto del panel, donde
    // rojo ya significa "en vivo" (PROGRAM - ON AIR, k_LiveAccent). Un poco
    // mas brillante que el rojo de Stop para distinguirlos entre si.
    static const ImVec4 kLiveRed = { 0.95f, 0.20f, 0.28f, 1.0f };
    static const ImVec4 kBlue    = { 0.20f, 0.55f, 0.90f, 1.0f };
    static const ImVec4 kRed     = { 0.80f, 0.16f, 0.16f, 1.0f };

    ImGui::SetCursorPosX(MT::k_PadLg);

    ImGui::PushID("vp_pad_replay");
    if (DrawPadButton("replay_10", padSize * 0.34f, kAmber, { padSize, padSize }, false)) {
        float np = livePos - (liveLen > 0 ? 10000.0f / static_cast<float>(liveLen) : 0.0f);
        core.SetLivePosition(std::max(0.0f, np));
    }
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    const char* mainIcon = m_LivePlaying ? "pause" : "play";
    ImGui::PushID("vp_pad_main");
    if (DrawPadButton(mainIcon, padSize * 0.40f, kLiveRed, { padSize, padSize }, m_LivePlaying)) {
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

    ImGui::PushID("vp_pad_fwd");
    if (DrawPadButton("forward_10", padSize * 0.34f, kBlue, { padSize, padSize }, false)) {
        float np = livePos + (liveLen > 0 ? 10000.0f / static_cast<float>(liveLen) : 0.0f);
        core.SetLivePosition(std::min(1.0f, np));
    }
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    ImGui::PushID("vp_pad_stop");
    if (DrawPadButton("stop", padSize * 0.34f, kRed, { padSize, padSize }, false)) {
        core.SetLivePosition(0.0f);
        if (bg) { bg->SetPosition(0.0f); bg->SetPause(true); }
    }
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    bool        isDanger  = (m_LiveVolume > 1.0f);
    DrawIconFn  speakerFn = m_LiveMuted ? DrawIcon_SpeakerMuted : DrawIcon_SpeakerOn;

    ImGui::PushID("vp_pad_mute");
    if (DrawPadButton(m_LiveMuted ? "no_sound" : "volume_up", muteW * 0.44f, kRed,
                      { muteW, padSize }, m_LiveMuted, speakerFn)) {
        m_LiveMuted = !m_LiveMuted;
        core.SetLiveMute(m_LiveMuted);
        core.SetLiveVolume(m_LiveMuted ? 0 : static_cast<int>(m_LiveVolume * 100.0f));
    }
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    ImU32 trackCol = ImGui::GetColorU32(MT::k_NeutBtn);
    ImU32 fillCol  = isDanger ? IM_COL32(235, 70, 70, 255) : ImGui::GetColorU32(MT::k_LiveGrab);
    ImU32 capCol   = isDanger ? IM_COL32(255, 90, 90, 255) : IM_COL32(225, 228, 235, 255);

    if (HorizontalFader("##vp_vol_fader", &m_LiveVolume, 0.0f, 2.0f, { faderW, padSize },
                         trackCol, fillCol, capCol)) {
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

    // ── 3+4. Contenido: Público (fondo+overlay+texto) o Stage (grilla/mirror)
    // Movido a UI::DrawPublicContent/DrawStageContent para poder reusarlo
    // desde el Monitor de Control (ver LiveContentRenderer.h) — el operador
    // elige la fuente con el boton "vaPreviewSource" del riel derecho.
    if (m_PreviewSource == PreviewSource::Publico)
        UI::DrawPublicContent(dl, p0, p1, drawW, drawH);
    else
        UI::DrawStageContent(dl, p0, p1);

    // ── 5b. Medidor VU chico, pegado al borde izquierdo del video ─────────
    // Antes vivia en RenderLiveTransport como una barra horizontal fija de
    // 48px de alto x todo el ancho, debajo del video. Se movio aca, chico y
    // vertical, para no robarle alto al transporte y quedar "encima" del
    // visor como en un mixer, sin estorbar. Solo tiene sentido mientras se
    // previsualiza Publico (Stage no tiene audio propio).
    if (m_PreviewSource == PreviewSource::Publico && state.isProjecting)
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
    if (m_PreviewSource == PreviewSource::Publico && core.IsBackgroundSwapPending())
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
    if (m_PreviewSource == PreviewSource::Publico && state.isProjecting && state.isStreamingNet)
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