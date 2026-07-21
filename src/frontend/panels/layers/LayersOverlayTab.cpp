#include "LayersOverlayTab.h"
#include "LayersTheme.h"
#include "backend/core/PresentationCore.h"
#include "FilePicker.h"
#include "TransitionPanel.h"
#include <imgui.h>
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <cstdlib>
#include <pwd.h>
#include <unistd.h>
#endif
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <GL/gl.h>
#include "stb_image.h"

namespace fs = std::filesystem;
namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  Rutas — mismo patron que LayersStyleTab/LayersBgTab (cada tab resuelve su
//  propia carpeta de datos).
// ─────────────────────────────────────────────────────────────────────────────
static fs::path GetAppDataDir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH] = {};
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf);
    fs::path dir = fs::path(buf) / "ProyecThor";
#else
    fs::path base;
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg) {
        base = xdg;
    } else if (const char* home = std::getenv("HOME"); home && *home) {
        base = fs::path(home) / ".local" / "share";
    } else if (struct passwd* pw = getpwuid(getuid())) {
        base = fs::path(pw->pw_dir) / ".local" / "share";
    } else {
        base = fs::current_path();
    }
    fs::path dir = base / "ProyecThor";
#endif
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}
static fs::path OverlaysDir() {
    fs::path dir = GetAppDataDir() / "assets" / "overlays";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}

// Carpeta de overlays de FoudreVue (app hermana) — mismo calculo XDG /
// %APPDATA% que GetAppDataDir() pero para "FoudreVue" en vez de
// "ProyecThor". A diferencia de OverlaysDir(), nunca la crea: solo se lee
// si FoudreVue ya la creo por su cuenta (ver ReloadList).
static fs::path FoudreVueOverlaysDir() {
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    fs::path dir = fs::path(appData ? appData : ".") / "FoudreVue";
#else
    fs::path base;
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg) {
        base = xdg;
    } else if (const char* home = std::getenv("HOME"); home && *home) {
        base = fs::path(home) / ".local" / "share";
    } else if (struct passwd* pw = getpwuid(getuid())) {
        base = fs::path(pw->pw_dir) / ".local" / "share";
    } else {
        base = fs::current_path();
    }
    fs::path dir = base / "FoudreVue";
#endif
    return dir / "assets" / "overlays";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Thumbnails (PNG ya rasterizado del overlay, generado por FoudreVue)
// ─────────────────────────────────────────────────────────────────────────────
static ImTextureID LoadImageThumb(const char* path) {
    int w, h, n;
    unsigned char* d = stbi_load(path, &w, &h, &n, 4);
    if (!d) return 0;
    GLuint tex; glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, d);
    stbi_image_free(d);
    return (ImTextureID)(intptr_t)tex;
}

