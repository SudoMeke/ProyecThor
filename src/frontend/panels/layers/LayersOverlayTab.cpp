#include "LayersOverlayTab.h"
#include "LayersTheme.h"
#include "backend/core/PresentationCore.h"
#include "backend/core/GitHubRelease.h"
#include "backend/settings/SettingsManager.h"
#include "OpenURL.h"
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

// ─────────────────────────────────────────────────────────────────────────────
//  "Abrir FoudreVue" — intenta lanzar el editor profesional de overlays (app
//  hermana, open source, ver /FoudreVue en el repo). Si no esta instalado,
//  en vez de un simple mensaje de texto, abre un modal que chequea la ultima
//  release publicada (por canal estable/beta) contra el repo de GitHub y
//  ofrece abrirla en el navegador (ver RenderFoudreVueDownloadModal).
// ─────────────────────────────────────────────────────────────────────────────
void LayersOverlayTab::OpenOrOfferFoudreVue() {
#ifdef _WIN32
    int found = std::system("where foudrevue >nul 2>nul");
    if (found == 0) {
        std::system("start \"\" foudrevue.exe");
        return;
    }
#else
    int found = std::system("command -v foudrevue > /dev/null 2>&1");
    if (found == 0) {
        std::system("foudrevue >/dev/null 2>&1 &");
        return;
    }
#endif
    m_ShowFoudreVueModal = true;
    if (m_FvStatus.load() == FvCheckStatus::Idle) StartFoudreVueCheck();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Chequeo de version — igual criterio que CategoryUpdates::DoCheckUpdate
//  (thread de fondo + estado atomico), pero contra TheVixcho/FoudreVue.
//  "beta" toma la release mas reciente (sea o no prerelease); "stable" pide
//  /releases/latest, que GitHub solo resuelve si hay al menos una release
//  no-prerelease (hoy no hay ninguna publicada todavia, asi que el estado
//  mas comun en la practica va a ser NoReleases, y eso se muestra tal cual).
// ─────────────────────────────────────────────────────────────────────────────
void LayersOverlayTab::StartFoudreVueCheck() {
    if (m_FvThread.joinable()) m_FvThread.join();
    m_FvStatus = FvCheckStatus::Checking;

    std::string channel = ProyecThor::Settings::SettingsManager::Get()
                               .GetSettings().foudrevue.releaseChannel;

    m_FvThread = std::thread([this, channel]() {
        std::string path = (channel == "beta")
            ? "/repos/TheVixcho/FoudreVue/releases?per_page=1"
            : "/repos/TheVixcho/FoudreVue/releases/latest";

        std::string body = ProyecThor::Core::FetchGitHubJson(path);
        if (body.empty() || body == "[]") {
            m_FvStatus = FvCheckStatus::NoReleases;
            return;
        }

        std::string tag = ProyecThor::Core::ExtractJsonField(body, "tag_name");
        if (tag.empty()) {
            std::string msg = ProyecThor::Core::ExtractJsonField(body, "message");
            m_FvStatus = msg.empty() ? FvCheckStatus::Error : FvCheckStatus::NoReleases;
            return;
        }

        m_FvVersion     = tag;
        m_FvHtmlUrl     = ProyecThor::Core::ExtractJsonField(body, "html_url");
        m_FvPublishedAt = ProyecThor::Core::ExtractJsonField(body, "published_at");
        m_FvStatus      = FvCheckStatus::Found;
    });
    m_FvThread.detach();
}

void LayersOverlayTab::RenderFoudreVueDownloadModal() {
    if (m_ShowFoudreVueModal)
        ImGui::OpenPopup("FoudreVue no encontrado");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(440, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22.0f, 20.0f));

    if (ImGui::BeginPopupModal("FoudreVue no encontrado", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Text);
        ImGui::TextWrapped(
            "FoudreVue es la app hermana, open source, donde se crean y "
            "editan los overlays (Pexels, rotacion, tipografia...). "
            "ProyecThor solo los consume ya renderizados.");
        ImGui::PopStyleColor();

        ImGui::Dummy({0, 8});
        ImGui::PushStyleColor(ImGuiCol_Text, LP::TextSub);
        ImGui::TextUnformatted("Canal:");
        ImGui::PopStyleColor();
        ImGui::SameLine();

        auto& fv = ProyecThor::Settings::SettingsManager::Get().GetSettings().foudrevue;
        bool isStable = (fv.releaseChannel != "beta");
        if (LPIconToggle("Estable", isStable, {70, 24})) {
            if (!isStable) {
                fv.releaseChannel = "stable";
                ProyecThor::Settings::SettingsManager::Get().SaveSettings();
                StartFoudreVueCheck();
            }
        }
        ImGui::SameLine();
        if (LPIconToggle("Beta", !isStable, {56, 24})) {
            if (isStable) {
                fv.releaseChannel = "beta";
                ProyecThor::Settings::SettingsManager::Get().SaveSettings();
                StartFoudreVueCheck();
            }
        }

        ImGui::Dummy({0, 10});

        switch (m_FvStatus.load()) {
            case FvCheckStatus::Checking:
                ImGui::PushStyleColor(ImGuiCol_Text, LP::TextSub);
                ImGui::TextUnformatted("Buscando la ultima version en GitHub...");
                ImGui::PopStyleColor();
                break;
            case FvCheckStatus::Found: {
                ImGui::PushStyleColor(ImGuiCol_Text, LP::Gold);
                ImGui::Text("Version %s disponible", m_FvVersion.c_str());
                ImGui::PopStyleColor();
                if (!m_FvPublishedAt.empty()) {
                    ImGui::PushStyleColor(ImGuiCol_Text, LP::TextMuted);
                    ImGui::Text("Publicada: %s", m_FvPublishedAt.c_str());
                    ImGui::PopStyleColor();
                }
                ImGui::Dummy({0, 6});
                if (LPPrimaryBtn("Descargar en GitHub") && !m_FvHtmlUrl.empty())
                    ProyecThor::External::OpenURL(m_FvHtmlUrl);
                break;
            }
            case FvCheckStatus::NoReleases:
                ImGui::PushStyleColor(ImGuiCol_Text, LP::TextSub);
                ImGui::TextWrapped(
                    "Todavia no hay versiones publicadas en este canal. Por "
                    "ahora se puede compilar desde la carpeta FoudreVue/ del "
                    "repositorio.");
                ImGui::PopStyleColor();
                ImGui::Dummy({0, 6});
                if (LPGhostBtn("Ver repositorio en GitHub"))
                    ProyecThor::External::OpenURL("https://github.com/TheVixcho/FoudreVue");
                break;
            case FvCheckStatus::Error:
                ImGui::PushStyleColor(ImGuiCol_Text, LP::Red);
                ImGui::TextUnformatted("No se pudo conectar a GitHub para chequear la version.");
                ImGui::PopStyleColor();
                break;
            case FvCheckStatus::Idle:
                break;
        }

        ImGui::Dummy({0, 12});
        if (LPGhostBtn("Cerrar")) {
            m_ShowFoudreVueModal = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();
}

// ─────────────────────────────────────────────────────────────────────────────
//  "Importar overlay..." — lee un paquete .foudrevue exportado (carpeta con
//  render.png + recipe.json). ProyecThor solo consume el PNG ya rasterizado;
//  no necesita parsear el recipe.json (eso es responsabilidad de FoudreVue,
//  que es donde se re-edita un overlay).
// ─────────────────────────────────────────────────────────────────────────────
void LayersOverlayTab::ImportBundle() {
#ifdef _WIN32
    SetStatus("Importar overlays todavia no esta disponible en Windows.");
#else
    std::string cmd = "zenity --file-selection --directory "
                       "--title=\"Elegi la carpeta .foudrevue a importar\" 2>/dev/null";
    std::string bundleDir;
    {
        char buffer[1024];
        FILE* pipe = popen(cmd.c_str(), "r");
        if (pipe) {
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) bundleDir += buffer;
            pclose(pipe);
        }
    }
    while (!bundleDir.empty() && (bundleDir.back() == '\n' || bundleDir.back() == '\r'))
        bundleDir.pop_back();
    if (bundleDir.empty()) return;

    fs::path png = fs::path(bundleDir) / "render.png";
    std::error_code ec;
    if (!fs::exists(png, ec)) {
        SetStatus("Esa carpeta no tiene un render.png — no parece un overlay exportado valido.");
        return;
    }

    std::string name = fs::path(bundleDir).stem().string();
    if (name.empty()) name = "overlay_importado";

    fs::copy_file(png, ResolvePngPath(name), fs::copy_options::overwrite_existing, ec);
    if (ec) {
        SetStatus("No se pudo copiar el overlay importado.");
        return;
    }

    m_ThumbnailCache.erase(ResolvePngPath(name));
    ReloadList();
    SetStatus("Overlay \"" + name + "\" importado.");
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
//  Toolbar superior — compacta, solo iconos
// ─────────────────────────────────────────────────────────────────────────────
void LayersOverlayTab::RenderTopBar() {
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, LP::TextSub);
    ImGui::TextUnformatted("Overlays");
    ImGui::PopStyleColor();

    const float btnSz = 26.0f;
    const float zoomW = 76.0f;
    const float gap   = 4.0f;
    const float rowW  = zoomW + gap + btnSz * 5 + gap * 5;
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
    ImGui::SameLine(0, gap);
    if (LPCornerIconBtn("##ovimport", LPDrawFolderPlus, "Importar overlay (.foudrevue)...", {btnSz,btnSz}))
        ImportBundle();
    ImGui::SameLine(0, gap);
    if (LPCornerIconBtn("##ovfoudrevue", LPDrawPlus, "Abrir FoudreVue (crear/editar overlays)", {btnSz,btnSz}, true))
        OpenOrOfferFoudreVue();

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
//  Galeria
// ─────────────────────────────────────────────────────────────────────────────
void LayersOverlayTab::RenderGallery() {
    if (m_Overlays.empty()) {
        ImGui::Dummy({0,16});
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w  = ImGui::GetContentRegionAvail().x;
        ImGui::GetWindowDrawList()->AddRectFilled(p, {p.x+w,p.y+64}, LPU32(LP::Surface1), 10.0f);
        ImGui::Dummy({0,12});
        ImGui::PushStyleColor(ImGuiCol_Text, LP::TextMuted);
        const char* msg = "Sin overlays aun. Creá uno en FoudreVue (se leen automaticamente si esta instalado) o importá un .foudrevue exportado.";
        float tw = ImGui::CalcTextSize(msg).x;
        ImGui::SetCursorPosX(std::max(0.0f, (w-tw)*0.5f));
        ImGui::Text("%s", msg);
        ImGui::PopStyleColor();
        return;
    }

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
        for (size_t i = 0; i < m_Overlays.size(); ++i) {
            RenderCard(m_Overlays[i], cW, cH, currentCol, cols);
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

    RenderGallery();
    RenderFoudreVueDownloadModal();
}

} // namespace ProyecThor::UI