ImTextureID LayersOverlayTab::GetThumbnail(const std::string& path) {
    auto it = m_ThumbnailCache.find(path);
    if (it != m_ThumbnailCache.end()) return it->second;
    std::string abs = fs::absolute(fs::path(path)).string();
    ImTextureID t = LoadImageThumb(abs.c_str());
    m_ThumbnailCache[path] = t;
    return t;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Listado — un overlay = un .png en la carpeta de overlays (lo genera
//  FoudreVue; ver ImportBundle para traer uno desde un paquete exportado).
// ─────────────────────────────────────────────────────────────────────────────
void LayersOverlayTab::ReloadList() {
    m_Overlays.clear();
    try {
        for (const auto& e : fs::directory_iterator(OverlaysDir())) {
            if (!e.is_regular_file()) continue;
            if (e.path().extension() != ".png") continue;
            m_Overlays.push_back({ e.path().stem().string(), e.path().string(), false });
        }
    } catch (const std::exception& ex) {
        std::cerr << "[LayersOverlayTab] " << ex.what() << "\n";
    }

    // Suite unificada: si FoudreVue esta instalado en esta misma maquina,
    // sus overlays ya rasterizados se leen directo de su carpeta (sin
    // copiar) — no hace falta pasar por "Importar overlay..." a mano. Un
    // nombre ya presente entre los overlays propios gana (no se pisa la
    // copia local del usuario).
    std::error_code ec;
    fs::path fvDir = FoudreVueOverlaysDir();
    if (fs::exists(fvDir, ec)) {
        try {
            for (const auto& e : fs::directory_iterator(fvDir)) {
                if (!e.is_regular_file()) continue;
                if (e.path().extension() != ".png") continue;
                std::string name = e.path().stem().string();
                bool alreadyMine = std::any_of(m_Overlays.begin(), m_Overlays.end(),
                    [&](const OverlayEntry& o) { return o.name == name; });
                if (alreadyMine) continue;
                m_Overlays.push_back({ name, e.path().string(), true });
            }
        } catch (const std::exception& ex) {
            std::cerr << "[LayersOverlayTab] " << ex.what() << "\n";
        }
    }

    std::sort(m_Overlays.begin(), m_Overlays.end(),
        [](const OverlayEntry& a, const OverlayEntry& b) { return a.name < b.name; });

    // Los macros ahora tambien se ven como tarjetas en esta misma galeria
    // (ver RenderGallery/RenderMacroCard), asi que el refresco "general"
    // (boton Actualizar + carga inicial) tiene que traer la lista de
    // macros tambien, no solo cuando se entra a la pestaña Macros.
    ReloadMacroList();
}

std::string LayersOverlayTab::ResolvePngPath(const std::string& name) {
    return (OverlaysDir() / (name + ".png")).string();
}

bool LayersOverlayTab::DeleteOverlay(const std::string& name) {
    std::error_code ec;
    fs::remove(OverlaysDir() / (name + ".png"), ec);
    m_ThumbnailCache.erase(ResolvePngPath(name));
    return true;
}

bool LayersOverlayTab::RenameOverlay(const std::string& oldName, const std::string& newName) {
    if (newName.empty() || oldName == newName) return false;
    std::error_code ec;
    fs::rename(OverlaysDir() / (oldName + ".png"), OverlaysDir() / (newName + ".png"), ec);
    m_ThumbnailCache.erase(ResolvePngPath(oldName));
    return !ec;
}

// Duplica un overlay leido desde la carpeta de FoudreVue a la carpeta
// propia de ProyecThor, para que a partir de ahi se pueda renombrar/borrar
// como cualquier overlay propio (FoudreVue sigue siendo el dueño de su
// copia original, que no se toca).
bool LayersOverlayTab::CopyExternalToMine(const OverlayEntry& e) {
    if (!e.external) return false;
    std::error_code ec;
    fs::copy_file(e.pngPath, ResolvePngPath(e.name), fs::copy_options::overwrite_existing, ec);
    if (ec) return false;
    m_ThumbnailCache.erase(ResolvePngPath(e.name));
    ReloadList();
    return true;
}

void LayersOverlayTab::SetStatus(const std::string& msg) {
    m_StatusMsg   = msg;
    m_StatusTimer = 5.0f;
}

// FoudreVue ya no tiene botones de creacion/instalacion/importacion manual
// en este panel (ver LayersOverlayTab.h): solo se sigue leyendo su carpeta
// de overlays automaticamente si esta instalada (ReloadList) — eso se
// mantiene tal cual, es la parte "que se importen solos".

// ─────────────────────────────────────────────────────────────────────────────
//  Toolbar superior — compacta, solo iconos
// ─────────────────────────────────────────────────────────────────────────────
void LayersOverlayTab::RenderTopBar() {
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, LP::TextSub);
    ImGui::TextUnformatted("Overlays");
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 14.0f);

    // ── Galeria / Macros ──────────────────────────────────────────────────
    if (LPIconToggle("Galeria", m_Mode == TabMode::Gallery, {80, 24}))
        m_Mode = TabMode::Gallery;
    ImGui::SameLine(0, 4.0f);
    if (LPIconToggle("Macros", m_Mode == TabMode::Macros, {74, 24})) {
        m_Mode = TabMode::Macros;
        if (!m_MacrosLoaded) ReloadMacroList();
    }

    const float btnSz = 26.0f;
    const float zoomW = 76.0f;
    const float gap   = 4.0f;

    if (m_Mode == TabMode::Gallery) {
        const float rowW  = zoomW + gap + btnSz * 3 + gap * 3;
        const float avail = ImGui::GetWindowContentRegionMax().x;
        ImGui::SameLine(std::max(ImGui::GetCursorPosX(), avail - rowW));

        if (m_GridMode) {
            LPZoomSlider("##ovzoom", &m_ThumbZoom, 0.65f, 1.8f, zoomW);
            ImGui::SameLine(0, gap);
        } else {
            ImGui::Dummy(ImVec2(zoomW, btnSz));
            ImGui::SameLine(0, gap);
        }

        ImGui::PushID("ovview");
        if (LPCornerIconBtn("##ovgridm", +[](ImDrawList* dl, ImVec2 c, float r, ImU32 col){
                float cs = r*0.42f, g = r*0.18f;
                for (int rI=0; rI<2; rI++) for (int cI=0; cI<2; cI++) {
                    ImVec2 o = { c.x - cs - g*0.5f + cI*(cs+g), c.y - cs - g*0.5f + rI*(cs+g) };
                    dl->AddRectFilled(o, {o.x+cs, o.y+cs}, col, 1.5f);
                }
            }, "Vista en cuadricula", {btnSz,btnSz}, m_GridMode))
            m_GridMode = true;
        ImGui::SameLine(0, gap);
        if (LPCornerIconBtn("##ovlistm", +[](ImDrawList* dl, ImVec2 c, float r, ImU32 col){
                for (int i=0;i<3;i++) {
                    float y = c.y - r*0.5f + i*r*0.5f;
                    dl->AddRectFilled({c.x-r*0.7f, y}, {c.x+r*0.7f, y+r*0.22f}, col, 1.0f);
                }
            }, "Vista en lista", {btnSz,btnSz}, !m_GridMode))
            m_GridMode = false;
        ImGui::PopID();

        ImGui::SameLine(0, gap*2);
        if (LPCornerIconBtn("##ovrefresh", LPDrawRefresh,
                "Refrescar (vuelve a leer mis overlays y los de FoudreVue)", {btnSz,btnSz}))
            ReloadList();
    } else {
        const float rowW  = btnSz + gap;
        const float avail = ImGui::GetWindowContentRegionMax().x;
        ImGui::SameLine(std::max(ImGui::GetCursorPosX(), avail - rowW));
        if (LPCornerIconBtn("##ovmacronew", LPDrawPlus, "Nuevo macro", {btnSz,btnSz}, true))
            NewMacro();
    }

    if (m_StatusTimer > 0.0f) {
        m_StatusTimer -= ImGui::GetIO().DeltaTime;
        ImGui::PushStyleColor(ImGuiCol_Text, LP::TextSub);
        ImGui::TextWrapped("%s", m_StatusMsg.c_str());
        ImGui::PopStyleColor();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tarjeta / fila
// ─────────────────────────────────────────────────────────────────────────────
void LayersOverlayTab::RenderCard(const OverlayEntry& e, float W, float H, int col, int cols) {
    ImGui::PushID(e.name.c_str());

    ImTextureID thumb = GetThumbnail(e.pngPath);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool hovRaw = ImGui::IsMouseHoveringRect(pos, {pos.x+W, pos.y+H});
    float t = LPHoverLerp(ImGui::GetID("##hov"), hovRaw);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float inset = 2.0f * t;
    ImVec2 p0 = { pos.x - inset, pos.y - inset };
    ImVec2 p1 = { pos.x + W + inset, pos.y + H + inset };

    dl->AddRectFilled(p0, p1, LPU32(LP::Surface1), 10.0f);
    if (thumb) dl->AddImageRounded(thumb, p0, p1, {0,0}, {1,1}, IM_COL32_WHITE, 10.0f);

    ImVec4 borderCol(
        LP::Border.x + (LP::Accent.x-LP::Border.x)*t,
        LP::Border.y + (LP::Accent.y-LP::Border.y)*t,
        LP::Border.z + (LP::Accent.z-LP::Border.z)*t,
        LP::Border.w + (0.6f-LP::Border.w)*t);
    dl->AddRect(p0, p1, LPU32(borderCol), 10.0f, 0, 1.0f + 0.8f*t);

    std::string dn = e.name.length() > 18 ? e.name.substr(0,15) + "..." : e.name;
    dl->AddRectFilled({p0.x, p1.y-26.0f}, {p1.x, p1.y}, LPU32({0,0,0,0.78f}), 10.0f, ImDrawFlags_RoundCornersBottom);
    ImVec2 ns = ImGui::CalcTextSize(dn.c_str());
    dl->AddText({p0.x+(W-ns.x)*0.5f, p1.y-21.0f}, LPU32(LP::Text), dn.c_str());

    if (e.external)
        LPBadge(dl, {p0.x+6.0f, p0.y+6.0f}, "FV", LP::AccentDim, LP::Accent);

    ImGui::InvisibleButton(("##ovc_"+e.name).c_str(), {W, H});
    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        Core::PresentationCore::Get().SetBackgroundMedia(e.pngPath, /*isVideo*/false, /*allowAudio*/false);

    if (ImGui::BeginPopupContextItem(("OvCtx_"+e.name).c_str())) {
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Accent);
        ImGui::Text("%s", e.name.c_str());
        ImGui::PopStyleColor();
        ImGui::Separator();
        if (e.external) {
            if (ImGui::Selectable("  Copiar a mis overlays"))
                CopyExternalToMine(e);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, LP::Red);
            if (ImGui::Selectable("  Eliminar")) {
                if (DeleteOverlay(e.name)) ReloadList();
            }
            ImGui::PopStyleColor();
        }
        ImGui::EndPopup();
    }

    if (col < cols-1) ImGui::SameLine();
    ImGui::PopID();
}

void LayersOverlayTab::RenderRow(const OverlayEntry& e, float W, float rowH) {
    const float thumbSz = 38.0f;
    ImGui::PushID(("ovr_"+e.name).c_str());

    ImTextureID thumb = GetThumbnail(e.pngPath);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool hovRaw = ImGui::IsMouseHoveringRect(pos, {pos.x+W, pos.y+rowH});
    float t = LPHoverLerp(ImGui::GetID("##hov"), hovRaw);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec4 bgCol(LP::Surface1.x+(LP::Surface2.x-LP::Surface1.x)*t,
                 LP::Surface1.y+(LP::Surface2.y-LP::Surface1.y)*t,
                 LP::Surface1.z+(LP::Surface2.z-LP::Surface1.z)*t, 1.0f);
    dl->AddRectFilled(pos, {pos.x+W,pos.y+rowH}, LPU32(bgCol), 8.0f);
    if (t > 0.01f)
        dl->AddRectFilled(pos, {pos.x+3.0f,pos.y+rowH}, LPU32(ImVec4(LP::Accent.x,LP::Accent.y,LP::Accent.z,t)), 2.0f);

    float tx = pos.x+8.0f, ty = pos.y+(rowH-thumbSz)*0.5f;
    if (thumb) dl->AddImageRounded(thumb, {tx,ty}, {tx+thumbSz,ty+thumbSz}, {0,0},{1,1}, IM_COL32_WHITE, 5.0f);
    else       dl->AddRectFilled({tx,ty}, {tx+thumbSz,ty+thumbSz}, LPU32(LP::Surface0), 5.0f);

    std::string dn = e.name.length() > 32 ? e.name.substr(0,29) + "..." : e.name;
    float labelX = tx+thumbSz+10.0f;
    dl->AddText({labelX, pos.y+(rowH-ImGui::GetTextLineHeight())*0.5f}, LPU32(LP::Text), dn.c_str());
    if (e.external) {
        ImVec2 lsz = ImGui::CalcTextSize(dn.c_str());
        LPBadge(dl, {labelX+lsz.x+10.0f, pos.y+(rowH-ImGui::GetTextLineHeight())*0.5f},
                "FV", LP::AccentDim, LP::Accent);
    }

    ImGui::InvisibleButton(("##ovrow_"+e.name).c_str(), {W, rowH});
    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        Core::PresentationCore::Get().SetBackgroundMedia(e.pngPath, /*isVideo*/false, /*allowAudio*/false);

    if (ImGui::BeginPopupContextItem(("OvRowCtx_"+e.name).c_str())) {
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Accent);
        ImGui::Text("%s", e.name.c_str());
        ImGui::PopStyleColor();
        ImGui::Separator();
        if (e.external) {
            if (ImGui::Selectable("  Copiar a mis overlays"))
                CopyExternalToMine(e);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, LP::Red);
            if (ImGui::Selectable("  Eliminar")) {
                if (DeleteOverlay(e.name)) ReloadList();
            }
            ImGui::PopStyleColor();
        }
        ImGui::EndPopup();
    }

    ImGui::Dummy({0, 5.0f});
    ImGui::PopID();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tarjetas de macro en la Galeria — mismo esqueleto visual que
//  RenderCard/RenderRow (overlays estaticos), pero el click reproduce el
//  macro directamente (core.PlayMacro) en vez de aplicar un fondo. La
//  pestaña "Macros" (RenderMacrosBrowser/RenderMacroEditor) queda entonces
//  solo para crear/editar — reproducir un macro guardado ya no depende de
//  entrar ahi.
// ─────────────────────────────────────────────────────────────────────────────
void LayersOverlayTab::RenderMacroCard(const std::string& macroName, float W, float H, int col, int cols) {
    ImGui::PushID(("mc_" + macroName).c_str());

    auto& core = Core::PresentationCore::Get();
    bool  isActive = core.IsMacroPlaying() && core.GetActiveMacroName() == macroName;

    std::string thumbPath = ResolveMacroThumbPath(macroName);
    ImTextureID thumb = thumbPath.empty() ? (ImTextureID)0 : GetThumbnail(thumbPath);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool hovRaw = ImGui::IsMouseHoveringRect(pos, {pos.x+W, pos.y+H});
    float t = LPHoverLerp(ImGui::GetID("##hov"), hovRaw);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float inset = 2.0f * t;
    ImVec2 p0 = { pos.x - inset, pos.y - inset };
    ImVec2 p1 = { pos.x + W + inset, pos.y + H + inset };

    dl->AddRectFilled(p0, p1, LPU32(LP::Surface1), 10.0f);
    if (thumb)
        dl->AddImageRounded(thumb, p0, p1, {0,0}, {1,1}, IM_COL32(200,200,200,255), 10.0f);
    else {
        // Sin thumbnail (macro vacio o sin cue de fondo/overlay con imagen):
        // tarjeta neutra con el glifo de macro centrado.
        const char* glyph = "MACRO";
        ImVec2 gs = ImGui::CalcTextSize(glyph);
        dl->AddText({p0.x+(W-gs.x)*0.5f, p0.y+(H-gs.y)*0.5f - 8.0f}, LPU32(LP::TextMuted), glyph);
    }

    ImVec4 borderCol = isActive
        ? ImVec4(LP::Green.x, LP::Green.y, LP::Green.z, 1.0f)
        : ImVec4(LP::Border.x + (LP::Accent.x-LP::Border.x)*t,
                 LP::Border.y + (LP::Accent.y-LP::Border.y)*t,
                 LP::Border.z + (LP::Accent.z-LP::Border.z)*t,
                 LP::Border.w + (0.6f-LP::Border.w)*t);
    dl->AddRect(p0, p1, LPU32(borderCol), 10.0f, 0, isActive ? 1.8f : (1.0f + 0.8f*t));

    LPBadge(dl, {p0.x+6.0f, p0.y+6.0f}, "MACRO", LP::AccentDim, LP::Accent);

    std::string dn = macroName.length() > 18 ? macroName.substr(0,15) + "..." : macroName;
    dl->AddRectFilled({p0.x, p1.y-26.0f}, {p1.x, p1.y}, LPU32({0,0,0,0.78f}), 10.0f, ImDrawFlags_RoundCornersBottom);
    ImVec2 ns = ImGui::CalcTextSize(dn.c_str());
    dl->AddText({p0.x+(W-ns.x)*0.5f, p1.y-21.0f}, LPU32(LP::Text), dn.c_str());

    // "PLAY" al centro en hover (o "REPRODUCIENDO" si ya esta activo).
    if (isActive || t > 0.02f) {
        const char* overlay = isActive ? "REPRODUCIENDO" : "▶ REPRODUCIR";
        ImU32 overlayCol = isActive ? LPU32(LP::Green) : LPU32(ImVec4(1,1,1,t));
        ImVec2 os = ImGui::CalcTextSize(overlay);
        dl->AddRectFilled({p0.x+(W-os.x)*0.5f-8.0f, p0.y+(H-26.0f-os.y)*0.5f-4.0f},
                          {p0.x+(W+os.x)*0.5f+8.0f, p0.y+(H-26.0f+os.y)*0.5f+4.0f},
                          IM_COL32(10,10,12, isActive ? 200 : (int)(160*t)), 6.0f);
        dl->AddText({p0.x+(W-os.x)*0.5f, p0.y+(H-26.0f-os.y)*0.5f}, overlayCol, overlay);
    }

    ImGui::InvisibleButton(("##mcbtn_"+macroName).c_str(), {W, H});
    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !isActive)
        core.PlayMacro(macroName, core.GetMacroAutoAdvance());

    if (ImGui::BeginPopupContextItem(("McCtx_"+macroName).c_str())) {
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Accent);
        ImGui::Text("%s", macroName.c_str());
        ImGui::PopStyleColor();
        ImGui::Separator();
        if (ImGui::Selectable("  Editar")) {
            m_Mode = TabMode::Macros;
            if (!m_MacrosLoaded) ReloadMacroList();
            OpenMacro(macroName);
        }
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Red);
        if (ImGui::Selectable("  Eliminar")) {
            if (isActive) core.StopMacro();
            Core::DeleteMacroFile(macroName);
            ReloadMacroList();
        }
        ImGui::PopStyleColor();
        ImGui::EndPopup();
    }

    if (col < cols-1) ImGui::SameLine();
    ImGui::PopID();
}

void LayersOverlayTab::RenderMacroRow(const std::string& macroName, float W, float rowH) {
    const float thumbSz = 38.0f;
    ImGui::PushID(("mr_" + macroName).c_str());

    auto& core = Core::PresentationCore::Get();
    bool  isActive = core.IsMacroPlaying() && core.GetActiveMacroName() == macroName;

    std::string thumbPath = ResolveMacroThumbPath(macroName);
    ImTextureID thumb = thumbPath.empty() ? (ImTextureID)0 : GetThumbnail(thumbPath);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool hovRaw = ImGui::IsMouseHoveringRect(pos, {pos.x+W, pos.y+rowH});
    float t = LPHoverLerp(ImGui::GetID("##hov"), hovRaw);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec4 bgCol = isActive
        ? ImVec4(0.12f, 0.40f, 0.20f, 0.35f)
        : ImVec4(LP::Surface1.x+(LP::Surface2.x-LP::Surface1.x)*t,
                 LP::Surface1.y+(LP::Surface2.y-LP::Surface1.y)*t,
                 LP::Surface1.z+(LP::Surface2.z-LP::Surface1.z)*t, 1.0f);
    dl->AddRectFilled(pos, {pos.x+W,pos.y+rowH}, LPU32(bgCol), 8.0f);
    if (isActive || t > 0.01f)
        dl->AddRectFilled(pos, {pos.x+3.0f,pos.y+rowH},
            LPU32(isActive ? LP::Green : ImVec4(LP::Accent.x,LP::Accent.y,LP::Accent.z,t)), 2.0f);

    float tx = pos.x+8.0f, ty = pos.y+(rowH-thumbSz)*0.5f;
    if (thumb) dl->AddImageRounded(thumb, {tx,ty}, {tx+thumbSz,ty+thumbSz}, {0,0},{1,1}, IM_COL32(200,200,200,255), 5.0f);
    else       dl->AddRectFilled({tx,ty}, {tx+thumbSz,ty+thumbSz}, LPU32(LP::Surface0), 5.0f);

    std::string dn = macroName.length() > 32 ? macroName.substr(0,29) + "..." : macroName;
    float labelX = tx+thumbSz+10.0f;
    dl->AddText({labelX, pos.y+(rowH-ImGui::GetTextLineHeight())*0.5f}, LPU32(LP::Text), dn.c_str());
    ImVec2 lsz = ImGui::CalcTextSize(dn.c_str());
    LPBadge(dl, {labelX+lsz.x+10.0f, pos.y+(rowH-ImGui::GetTextLineHeight())*0.5f},
            isActive ? "REPRODUCIENDO" : "MACRO", isActive ? LP::GreenDim : LP::AccentDim,
            isActive ? LP::Green : LP::Accent);

    ImGui::InvisibleButton(("##mrbtn_"+macroName).c_str(), {W, rowH});
    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !isActive)
        core.PlayMacro(macroName, core.GetMacroAutoAdvance());

    if (ImGui::BeginPopupContextItem(("McRowCtx_"+macroName).c_str())) {
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Accent);
        ImGui::Text("%s", macroName.c_str());
        ImGui::PopStyleColor();
        ImGui::Separator();
        if (ImGui::Selectable("  Editar")) {
            m_Mode = TabMode::Macros;
            if (!m_MacrosLoaded) ReloadMacroList();
            OpenMacro(macroName);
        }
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Red);
        if (ImGui::Selectable("  Eliminar")) {
            if (isActive) core.StopMacro();
            Core::DeleteMacroFile(macroName);
            ReloadMacroList();
        }
        ImGui::PopStyleColor();
        ImGui::EndPopup();
    }

    ImGui::Dummy({0, 5.0f});
    ImGui::PopID();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Galeria
// ─────────────────────────────────────────────────────────────────────────────
void LayersOverlayTab::RenderGallery() {
    // Los macros guardados se muestran ACA tambien (ademas de en la pestaña
    // Macros, que ahora es solo para editarlos): una sola galeria con
    // overlays estaticos + macros reproducibles, en vez de que reproducir
    // un macro dependa de entrar a otra pestaña.
    if (!m_MacrosLoaded) ReloadMacroList();

    if (m_Overlays.empty() && m_MacroNames.empty()) {
        ImGui::Dummy({0,16});
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w  = ImGui::GetContentRegionAvail().x;
        ImGui::GetWindowDrawList()->AddRectFilled(p, {p.x+w,p.y+64}, LPU32(LP::Surface1), 10.0f);
        ImGui::Dummy({0,12});
        ImGui::PushStyleColor(ImGuiCol_Text, LP::TextMuted);
        const char* msg = "Sin overlays ni macros aun. Los overlays estaticos se leen automaticamente de FoudreVue si esta instalado; los macros se crean con el boton + de la pestaña Macros.";
        float tw = ImGui::CalcTextSize(msg).x;
        ImGui::SetCursorPosX(std::max(0.0f, (w-tw)*0.5f));
        ImGui::Text("%s", msg);
        ImGui::PopStyleColor();
        return;
    }

    int totalItems = (int)m_Overlays.size() + (int)m_MacroNames.size();

    if (m_GridMode) {
        const float cW = 150.0f * m_ThumbZoom;
        const float cH = 92.0f  * m_ThumbZoom;
        const float minGap = 10.0f;

        float availWidth = ImGui::GetContentRegionAvail().x;
        int cols = std::max(1, static_cast<int>((availWidth + minGap) / (cW + minGap)));

        float totalGaps = static_cast<float>(cols - 1);
        float dynamicGap = minGap;
        if (totalGaps > 0) {
            float extraSpace = availWidth - (cols * cW);
            dynamicGap = std::max(minGap, extraSpace / totalGaps);
        }

        int currentCol = 0;
        for (int i = 0; i < totalItems; ++i) {
            if (i < (int)m_Overlays.size())
                RenderCard(m_Overlays[i], cW, cH, currentCol, cols);
            else
                RenderMacroCard(m_MacroNames[i - (int)m_Overlays.size()], cW, cH, currentCol, cols);
            currentCol++;
            if (currentCol < cols) {
                ImGui::SameLine(0.0f, dynamicGap);
            } else {
                currentCol = 0;
                ImGui::Dummy({0.0f, minGap});
            }
        }
    } else {
        float w = ImGui::GetContentRegionAvail().x;
        for (const auto& e : m_Overlays)
            RenderRow(e, w, 44.0f);
        for (const auto& name : m_MacroNames)
            RenderMacroRow(name, w, 44.0f);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Render principal
// ─────────────────────────────────────────────────────────────────────────────
void LayersOverlayTab::Render() {
    if (!m_Loaded) {
        ReloadList();
        m_Loaded = true;
    }

    RenderTopBar();
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    LPSeparatorLine();
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    if (m_Mode == TabMode::Gallery) {
        RenderGallery();
    } else {
        if (!m_MacrosLoaded) ReloadMacroList();
        if (m_HasEditingMacro) RenderMacroEditor();
        else                   RenderMacrosBrowser();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Macros — lista guardada
// ─────────────────────────────────────────────────────────────────────────────
void LayersOverlayTab::ReloadMacroList() {
    m_MacroNames   = Core::ListMacroNames();
    m_MacrosLoaded = true;
    m_MacroThumbPathCache.clear();
}

std::string LayersOverlayTab::ResolveMacroThumbPath(const std::string& macroName) {
    auto it = m_MacroThumbPathCache.find(macroName);
    if (it != m_MacroThumbPathCache.end()) return it->second;

    std::string path;
    Core::Macro m;
    if (Core::LoadMacro(macroName, m)) {
        for (const auto& cue : m.cues) {
            bool usable = !cue.param.empty() &&
                ((cue.type == Core::MacroCueType::ChangeBackground && !cue.paramIsVideo) ||
                 cue.type == Core::MacroCueType::SetOverlay);
            if (usable) { path = cue.param; break; }
        }
    }
    m_MacroThumbPathCache[macroName] = path;
    return path;
}

void LayersOverlayTab::NewMacro() {
    m_EditingMacro = Core::Macro{};
    m_EditingMacro.name = "Macro sin nombre";
    m_HasEditingMacro = true;
    m_EditingCueIndex = -1;
}

void LayersOverlayTab::OpenMacro(const std::string& name) {
    Core::Macro m;
    if (!Core::LoadMacro(name, m)) {
        SetStatus("No se pudo abrir el macro \"" + name + "\".");
        return;
    }
    m_EditingMacro    = std::move(m);
    m_HasEditingMacro = true;
    m_EditingCueIndex = -1;
}

void LayersOverlayTab::SaveEditingMacro() {
    if (m_EditingMacro.name.empty()) {
        SetStatus("Ponele un nombre al macro antes de guardar.");
        return;
    }
    Core::SortCues(m_EditingMacro);
    if (Core::SaveMacro(m_EditingMacro)) {
        SetStatus("Macro \"" + m_EditingMacro.name + "\" guardado.");
        ReloadMacroList();
    } else {
        SetStatus("No se pudo guardar el macro.");
    }
}

void LayersOverlayTab::RenderMacrosBrowser() {
    if (m_MacroNames.empty()) {
        ImGui::Dummy({0,16});
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w  = ImGui::GetContentRegionAvail().x;
        ImGui::GetWindowDrawList()->AddRectFilled(p, {p.x+w,p.y+64}, LPU32(LP::Surface1), 10.0f);
        ImGui::Dummy({0,12});
        ImGui::PushStyleColor(ImGuiCol_Text, LP::TextMuted);
        const char* msg = "Sin macros aun. Creá uno con el boton + de arriba.";
        float tw = ImGui::CalcTextSize(msg).x;
        ImGui::SetCursorPosX(std::max(0.0f, (w-tw)*0.5f));
        ImGui::Text("%s", msg);
        ImGui::PopStyleColor();
        return;
    }

    auto& core = Core::PresentationCore::Get();
    const float rowH = 40.0f;
    float w = ImGui::GetContentRegionAvail().x;

    for (const auto& name : m_MacroNames) {
        ImGui::PushID(name.c_str());

        bool isActive = core.IsMacroPlaying() && core.GetActiveMacroName() == name;

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        bool hovRaw = ImGui::IsMouseHoveringRect(pos, {pos.x+w, pos.y+rowH});
        float t = LPHoverLerp(ImGui::GetID("##hov"), hovRaw);

        ImVec4 bgCol = isActive
            ? ImVec4(0.12f, 0.40f, 0.20f, 0.35f)
            : ImVec4(LP::Surface1.x+(LP::Surface2.x-LP::Surface1.x)*t,
                     LP::Surface1.y+(LP::Surface2.y-LP::Surface1.y)*t,
                     LP::Surface1.z+(LP::Surface2.z-LP::Surface1.z)*t, 1.0f);
        dl->AddRectFilled(pos, {pos.x+w,pos.y+rowH}, LPU32(bgCol), 8.0f);
        if (isActive)
            dl->AddRectFilled(pos, {pos.x+3.0f,pos.y+rowH}, LPU32(LP::Accent), 2.0f);

        dl->AddText({pos.x+14.0f, pos.y+(rowH-ImGui::GetTextLineHeight())*0.5f},
                    LPU32(isActive ? LP::Accent : LP::Text), name.c_str());
        if (isActive) {
            const char* tag = "REPRODUCIENDO";
            ImVec2 ts = ImGui::CalcTextSize(tag);
            dl->AddText({pos.x+w-ts.x-14.0f, pos.y+(rowH-ts.y)*0.5f}, LPU32(LP::Accent), tag);
        }

        ImGui::InvisibleButton("##macrorow", {w, rowH});
        if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            OpenMacro(name);

        if (ImGui::BeginPopupContextItem(("MacroCtx_"+name).c_str())) {
            ImGui::PushStyleColor(ImGuiCol_Text, LP::Accent);
            ImGui::Text("%s", name.c_str());
            ImGui::PopStyleColor();
            ImGui::Separator();
            if (ImGui::Selectable("  Editar")) OpenMacro(name);
            ImGui::PushStyleColor(ImGuiCol_Text, LP::Red);
            if (ImGui::Selectable("  Eliminar")) {
                if (isActive) core.StopMacro();
                Core::DeleteMacroFile(name);
                ReloadMacroList();
            }
            ImGui::PopStyleColor();
            ImGui::EndPopup();
        }

        ImGui::Dummy({0, 5.0f});
        ImGui::PopID();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Macros — editor de un macro (lista de cues + formulario)
// ─────────────────────────────────────────────────────────────────────────────
void LayersOverlayTab::RenderMacroEditor() {
    auto& core = Core::PresentationCore::Get();
    bool isThisMacroActive = core.IsMacroPlaying() && core.GetActiveMacroName() == m_EditingMacro.name;

    // Contenedor con padding/borde propio — antes el editor dibujaba
    // directo sobre el fondo desnudo de la pestaña, sin ninguna
    // contencion visual (a diferencia del resto de los editores/modales de
    // la app, ver SongView::RenderEditorModal).
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, LP::Surface0);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.08f));
    ImGui::BeginChild("##macroEditorCard", ImGui::GetContentRegionAvail(), true);

    // ── Cabecera: volver + nombre + guardar ──────────────────────────────
    bool backClicked = LPGhostBtn("< Volver");
    ImGui::SameLine();

    ImGui::SetNextItemWidth(240.0f);
    char nameBuf[128];
    strncpy(nameBuf, m_EditingMacro.name.c_str(), sizeof(nameBuf)-1);
    nameBuf[sizeof(nameBuf)-1] = '\0';
    if (ImGui::InputText("##macroName", nameBuf, sizeof(nameBuf)))
        m_EditingMacro.name = nameBuf;

    ImGui::SameLine();
    if (LPPrimaryBtn("Guardar")) SaveEditingMacro();

    ImGui::Dummy({0, 8});

    // ── Transporte: reproducir / detener / auto-manual / anterior-siguiente ──
    {
        if (!isThisMacroActive) {
            if (LPPrimaryBtn("Reproducir")) {
                SaveEditingMacro();
                core.PlayMacro(m_EditingMacro.name, core.GetMacroAutoAdvance());
            }
        } else {
            if (LPGhostBtn("Detener")) core.StopMacro();
        }
        ImGui::SameLine();

        bool autoAdv = core.GetMacroAutoAdvance();
        if (LPIconToggle("Automatico", autoAdv, {96, 24})) core.SetMacroAutoAdvance(true);
        ImGui::SameLine();
        if (LPIconToggle("Manual", !autoAdv, {74, 24})) core.SetMacroAutoAdvance(false);

        if (isThisMacroActive) {
            ImGui::SameLine();
            if (LPGhostBtn("< Cue")) core.PrevMacroCue();
            ImGui::SameLine();
            if (LPGhostBtn("Cue >")) core.NextMacroCue();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, LP::TextMuted);
            ImGui::Text("Cue %d/%d", core.GetMacroCueIndex()+1, core.GetMacroCueCount());
            ImGui::PopStyleColor();
        }
    }

    ImGui::Dummy({0, 10});
    LPSeparatorLine();
    ImGui::Dummy({0, 10});

    // ── Grid de cues (tarjetas tipo ProPresenter, ver reference: thumbnail +
    //    barra de color inferior por tipo) — reemplaza la lista de filas
    //    finas de antes, mismo lenguaje visual que las tarjetas de
    //    Fondos/Overlays (RenderCard/RenderBgCard) y las de SongView. ──────
    const float tileW  = 150.0f;
    const float tileH  = 92.0f;
    const float tileBarH = 22.0f;
    const float minGap = 10.0f;
    int removeIdx  = -1;
    int dragSrcIdx = -1;
    int dragDstIdx = -1;

    auto CueTypeColor = [](Core::MacroCueType t) -> ImU32 {
        switch (t) {
            case Core::MacroCueType::ChangeBackground: return IM_COL32( 74, 130, 214, 255); // azul
            case Core::MacroCueType::SetOverlay:       return IM_COL32(150, 100, 214, 255); // violeta
            case Core::MacroCueType::ClearOverlay:     return IM_COL32( 92,  92,  98, 255); // gris
            case Core::MacroCueType::ShowText:         return IM_COL32( 96, 190, 110, 255); // verde
            case Core::MacroCueType::ClearText:        return IM_COL32( 92,  92,  98, 255); // gris
            case Core::MacroCueType::ChangeClockStyle: return IM_COL32(214, 150,  64, 255); // naranja
        }
        return IM_COL32(92, 92, 98, 255);
    };

    if (m_EditingMacro.cues.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, LP::TextMuted);
        ImGui::TextUnformatted("Sin cues todavia. Agrega la primera con el formulario de abajo.");
        ImGui::PopStyleColor();
    } else {
        float availWidth = ImGui::GetContentRegionAvail().x;
        int   cols       = std::max(1, static_cast<int>((availWidth + minGap) / (tileW + minGap)));
        int   currentCol = 0;

        for (int i = 0; i < (int)m_EditingMacro.cues.size(); i++) {
            Core::MacroCue& cue = m_EditingMacro.cues[i];
            ImGui::PushID(i);

            bool isCurrentCue = isThisMacroActive && core.GetMacroCueIndex() == i;

            ImVec2      p0 = ImGui::GetCursorScreenPos();
            ImVec2      p1 = { p0.x + tileW, p0.y + tileH };
            ImDrawList* dl = ImGui::GetWindowDrawList();

            bool  hovRaw = ImGui::IsMouseHoveringRect(p0, p1);
            float t      = LPHoverLerp(ImGui::GetID("##cuehov"), hovRaw);

            ImU32 bg = isCurrentCue ? LPU32(ImVec4(0.12f, 0.40f, 0.20f, 0.35f)) : LPU32(LP::Surface1);
            dl->AddRectFilled(p0, p1, bg, 10.0f);

            ImU32 borderCol = isCurrentCue ? LPU32(LP::Accent)
                             : LPU32(ImVec4(LP::Border.x + (LP::Accent.x - LP::Border.x) * t,
                                            LP::Border.y + (LP::Accent.y - LP::Border.y) * t,
                                            LP::Border.z + (LP::Accent.z - LP::Border.z) * t,
                                            LP::Border.w + (1.0f - LP::Border.w) * t));
            dl->AddRect(p0, p1, borderCol, 10.0f, 0, isCurrentCue ? 1.8f : (1.0f + t * 0.6f));

            // ── Centro: thumbnail si es fondo/overlay con imagen, si no el
            //    nombre de la accion ────────────────────────────────────────
            ImVec2 contentMax = { p1.x, p1.y - tileBarH };
            dl->PushClipRect(p0, contentMax, true);

            ImTextureID thumb = (ImTextureID)0;
            bool wantsThumb = !cue.param.empty() &&
                ((cue.type == Core::MacroCueType::ChangeBackground && !cue.paramIsVideo) ||
                 cue.type == Core::MacroCueType::SetOverlay);
            if (wantsThumb) thumb = GetThumbnail(cue.param);

            if (thumb) {
                dl->AddImageRounded(thumb, p0, contentMax, {0, 0}, {1, 1}, IM_COL32(215, 215, 215, 255), 10.0f);
            } else {
                const char* typeLbl = Core::MacroCueTypeLabel(cue.type);
                ImVec2 lblSz = ImGui::CalcTextSize(typeLbl);
                dl->AddText({ p0.x + (tileW - lblSz.x) * 0.5f, p0.y + (tileH - tileBarH - lblSz.y) * 0.5f },
                            LPU32(LP::Text), typeLbl);
            }
            dl->PopClipRect();

            // ── Barra inferior: color por tipo + tiempo + preview del param ──
            ImVec2 barMin = { p0.x, p1.y - tileBarH };
            dl->AddRectFilled(barMin, p1, CueTypeColor(cue.type), 10.0f, ImDrawFlags_RoundCornersBottom);

            char timeBuf[16];
            snprintf(timeBuf, sizeof(timeBuf), "%.1fs", cue.timeSeconds);
            dl->AddText({ barMin.x + 6.0f, barMin.y + (tileBarH - ImGui::GetTextLineHeight()) * 0.5f },
                        IM_COL32(18, 18, 20, 235), timeBuf);

            if (!cue.param.empty()) {
                std::string preview = fs::path(cue.param).filename().string();
                if (preview.empty()) preview = cue.param;
                float maxW = tileW - 44.0f;
                while (preview.size() > 3 && ImGui::CalcTextSize(preview.c_str()).x > maxW)
                    preview.resize(preview.size() - 1);
                float pw = ImGui::CalcTextSize(preview.c_str()).x;
                dl->AddText({ barMin.x + tileW - pw - 6.0f, barMin.y + (tileBarH - ImGui::GetTextLineHeight()) * 0.5f },
                            IM_COL32(18, 18, 20, 200), preview.c_str());
            }

            // ── Interaccion: click = editar, arrastrar = reordenar ──────────
            ImGui::SetCursorScreenPos(p0);
            if (ImGui::InvisibleButton("##cuetile", { tileW, tileH }))
                m_EditingCueIndex = i;

            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("MACRO_CUE_REORDER", &i, sizeof(int));
                ImGui::TextUnformatted(Core::MacroCueTypeLabel(cue.type));
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MACRO_CUE_REORDER")) {
                    int srcIdx = *(const int*)payload->Data;
                    if (srcIdx != i) { dragSrcIdx = srcIdx; dragDstIdx = i; }
                }
                ImGui::EndDragDropTarget();
            }

            if (ImGui::BeginPopupContextItem("##cuectx")) {
                if (ImGui::Selectable("Editar")) m_EditingCueIndex = i;
                ImGui::PushStyleColor(ImGuiCol_Text, LP::Red);
                if (ImGui::Selectable("Eliminar")) removeIdx = i;
                ImGui::PopStyleColor();
                ImGui::EndPopup();
            }

            // "x" de borrado rapido, visible solo en hover.
            if (t > 0.05f) {
                ImVec2 xC = { p1.x - 12.0f, p0.y + 12.0f };
                dl->AddCircleFilled(xC, 9.0f, IM_COL32(20, 20, 22, (int)(200 * t)));
                ImVec2 xSz = ImGui::CalcTextSize("x");
                dl->AddText({ xC.x - xSz.x * 0.5f, xC.y - xSz.y * 0.5f }, IM_COL32(255, 130, 130, (int)(255 * t)), "x");
                ImGui::SetCursorScreenPos({ xC.x - 9.0f, xC.y - 9.0f });
                if (ImGui::InvisibleButton("##cuedel", { 18.0f, 18.0f })) removeIdx = i;
            }

            ImGui::PopID();

            currentCol++;
            if (currentCol < cols) {
                ImGui::SameLine(0.0f, minGap);
            } else {
                currentCol = 0;
                ImGui::Dummy({ 0.0f, minGap });
            }
        }
        if (currentCol != 0) ImGui::Dummy({ 0.0f, minGap });
    }

    if (removeIdx >= 0)
        m_EditingMacro.cues.erase(m_EditingMacro.cues.begin() + removeIdx);

    if (dragSrcIdx >= 0 && dragDstIdx >= 0 && dragSrcIdx != dragDstIdx &&
        dragSrcIdx < (int)m_EditingMacro.cues.size()) {
        // Se preserva el CONJUNTO de timeSeconds existente (solo se
        // reordena), asi el auto-avance conserva el mismo espaciado en vez
        // de renumerar a segundos redondos y perder el ajuste fino que ya
        // tenia el operador.
        std::vector<float> times;
        times.reserve(m_EditingMacro.cues.size());
        for (auto& c : m_EditingMacro.cues) times.push_back(c.timeSeconds);
        std::sort(times.begin(), times.end());

        Core::MacroCue moved = m_EditingMacro.cues[dragSrcIdx];
        m_EditingMacro.cues.erase(m_EditingMacro.cues.begin() + dragSrcIdx);
        int insertAt = dragDstIdx;
        if (dragSrcIdx < dragDstIdx) insertAt--;
        insertAt = std::clamp(insertAt, 0, (int)m_EditingMacro.cues.size());
        m_EditingMacro.cues.insert(m_EditingMacro.cues.begin() + insertAt, moved);

        for (int k = 0; k < (int)m_EditingMacro.cues.size() && k < (int)times.size(); k++)
            m_EditingMacro.cues[k].timeSeconds = times[k];
    }

    ImGui::Dummy({0, 10});
    LPSeparatorLine();
    ImGui::Dummy({0, 10});

    RenderCueForm();

    if (backClicked) m_HasEditingMacro = false;

    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

// Selector de archivo (imagen o video) y sniffing de extension: extraidos a
// frontend/ui/FilePicker.h — este mismo patron zenity/kdialog/IFileOpenDialog
// ya se necesitaba en 3 lugares (Audio.cpp, LayersBgTab.cpp, y aca), asi que
// se centraliza en vez de triplicar el codigo especifico de plataforma.
using ProyecThor::UI::PickImageOrVideoFile;
using ProyecThor::UI::LooksLikeVideoPath;

// ─────────────────────────────────────────────────────────────────────────────
//  Formulario para agregar/editar una cue
// ─────────────────────────────────────────────────────────────────────────────
// Opciones de transicion para el combo de la cue — labels cortos propios en
// vez de reusar los de TransitionPanel::RenderContent (esos incluyen
// flechas/acentos pensados para tarjetas grandes, aca es un combo angosto).
// El nombre persistido (para MacroCue::transitionName) sale de
// UI::TransitionTypeToName, no de este array, asi que quedan desacoplados.
namespace {
struct CueTransitionOption { TransitionType type; const char* label; };
constexpr CueTransitionOption kCueTransitions[] = {
    { TransitionType::None,         "Sin transicion" },
    { TransitionType::Fade,         "Disolver" },
    { TransitionType::ZoomIn,       "Zoom In" },
    { TransitionType::ZoomOut,      "Zoom Out" },
    { TransitionType::SlideLeft,    "Barrido izquierda" },
    { TransitionType::SlideRight,   "Barrido derecha" },
    { TransitionType::SlideUp,      "Barrido arriba" },
    { TransitionType::SlideDown,    "Barrido abajo" },
    { TransitionType::CoverLeft,    "Cubrir izquierda" },
    { TransitionType::CoverRight,   "Cubrir derecha" },
    { TransitionType::CoverUp,      "Cubrir arriba" },
    { TransitionType::CoverDown,    "Cubrir abajo" },
    { TransitionType::UncoverLeft,  "Revelar izquierda" },
    { TransitionType::UncoverRight, "Revelar derecha" },
    { TransitionType::UncoverUp,    "Revelar arriba" },
    { TransitionType::UncoverDown,  "Revelar abajo" },
};
constexpr int kCueTransitionCount = sizeof(kCueTransitions) / sizeof(kCueTransitions[0]);
} // namespace

void LayersOverlayTab::RenderCueForm() {
    static float       s_Time      = 0.0f;
    static int         s_TypeIdx   = 0;
    static char        s_ParamBuf[256] = {};
    static bool        s_IsVideo   = false;
    static bool        s_TextStandalone     = false;
    static int         s_TransitionIdx      = -1;   // -1 = usar la transicion actual (heredar)
    static float       s_TransitionDuration = -1.0f; // -1 = usar la duracion actual
    static int         s_LoadedFor = -2; // evita repoblar el form en cada frame

    const Core::MacroCueType kTypes[] = {
        Core::MacroCueType::ChangeBackground,
        Core::MacroCueType::SetOverlay,
        Core::MacroCueType::ClearOverlay,
        Core::MacroCueType::ShowText,
        Core::MacroCueType::ClearText,
        Core::MacroCueType::ChangeClockStyle,
    };
    constexpr int kTypeCount = sizeof(kTypes) / sizeof(kTypes[0]);

    // Repoblar el formulario cuando cambia que cue se esta editando (o al
    // entrar en modo "agregar").
    if (s_LoadedFor != m_EditingCueIndex) {
        s_LoadedFor = m_EditingCueIndex;
        if (m_EditingCueIndex >= 0 && m_EditingCueIndex < (int)m_EditingMacro.cues.size()) {
            const Core::MacroCue& c = m_EditingMacro.cues[m_EditingCueIndex];
            s_Time    = c.timeSeconds;
            s_IsVideo = c.paramIsVideo;
            strncpy(s_ParamBuf, c.param.c_str(), sizeof(s_ParamBuf)-1);
            s_ParamBuf[sizeof(s_ParamBuf)-1] = '\0';
            for (int i = 0; i < kTypeCount; i++) if (kTypes[i] == c.type) s_TypeIdx = i;

            s_TextStandalone     = c.textStandalone;
            s_TransitionDuration = c.transitionDuration;
            s_TransitionIdx      = -1;
            if (!c.transitionName.empty()) {
                TransitionType t = TransitionTypeFromName(c.transitionName);
                for (int i = 0; i < kCueTransitionCount; i++)
                    if (kCueTransitions[i].type == t) { s_TransitionIdx = i; break; }
            }
        } else {
            s_Time = m_EditingMacro.cues.empty() ? 0.0f
                   : m_EditingMacro.cues.back().timeSeconds + 2.0f;
            s_ParamBuf[0] = '\0';
            s_IsVideo = false;
            s_TextStandalone     = false;
            s_TransitionIdx      = -1;
            s_TransitionDuration = -1.0f;
        }
    }

    ImGui::PushStyleColor(ImGuiCol_Text, LP::TextSub);
    ImGui::TextUnformatted(m_EditingCueIndex >= 0 ? "Editar cue" : "Nueva cue");
    ImGui::PopStyleColor();

    // FIX: el formato anterior ("%.1f s") metia la unidad DENTRO del texto
    // editable — al hacer click para escribir, ImGui muestra/edita ese
    // texto crudo ("4.0 s"), asi que tipear un numero nuevo peleaba contra
    // el sufijo no numerico y el valor no quedaba bien puesto. El formato
    // del campo ahora es numerico puro (se puede tipear y confirmar sin
    // lios); la unidad se muestra aparte, despues del campo, solo como
    // texto fijo.
    ImGui::PushStyleColor(ImGuiCol_Text, LP::TextSub);
    ImGui::TextUnformatted("Segundos:");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, LP::Text);
    ImGui::InputFloat("##cueTime", &s_Time, 0.5f, 1.0f, "%.1f");
    ImGui::PopStyleColor();
    if (s_Time < 0.0f) s_Time = 0.0f;
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, LP::TextMuted);
    ImGui::TextUnformatted("s");
    ImGui::PopStyleColor();

    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::BeginCombo("Accion", Core::MacroCueTypeLabel(kTypes[s_TypeIdx]))) {
        for (int i = 0; i < kTypeCount; i++) {
            bool sel = (i == s_TypeIdx);
            if (ImGui::Selectable(Core::MacroCueTypeLabel(kTypes[i]), sel)) s_TypeIdx = i;
        }
        ImGui::EndCombo();
    }

    Core::MacroCueType type = kTypes[s_TypeIdx];
    switch (type) {
        case Core::MacroCueType::ChangeBackground:
            // FIX: antes solo habia un campo de texto para tipear la ruta a
            // mano — faltaba el boton para elegir el archivo con un
            // selector, como en el resto de la app.
            ImGui::SetNextItemWidth(-90.0f);
            ImGui::InputTextWithHint("##cueParam", "Ruta de imagen o video...", s_ParamBuf, sizeof(s_ParamBuf));
            ImGui::SameLine();
            if (LPGhostBtn("Elegir...", {80, 0})) {
                std::string picked = PickImageOrVideoFile();
                if (!picked.empty()) {
                    strncpy(s_ParamBuf, picked.c_str(), sizeof(s_ParamBuf) - 1);
                    s_ParamBuf[sizeof(s_ParamBuf) - 1] = '\0';
                    s_IsVideo = LooksLikeVideoPath(picked);
                }
            }
            ImGui::Checkbox("Es un video", &s_IsVideo);
            break;
        case Core::MacroCueType::SetOverlay: {
            ImGui::SetNextItemWidth(-1.0f);
            std::string preview = s_ParamBuf[0] ? s_ParamBuf : "Elegi un overlay...";
            if (ImGui::BeginCombo("##cueOverlay", preview.c_str())) {
                for (const auto& ov : m_Overlays) {
                    bool sel = (ov.pngPath == s_ParamBuf);
                    if (ImGui::Selectable(ov.name.c_str(), sel)) {
                        strncpy(s_ParamBuf, ov.pngPath.c_str(), sizeof(s_ParamBuf)-1);
                        s_ParamBuf[sizeof(s_ParamBuf)-1] = '\0';
                    }
                }
                ImGui::EndCombo();
            }
            break;
        }
        case Core::MacroCueType::ShowText:
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextMultiline("##cueParam", s_ParamBuf, sizeof(s_ParamBuf), {-1.0f, 60.0f});
            ImGui::Checkbox("Mostrar sin fondo (pantalla negra detras del texto)", &s_TextStandalone);
            break;
        case Core::MacroCueType::ChangeClockStyle: {
            ImGui::SetNextItemWidth(-1.0f);
            std::string preview = s_ParamBuf[0] ? s_ParamBuf : "Elegi un estilo guardado...";
            if (ImGui::BeginCombo("##cueClockStyle", preview.c_str())) {
                for (const auto& styleName : Core::PresentationCore::Get().GetSavedStyleNames()) {
                    bool sel = (styleName == s_ParamBuf);
                    if (ImGui::Selectable(styleName.c_str(), sel)) {
                        strncpy(s_ParamBuf, styleName.c_str(), sizeof(s_ParamBuf)-1);
                        s_ParamBuf[sizeof(s_ParamBuf)-1] = '\0';
                    }
                }
                ImGui::EndCombo();
            }
            break;
        }
        case Core::MacroCueType::ClearOverlay:
        case Core::MacroCueType::ClearText:
            ImGui::PushStyleColor(ImGuiCol_Text, LP::TextMuted);
            ImGui::TextUnformatted("Esta accion no necesita parametros.");
            ImGui::PopStyleColor();
            break;
    }

    // ── Transicion de esta cue ────────────────────────────────────────────
    // Aplica a cualquier tipo de cue (no solo texto): pisa la transicion
    // global SOLO para este disparo puntual (ver TransitionPanel::Trigger /
    // PresentationCore::SetPendingTransitionOverride), sin tocar la
    // eleccion persistente del operador en el panel de Transiciones.
    ImGui::Dummy({0, 4});
    ImGui::PushStyleColor(ImGuiCol_Text, LP::TextSub);
    ImGui::TextUnformatted("Transicion:");
    ImGui::PopStyleColor();
    ImGui::SetNextItemWidth(220.0f);
    const char* transPreview = (s_TransitionIdx >= 0) ? kCueTransitions[s_TransitionIdx].label : "(usar la actual)";
    if (ImGui::BeginCombo("##cueTransition", transPreview)) {
        if (ImGui::Selectable("(usar la actual)", s_TransitionIdx < 0))
            s_TransitionIdx = -1;
        for (int i = 0; i < kCueTransitionCount; i++) {
            bool sel = (i == s_TransitionIdx);
            if (ImGui::Selectable(kCueTransitions[i].label, sel)) s_TransitionIdx = i;
        }
        ImGui::EndCombo();
    }
    if (s_TransitionIdx >= 0) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, LP::TextMuted);
        ImGui::TextUnformatted("Duracion:");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        ImGui::InputFloat("##cueTransDur", &s_TransitionDuration, 0.1f, 0.5f, "%.1f");
        if (s_TransitionDuration <= 0.0f && s_TransitionDuration != -1.0f) s_TransitionDuration = 0.1f;
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("-1 = usar la duracion configurada en Transiciones");
    }

    // ── Validacion: a prueba de fallos ──────────────────────────────────────
    // Las acciones que necesitan un parametro no se dejan guardar vacias
    // (evita cues "fantasma" que al reproducirse llaman a
    // SetBackgroundMedia("")/SetLayer2_Text("") sin avisar por que no pasa
    // nada quando se reproduce el macro).
    bool needsParam = (type == Core::MacroCueType::ChangeBackground ||
                        type == Core::MacroCueType::SetOverlay ||
                        type == Core::MacroCueType::ShowText ||
                        type == Core::MacroCueType::ChangeClockStyle);
    bool paramOk = !needsParam || s_ParamBuf[0] != '\0';

    if (!paramOk) {
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Red);
        ImGui::TextUnformatted("Esta accion necesita un valor arriba antes de guardar.");
        ImGui::PopStyleColor();
    }

    ImGui::Dummy({0, 6});
    bool isEditing = (m_EditingCueIndex >= 0 && m_EditingCueIndex < (int)m_EditingMacro.cues.size());

    ImGui::BeginDisabled(!paramOk);
    if (LPPrimaryBtn(isEditing ? "Guardar cambios" : "Agregar")) {
        Core::MacroCue cue;
        cue.timeSeconds        = s_Time;
        cue.type               = type;
        cue.param              = s_ParamBuf;
        cue.paramIsVideo       = s_IsVideo;
        cue.textStandalone     = (type == Core::MacroCueType::ShowText) && s_TextStandalone;
        cue.transitionName     = (s_TransitionIdx >= 0) ? TransitionTypeToName(kCueTransitions[s_TransitionIdx].type) : "";
        cue.transitionDuration = s_TransitionDuration;

        if (isEditing) m_EditingMacro.cues[m_EditingCueIndex] = cue;
        else            m_EditingMacro.cues.push_back(cue);

        Core::SortCues(m_EditingMacro);
        m_EditingCueIndex = -1;

        // Reset directo del formulario para la proxima cue: si ya estabamos
        // en modo "agregar" (indice -1 tanto antes como despues), el
        // detector de "cambio de indice" de arriba no se dispara solo, asi
        // que hay que limpiar los buffers estaticos aca mismo.
        s_Time    = m_EditingMacro.cues.empty() ? 0.0f : m_EditingMacro.cues.back().timeSeconds + 2.0f;
        s_ParamBuf[0] = '\0';
        s_IsVideo = false;
        s_TypeIdx = 0;
        s_TextStandalone     = false;
        s_TransitionIdx      = -1;
        s_TransitionDuration = -1.0f;
    }
    ImGui::EndDisabled();

    if (isEditing) {
        ImGui::SameLine();
        if (LPGhostBtn("Cancelar")) m_EditingCueIndex = -1;
    }
}

} // namespace ProyecThor::UI
