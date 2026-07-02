#include "LayersBgTab.h"
#include "LayersTheme.h"
#include "backend/core/PresentationCore.h"
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <windows.h>
#include <string>
#include <fstream>
#include <GL/gl.h>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include "stb_image.h"
#include <shobjidl.h>
#include <shlobj.h>

namespace fs = std::filesystem;
namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  Rutas
// ─────────────────────────────────────────────────────────────────────────────
static fs::path GetAppDataDir() {
    wchar_t buf[MAX_PATH] = {};
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf);
    fs::path dir = fs::path(buf) / "ProyecThor";
    std::error_code ec;
    fs::create_directories(dir / "themes", ec);
    return dir;
}
static fs::path BgRootDir() {
    fs::path dir = GetAppDataDir() / "assets" / "backgrounds";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers locales
// ─────────────────────────────────────────────────────────────────────────────
static std::wstring ToWide(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring r(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &r[0], n);
    return r;
}
static bool IsMedia(const std::string& ext) {
    return ext==".mp4"||ext==".mkv"||ext==".avi"||ext==".mov"
          ||ext==".jpg"||ext==".jpeg"||ext==".png";
}
static bool IsImage(const std::string& ext) {
    return ext==".jpg"||ext==".jpeg"||ext==".png";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Thumbnails
// ─────────────────────────────────────────────────────────────────────────────
static ImTextureID LoadVideoThumb(const std::string& path) {
    CoInitialize(nullptr);
    std::wstring wp = ToWide(path);
    IShellItemImageFactory* f = nullptr;
    if (FAILED(SHCreateItemFromParsingName(wp.c_str(), nullptr, IID_PPV_ARGS(&f)))) return 0;
    SIZE sz = {256, 256};
    HBITMAP hbm = nullptr;
    f->GetImage(sz, SIIGBF_RESIZETOFIT, &hbm);
    f->Release();
    if (!hbm) return 0;

    BITMAP bm; GetObject(hbm, sizeof(bm), &bm);
    HDC hdc = GetDC(nullptr);
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bm.bmWidth;
    bmi.bmiHeader.biHeight = -bm.bmHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    std::vector<unsigned char> px(bm.bmWidth * bm.bmHeight * 4);
    GetDIBits(hdc, hbm, 0, bm.bmHeight, px.data(), &bmi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, hdc);
    DeleteObject(hbm);
    for (size_t i = 0; i < px.size(); i += 4) { std::swap(px[i], px[i+2]); px[i+3]=255; }

    GLuint tex; glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bm.bmWidth, bm.bmHeight,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    return (ImTextureID)(intptr_t)tex;
}
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

ImTextureID LayersBgTab::GetThumbnail(const std::string& path, bool isVideo) {
    auto it = m_ThumbnailCache.find(path);
    if (it != m_ThumbnailCache.end()) return it->second;
    std::string abs = fs::absolute(fs::path(path)).string();
    ImTextureID t = isVideo ? LoadVideoThumb(abs) : LoadImageThumb(abs.c_str());
    m_ThumbnailCache[path] = t;
    return t;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor / ReloadList
// ─────────────────────────────────────────────────────────────────────────────
LayersBgTab::LayersBgTab() {
    ReloadList();
}

void LayersBgTab::ReloadList() {
    m_AllBackgrounds.clear();
    m_BgFolders.clear();
    fs::path root = BgRootDir();
    try {
        for (const auto& e : fs::directory_iterator(root)) {
            if (e.is_directory()) {
                m_BgFolders.push_back(e.path().filename().string());
                for (const auto& sub : fs::directory_iterator(e.path())) {
                    if (!sub.is_regular_file()) continue;
                    std::string ext = sub.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (!IsMedia(ext)) continue;
                    BgEntry bg;
                    bg.fullPath = sub.path().string();
                    bg.name     = sub.path().stem().string();
                    bg.ext      = ext;
                    bg.folder   = e.path().filename().string();
                    bg.isImage  = IsImage(ext);
                    m_AllBackgrounds.push_back(std::move(bg));
                }
            } else if (e.is_regular_file()) {
                std::string ext = e.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (!IsMedia(ext)) continue;
                BgEntry bg;
                bg.fullPath = e.path().string();
                bg.name     = e.path().stem().string();
                bg.ext      = ext;
                bg.folder   = "";
                bg.isImage  = IsImage(ext);
                m_AllBackgrounds.push_back(std::move(bg));
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "[LayersBgTab] " << ex.what() << "\n";
    }
    std::sort(m_BgFolders.begin(), m_BgFolders.end());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Operaciones de disco
// ─────────────────────────────────────────────────────────────────────────────
bool LayersBgTab::ImportBackground() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dlg))))
        return false;
    COMDLG_FILTERSPEC fs[] = {
        {L"Video e Imagen", L"*.mp4;*.mkv;*.avi;*.mov;*.jpg;*.jpeg;*.png"},
        {L"Videos",         L"*.mp4;*.mkv;*.avi;*.mov"},
        {L"Imagenes",       L"*.jpg;*.jpeg;*.png"}
    };
    dlg->SetFileTypes(3, fs); dlg->SetFileTypeIndex(1); dlg->SetTitle(L"Importar Fondo");
    FILEOPENDIALOGOPTIONS o = 0; dlg->GetOptions(&o);
    dlg->SetOptions(o | FOS_ALLOWMULTISELECT | FOS_FILEMUSTEXIST);

    bool imported = false;
    if (SUCCEEDED(dlg->Show(nullptr))) {
        IShellItemArray* items = nullptr;
        if (SUCCEEDED(dlg->GetResults(&items))) {
            DWORD count = 0; items->GetCount(&count);
            ::fs::path dest = BgRootDir();
            if (!m_CurrentBgFolder.empty()) dest = dest / m_CurrentBgFolder;
            std::error_code ec; ::fs::create_directories(dest, ec);
            for (DWORD i = 0; i < count; i++) {
                IShellItem* item = nullptr;
                if (SUCCEEDED(items->GetItemAt(i, &item))) {
                    PWSTR pp = nullptr;
                    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &pp))) {
                        ::fs::path src = pp;
                        ::fs::path dst = dest / src.filename();
                        ::fs::copy_file(src, dst, ::fs::copy_options::overwrite_existing, ec);
                        if (!ec) imported = true;
                        CoTaskMemFree(pp);
                    }
                    item->Release();
                }
            }
            items->Release();
        }
    }
    dlg->Release();
    return imported;
}

bool LayersBgTab::CreateBgFolder(const std::string& name) {
    if (name.empty()) return false;
    std::error_code ec;
    fs::create_directories(BgRootDir() / name, ec);
    return !ec;
}
bool LayersBgTab::RenameBgFile(const std::string& oldPath, const std::string& newName) {
    if (newName.empty()) return false;
    fs::path src = oldPath;
    fs::path dst = src.parent_path() / (newName + src.extension().string());
    std::error_code ec; fs::rename(src, dst, ec); return !ec;
}
bool LayersBgTab::RenameBgFolder(const std::string& oldName, const std::string& newName) {
    if (newName.empty() || oldName == newName) return false;
    std::error_code ec; fs::rename(BgRootDir()/oldName, BgRootDir()/newName, ec); return !ec;
}
bool LayersBgTab::DeleteBgFile(const std::string& fullPath) {
    std::error_code ec; fs::remove(fs::path(fullPath), ec); return !ec;
}
bool LayersBgTab::DeleteBgFolder(const std::string& folderName) {
    std::error_code ec; fs::remove_all(BgRootDir()/folderName, ec); return !ec;
}
bool LayersBgTab::MoveBgToFolder(const std::string& srcFull, const std::string& destFolder) {
    fs::path src  = srcFull;
    fs::path dest = BgRootDir();
    if (!destFolder.empty()) dest = dest / destFolder;
    dest = dest / src.filename();
    if (src == dest) return false;
    std::error_code ec; fs::rename(src, dest, ec);
    if (ec) { fs::copy_file(src, dest, fs::copy_options::overwrite_existing, ec); if (!ec) fs::remove(src, ec); }
    return !ec;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers de render compartidos
// ─────────────────────────────────────────────────────────────────────────────
void LayersBgTab::RenderViewToggleBar(bool& gridMode) {
    float avail = ImGui::GetContentRegionAvail().x;
    ImGui::SameLine(avail - 56.0f);
    ImGui::PushID("viewtoggle");

    if (LPIconToggle("##grid", gridMode)) gridMode = true;
    DrawGridIcon(gridMode);

    ImGui::SameLine(0, 4);
    if (LPIconToggle("##list", !gridMode)) gridMode = false;
    DrawListIcon(!gridMode);

    ImGui::PopID();
}

void LayersBgTab::BgContextMenu(const BgEntry& entry) {
    ImGui::PushStyleColor(ImGuiCol_Text, LP::Accent);
    std::string disp = entry.name.length() > 22 ? entry.name.substr(0,19)+"..." : entry.name;
    ImGui::Text("%s", disp.c_str());
    ImGui::PopStyleColor();
    ImGui::Separator();

    if (ImGui::Selectable("  Renombrar")) {
        m_RenamingBg    = true;
        m_RenameOldPath = entry.fullPath;
        size_t len = std::min(entry.name.size(), sizeof(m_RenameBuf)-1);
        memcpy(m_RenameBuf, entry.name.c_str(), len); m_RenameBuf[len] = '\0';
    }
    if (!m_BgFolders.empty() && ImGui::BeginMenu("  Mover a carpeta")) {
        if (!entry.folder.empty() && ImGui::MenuItem("  Raiz")) {
            if (MoveBgToFolder(entry.fullPath, "")) ReloadList();
        }
        for (const auto& fn : m_BgFolders) {
            if (fn == entry.folder) continue;
            if (ImGui::MenuItem(fn.c_str()))
                if (MoveBgToFolder(entry.fullPath, fn)) ReloadList();
        }
        ImGui::EndMenu();
    }
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, LP::Red);
    if (ImGui::Selectable("  Eliminar"))
        if (DeleteBgFile(entry.fullPath)) ReloadList();
    ImGui::PopStyleColor();
}

void LayersBgTab::FolderContextMenu(const std::string& folderName) {
    ImGui::PushStyleColor(ImGuiCol_Text, LP::Gold);
    std::string disp = folderName.length()>22 ? folderName.substr(0,19)+"..." : folderName;
    ImGui::Text("%s", disp.c_str());
    ImGui::PopStyleColor();
    ImGui::Separator();
    if (ImGui::Selectable("  Renombrar")) {
        m_RenamingFolder  = true;
        m_RenameFolderOld = folderName;
        size_t len = std::min(folderName.size(), sizeof(m_RenameFolderBuf)-1);
        memcpy(m_RenameFolderBuf, folderName.c_str(), len); m_RenameFolderBuf[len]='\0';
    }
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, LP::Red);
    if (ImGui::Selectable("  Eliminar (con contenido)")) {
        if (DeleteBgFolder(folderName)) {
            if (m_CurrentBgFolder == folderName) m_CurrentBgFolder.clear();
            ReloadList();
        }
    }
    ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Modales
// ─────────────────────────────────────────────────────────────────────────────
void LayersBgTab::RenderCreateFolderModal() {
    if (m_CreatingFolder) {
        ImGui::OpenPopup("##NewFolderPop");
        m_CreatingFolder = false;
        memset(m_NewFolderBuf, 0, sizeof(m_NewFolderBuf));
    }
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.10f,0.11f,0.14f,1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));
    if (ImGui::BeginPopup("##NewFolderPop")) {
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Gold);
        ImGui::Text("Nueva carpeta");
        ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_FrameBg,        LP::Surface2);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, LP::Surface3);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::SetNextItemWidth(200.0f);
        bool confirm = ImGui::InputText("##nf", m_NewFolderBuf, sizeof(m_NewFolderBuf),
                                        ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopStyleVar(); ImGui::PopStyleColor(2);
        ImGui::SetItemDefaultFocus();
        ImGui::Spacing();
        if (LPPrimaryBtn("Crear") || confirm) {
            if (strlen(m_NewFolderBuf)>0 && CreateBgFolder(m_NewFolderBuf)) ReloadList();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0,6);
        if (LPGhostBtn("Cancelar")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(); ImGui::PopStyleColor();
}

void LayersBgTab::RenderRenameBgModal() {
    if (m_RenamingBg) { ImGui::OpenPopup("##RenameBgPop"); m_RenamingBg = false; }
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.10f,0.11f,0.14f,1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16,14));
    if (ImGui::BeginPopup("##RenameBgPop")) {
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Accent);
        ImGui::Text("Renombrar archivo");
        ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_FrameBg,        LP::Surface2);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, LP::Surface3);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::SetNextItemWidth(240.0f);
        bool confirm = ImGui::InputText("##rb", m_RenameBuf, sizeof(m_RenameBuf),
                                        ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopStyleVar(); ImGui::PopStyleColor(2);
        ImGui::SetItemDefaultFocus(); ImGui::Spacing();
        if (LPPrimaryBtn("Renombrar") || confirm) {
            if (strlen(m_RenameBuf)>0 && RenameBgFile(m_RenameOldPath, m_RenameBuf)) {
                m_ThumbnailCache.erase(m_RenameOldPath);
                ReloadList();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0,6);
        if (LPGhostBtn("Cancelar")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(); ImGui::PopStyleColor();
}

void LayersBgTab::RenderRenameFolderModal() {
    if (m_RenamingFolder) { ImGui::OpenPopup("##RenameFolderPop"); m_RenamingFolder = false; }
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.10f,0.11f,0.14f,1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16,14));
    if (ImGui::BeginPopup("##RenameFolderPop")) {
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Gold);
        ImGui::Text("Renombrar carpeta");
        ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_FrameBg,        LP::Surface2);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, LP::Surface3);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::SetNextItemWidth(200.0f);
        bool confirm = ImGui::InputText("##rfn", m_RenameFolderBuf, sizeof(m_RenameFolderBuf),
                                        ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopStyleVar(); ImGui::PopStyleColor(2);
        ImGui::SetItemDefaultFocus(); ImGui::Spacing();
        if (LPPrimaryBtn("Renombrar") || confirm) {
            if (strlen(m_RenameFolderBuf)>0) {
                std::string nn(m_RenameFolderBuf);
                if (RenameBgFolder(m_RenameFolderOld, nn)) {
                    if (m_CurrentBgFolder == m_RenameFolderOld) m_CurrentBgFolder = nn;
                    ReloadList();
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0,6);
        if (LPGhostBtn("Cancelar")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(); ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Toolbar
// ─────────────────────────────────────────────────────────────────────────────
void LayersBgTab::RenderToolbar() {
    // Espacio pequeño al inicio (separación superior)
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    float avail   = ImGui::GetContentRegionAvail().x;
    float btnH    = 30.0f;
    float folderW = 110.0f;
    float toggleW = 56.0f;
    float importW = 110.0f;
    float gap     = 6.0f;

    // --- FILA 1: Botones principales ---
    float row1Width = importW + gap + folderW;
    float startX1 = (avail - row1Width) * 0.5f;
    
    if (startX1 > 0.0f) ImGui::SetCursorPosX(startX1);

    if (LPPrimaryBtn("  + Importar  ", {importW, btnH})) {
        if (ImportBackground()) ReloadList();
    }

    ImGui::SameLine(0, gap);
    if (LPGhostBtn("+ Carpeta", {folderW, btnH})) {
        m_CreatingFolder = true;
    }

    ImGui::Dummy(ImVec2(0.0f, 10.0f)); 
    // Calculamos el centro solo para el ToggleBar
    float startX2 = (avail - toggleW) * 0.5f;
    if (startX2 > 0.0f) ImGui::SetCursorPosX(startX2);

    RenderViewToggleBar(m_GridMode);

}
// ─────────────────────────────────────────────────────────────────────────────
//  Breadcrumb
// ─────────────────────────────────────────────────────────────────────────────
void LayersBgTab::RenderBreadcrumb() {
    if (m_CurrentBgFolder.empty()) return;

    ImGui::Spacing();
    
    ImGui::PushStyleColor(ImGuiCol_Button,        {0,0,0,0});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, LP::AccentDim);
    ImGui::PushStyleColor(ImGuiCol_Text,          LP::Accent);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,   {2, 1});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    if (ImGui::Button("Fondos")) m_CurrentBgFolder.clear();
    ImGui::PopStyleVar(2); ImGui::PopStyleColor(3);

    ImGui::SameLine(0,4);
    ImGui::PushStyleColor(ImGuiCol_Text, LP::TextMuted);
    ImGui::Text("›");
    ImGui::PopStyleColor();
    ImGui::SameLine(0,4);
    ImGui::PushStyleColor(ImGuiCol_Text, LP::Gold);
    ImGui::Text("%s", m_CurrentBgFolder.c_str());
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tarjeta de carpeta (grid)
// ─────────────────────────────────────────────────────────────────────────────
void LayersBgTab::RenderFolderCard(const std::string& name, float W, float H, int col, int cols) {
    ImGui::PushID(("fc_"+name).c_str());

    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool hov   = ImGui::IsMouseHoveringRect(pos, {pos.x+W, pos.y+H});
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Fondo
    ImU32 bg = hov ? LPU32({0.22f,0.20f,0.09f,1.0f}) : LPU32({0.15f,0.13f,0.05f,1.0f});
    dl->AddRectFilled(pos, {pos.x+W, pos.y+H}, bg, 10.0f);
    dl->AddRect(pos, {pos.x+W, pos.y+H},
        hov ? LPU32(LP::Gold) : LPU32({0.55f,0.42f,0.10f,0.45f}),
        10.0f, 0, hov ? 1.5f : 1.0f);

    // Icono carpeta
    float fx = pos.x + W*0.5f - 18.0f, fy = pos.y + 14.0f;
    ImU32 gc = LPU32({0.92f,0.72f,0.18f,0.88f});
    dl->AddRectFilled({fx, fy+5.0f}, {fx+36.0f, fy+28.0f}, gc, 3.0f);
    dl->AddRectFilled({fx, fy+10.0f},{fx+16.0f, fy+5.0f},  gc, 2.0f);

    // Contador
    int cnt = 0;
    for (const auto& bg : m_AllBackgrounds) if (bg.folder==name) cnt++;
    std::string cs = std::to_string(cnt) + (cnt==1?" arch.":" archs.");
    ImVec2 csSz = ImGui::CalcTextSize(cs.c_str());
    dl->AddText({pos.x+(W-csSz.x)*0.5f, pos.y+H-34.0f}, LPU32(LP::TextMuted), cs.c_str());

    // Nombre
    dl->AddRectFilled({pos.x,pos.y+H-22.0f},{pos.x+W,pos.y+H},
        LPU32({0,0,0,0.55f}), 10.0f, ImDrawFlags_RoundCornersBottom);
    std::string dn = name.length()>17 ? name.substr(0,14)+"..." : name;
    ImVec2 ns = ImGui::CalcTextSize(dn.c_str());
    dl->AddText({pos.x+(W-ns.x)*0.5f, pos.y+H-19.0f}, LPU32(LP::Gold), dn.c_str());

    ImGui::InvisibleButton(("##fcard_"+name).c_str(), {W, H});

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("BG_FILE")) {
            std::string src(static_cast<const char*>(p->Data), p->DataSize-1);
            if (MoveBgToFolder(src, name)) ReloadList();
        }
        ImGui::EndDragDropTarget();
        dl->AddRect(pos, {pos.x+W,pos.y+H}, LPU32(LP::Gold), 10.0f, 0, 2.5f);
    }

    // ── Click: entrar en carpeta — proteger contra autoclick ─────────────
    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)
        && !m_JustEnteredFolder) {
        m_CurrentBgFolder  = name;
        m_JustEnteredFolder = true;
    }

    if (ImGui::BeginPopupContextItem(("FCtx_"+name).c_str())) {
        FolderContextMenu(name); ImGui::EndPopup();
    }

    if (col < cols-1) ImGui::SameLine();
    ImGui::PopID();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Fila de carpeta (lista)
// ─────────────────────────────────────────────────────────────────────────────
void LayersBgTab::RenderFolderRow(const std::string& name, float W, float rowH) {
    ImGui::PushID(("fr_"+name).c_str());
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool hov   = ImGui::IsMouseHoveringRect(pos, {pos.x+W, pos.y+rowH});
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImU32 bg = hov ? LPU32({0.22f,0.19f,0.08f,1.0f}) : LPU32({0.13f,0.12f,0.06f,1.0f});
    dl->AddRectFilled(pos, {pos.x+W, pos.y+rowH}, bg, 8.0f);
    dl->AddRectFilled(pos, {pos.x+3.0f, pos.y+rowH},
        LPU32({0.92f,0.72f,0.18f, hov?1.0f:0.5f}), 2.0f);

    float fx=pos.x+10.0f, fy=pos.y+(rowH-16.0f)*0.5f;
    ImU32 gc = LPU32({0.92f,0.72f,0.18f,0.88f});
    dl->AddRectFilled({fx,fy+3.0f},{fx+22.0f,fy+16.0f},gc,2.0f);
    dl->AddRectFilled({fx,fy+6.0f},{fx+10.0f,fy+3.0f}, gc,1.5f);

    std::string dn = name.length()>30 ? name.substr(0,27)+"..." : name;
    dl->AddText({pos.x+38.0f, pos.y+(rowH-ImGui::GetTextLineHeight())*0.5f},
        LPU32(LP::Gold), dn.c_str());

    int cnt = 0; for (const auto& b : m_AllBackgrounds) if (b.folder==name) cnt++;
    std::string cs = std::to_string(cnt)+" arch.";
    ImVec2 csSz = ImGui::CalcTextSize(cs.c_str());
    dl->AddText({pos.x+W-csSz.x-10.0f, pos.y+(rowH-csSz.y)*0.5f},
        LPU32(LP::TextMuted), cs.c_str());

    ImGui::InvisibleButton(("##frow_"+name).c_str(), {W, rowH});

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("BG_FILE")) {
            std::string src(static_cast<const char*>(p->Data), p->DataSize-1);
            if (MoveBgToFolder(src, name)) ReloadList();
        }
        ImGui::EndDragDropTarget();
        dl->AddRect(pos, {pos.x+W,pos.y+rowH}, LPU32(LP::Gold), 8.0f, 0, 2.0f);
    }

    // ── Click: entrar en carpeta — proteger contra autoclick ─────────────
    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)
        && !m_JustEnteredFolder) {
        m_CurrentBgFolder  = name;
        m_JustEnteredFolder = true;
    }

    if (ImGui::BeginPopupContextItem(("FRowCtx_"+name).c_str())) {
        FolderContextMenu(name); ImGui::EndPopup();
    }

    ImGui::Dummy({0, 4.0f});
    ImGui::PopID();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tarjeta de background (grid)
// ─────────────────────────────────────────────────────────────────────────────
void LayersBgTab::RenderBgCard(const BgEntry& e, float W, float H, int col, int cols) {
    std::string id = "##bgc_"+e.fullPath;
    ImGui::PushID(id.c_str());

    ImTextureID thumb = GetThumbnail(e.fullPath, !e.isImage);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool hov   = ImGui::IsMouseHoveringRect(pos, {pos.x+W, pos.y+H});
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Fondo + thumbnail
    dl->AddRectFilled(pos, {pos.x+W,pos.y+H},
        hov ? LPU32(LP::Surface2) : LPU32(LP::Surface1), 10.0f);
    if (thumb)
        dl->AddImageRounded(thumb, pos, {pos.x+W,pos.y+H},
                            {0,0},{1,1}, IM_COL32_WHITE, 10.0f);

    // Borde
    dl->AddRect(pos, {pos.x+W,pos.y+H},
        hov ? LPU32({LP::Accent.x,LP::Accent.y,LP::Accent.z,0.60f}) : LPU32(LP::Border),
        10.0f, 0, hov?1.8f:1.0f);

    // Chip tipo
    {
        const char* lbl = e.isImage ? "IMG" : "VID";
        ImVec4 chipBg   = e.isImage ? LP::GreenDim : LP::AccentDim;
        ImVec4 chipFg   = e.isImage ? LP::Green    : LP::Accent;
        float bx=pos.x+7.0f, by=pos.y+7.0f;
        ImVec2 ts = ImGui::CalcTextSize(lbl);
        dl->AddRectFilled({bx,by},{bx+ts.x+8.0f,by+ts.y+4.0f},LPU32(chipBg),4.0f);
        dl->AddText({bx+4.0f,by+2.0f}, LPU32(chipFg), lbl);
    }

    // Nombre
    std::string dn = e.name.length()>18 ? e.name.substr(0,15)+"..." : e.name;
    dl->AddRectFilled({pos.x,pos.y+H-26.0f},{pos.x+W,pos.y+H},
        LPU32({0,0,0,0.78f}), 10.0f, ImDrawFlags_RoundCornersBottom);
    ImVec2 ns = ImGui::CalcTextSize(dn.c_str());
    dl->AddText({pos.x+(W-ns.x)*0.5f, pos.y+H-21.0f}, LPU32(LP::Text), dn.c_str());

    // Overlay de hover: play/imagen
    if (hov) {
        const char* icon = e.isImage ? "[ IMG ]" : "[ PLAY ]";
        ImVec2 is = ImGui::CalcTextSize(icon);
        dl->AddRectFilled({pos.x,pos.y},{pos.x+W,pos.y+H-26.0f},
            LPU32({0,0,0,0.35f}), 10.0f, ImDrawFlags_RoundCornersTop);
        dl->AddText({pos.x+(W-is.x)*0.5f, pos.y+(H-26.0f-is.y)*0.5f},
            LPU32(LP::Text), icon);
    }

    ImGui::InvisibleButton(id.c_str(), {W, H});

    // Drag source
    bool wasDragged = false;
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        wasDragged = true;
        ImGui::SetDragDropPayload("BG_FILE", e.fullPath.c_str(), e.fullPath.size()+1);
        ImGui::PushStyleColor(ImGuiCol_Text, LP::TextSub);
        ImGui::Text("Mover: %s", dn.c_str());
        ImGui::PopStyleColor();
        ImGui::EndDragDropSource();
    }

    // Click limpio → proyectar (nunca si acabamos de entrar en carpeta)
    if (!wasDragged && !m_JustEnteredFolder
        && ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        Core::PresentationCore::Get().SetBackgroundMedia(e.fullPath, !e.isImage);

    if (ImGui::BeginPopupContextItem(("BgCtx_"+e.fullPath).c_str())) {
        BgContextMenu(e); ImGui::EndPopup();
    }

    if (col < cols-1) ImGui::SameLine();
    ImGui::PopID();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Fila de background (lista)
// ─────────────────────────────────────────────────────────────────────────────
void LayersBgTab::RenderBgRow(const BgEntry& e, float W, float rowH) {
    const float thumbSz = 38.0f;
    ImGui::PushID(("bgr_"+e.fullPath).c_str());

    ImTextureID thumb = GetThumbnail(e.fullPath, !e.isImage);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool hov   = ImGui::IsMouseHoveringRect(pos, {pos.x+W, pos.y+rowH});
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(pos, {pos.x+W,pos.y+rowH},
        hov ? LPU32(LP::Surface2) : LPU32(LP::Surface1), 8.0f);

    // Accent bar a la izquierda al hacer hover
    if (hov)
        dl->AddRectFilled(pos,{pos.x+3.0f,pos.y+rowH}, LPU32(LP::Accent), 2.0f);

    float tx=pos.x+8.0f, ty=pos.y+(rowH-thumbSz)*0.5f;
    if (thumb)
        dl->AddImageRounded(thumb,{tx,ty},{tx+thumbSz,ty+thumbSz},{0,0},{1,1},IM_COL32_WHITE,5.0f);
    else {
        dl->AddRectFilled({tx,ty},{tx+thumbSz,ty+thumbSz},LPU32(LP::Surface0),5.0f);
        const char* ic = e.isImage?"IMG":"VID";
        ImVec4 ic4 = e.isImage ? LP::Green : LP::Accent;
        ImVec2 is = ImGui::CalcTextSize(ic);
        dl->AddText({tx+(thumbSz-is.x)*0.5f,ty+(thumbSz-is.y)*0.5f},LPU32(ic4),ic);
    }

    std::string dn = e.name.length()>32 ? e.name.substr(0,29)+"..." : e.name;
    float txtX=tx+thumbSz+10.0f, txtY=pos.y+(rowH-ImGui::GetTextLineHeight())*0.5f;
    dl->AddText({txtX,txtY}, LPU32(LP::Text), dn.c_str());

    // Tag derecha
    const char* tag = e.isImage?"IMG":"VID";
    ImVec4 tagBg  = e.isImage ? LP::GreenDim : LP::AccentDim;
    ImVec4 tagFg  = e.isImage ? LP::Green    : LP::Accent;
    ImVec2 tagSz  = ImGui::CalcTextSize(tag);
    float  tagX   = pos.x+W-tagSz.x-16.0f;
    float  tagY   = pos.y+(rowH-tagSz.y)*0.5f;
    LPBadge(dl, {tagX,tagY}, tag, tagBg, tagFg);

    ImGui::InvisibleButton(("##bgrow_"+e.fullPath).c_str(), {W, rowH});

    bool wasDragged = false;
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        wasDragged = true;
        ImGui::SetDragDropPayload("BG_FILE", e.fullPath.c_str(), e.fullPath.size()+1);
        ImGui::PushStyleColor(ImGuiCol_Text, LP::TextSub);
        ImGui::Text("Mover: %s", dn.c_str());
        ImGui::PopStyleColor();
        ImGui::EndDragDropSource();
    }

    if (!wasDragged && !m_JustEnteredFolder
        && ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        Core::PresentationCore::Get().SetBackgroundMedia(e.fullPath, !e.isImage);

    if (ImGui::BeginPopupContextItem(("BgRowCtx_"+e.fullPath).c_str())) {
        BgContextMenu(e); ImGui::EndPopup();
    }

    ImGui::Dummy({0, 5.0f});
    ImGui::PopID();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Vistas
// ─────────────────────────────────────────────────────────────────────────────
void LayersBgTab::RenderFolderView() {
    std::vector<const BgEntry*> roots;
    for (const auto& bg : m_AllBackgrounds) if (bg.folder.empty()) roots.push_back(&bg);

    if (m_BgFolders.empty() && roots.empty()) {
        ImGui::Dummy({0,16});
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w  = ImGui::GetContentRegionAvail().x;
        ImGui::GetWindowDrawList()->AddRectFilled(p,{p.x+w,p.y+70},LPU32(LP::Surface1),12.0f);
        ImGui::Dummy({0,14});
        ImGui::PushStyleColor(ImGuiCol_Text, LP::TextMuted);
        const char* h = "Importa fondos con el boton de arriba";
        float tw = ImGui::CalcTextSize(h).x;
        ImGui::SetCursorPosX((w-tw)*0.5f);
        ImGui::Text("%s", h);
        ImGui::PopStyleColor();
        return;
    }

    if (m_GridMode) {
        const float cW = 150.0f;
        const float cH = 92.0f;
        const float minGap = 10.0f;

        // 1. Calcular espacio disponible y columnas
        float availWidth = ImGui::GetContentRegionAvail().x;
        int cols = std::max(1, static_cast<int>((availWidth + minGap) / (cW + minGap)));

        // 2. Calcular un gap dinámico para distribuir uniformemente (elimina espacio vacío a la derecha)
        float totalGaps = static_cast<float>(cols - 1);
        float dynamicGap = minGap;
        if (totalGaps > 0) {
            float extraSpace = availWidth - (cols * cW);
            dynamicGap = std::max(minGap, extraSpace / totalGaps);
        }

        int currentCol = 0;

        // 3. Renderizar Carpetas
        for (size_t i = 0; i < m_BgFolders.size(); ++i) {
            RenderFolderCard(m_BgFolders[i], cW, cH, currentCol, cols);
            
            currentCol++;
            if (currentCol < cols) {
                // Forzamos el SameLine con el gap dinámico calculado
                ImGui::SameLine(0.0f, dynamicGap);
            } else {
                currentCol = 0;
                ImGui::Dummy({0.0f, minGap}); // Espaciado vertical entre filas
            }
        }

        // 4. Renderizar Archivos Raíz (Backgrounds)
        for (size_t i = 0; i < roots.size(); ++i) {
            RenderBgCard(*roots[i], cW, cH, currentCol, cols);

            currentCol++;
            if (currentCol < cols) {
                ImGui::SameLine(0.0f, dynamicGap);
            } else {
                currentCol = 0;
                ImGui::Dummy({0.0f, minGap}); // Espaciado vertical entre filas
            }
        }
    } else {
        // Vista de lista: Agregamos un pequeño margen para que no toque los bordes
        float availW = ImGui::GetContentRegionAvail().x;
        float marginX = 16.0f; 
        float pW = availW - (marginX * 2.0f);
        float cursorStartX = ImGui::GetCursorPosX() + marginX;

        for (const auto& fn : m_BgFolders) {
            ImGui::SetCursorPosX(cursorStartX);
            RenderFolderRow(fn, pW, 44.0f);
        }
        for (const auto* bg : roots) {
            ImGui::SetCursorPosX(cursorStartX);
            RenderBgRow(*bg, pW, 44.0f);
        }
    }
}

void LayersBgTab::RenderFilesInFolder() {
    std::vector<const BgEntry*> files;
    for (const auto& bg : m_AllBackgrounds) if (bg.folder==m_CurrentBgFolder) files.push_back(&bg);

    if (files.empty()) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, LP::TextMuted);
        ImGui::Text("Esta carpeta esta vacia.");
        ImGui::PopStyleColor();
        return;
    }

    if (m_GridMode) {
        const float cW = 150.0f;
        const float cH = 92.0f;
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
        for (size_t i = 0; i < files.size(); ++i) {
            RenderBgCard(*files[i], cW, cH, currentCol, cols);
            
            currentCol++;
            if (currentCol < cols) {
                ImGui::SameLine(0.0f, dynamicGap);
            } else {
                currentCol = 0;
                ImGui::Dummy({0.0f, minGap});
            }
        }
    } else {
        float availW = ImGui::GetContentRegionAvail().x;
        float marginX = 16.0f; 
        float pW = availW - (marginX * 2.0f);
        float cursorStartX = ImGui::GetCursorPosX() + marginX;

        for (const auto* bg : files) {
            ImGui::SetCursorPosX(cursorStartX);
            RenderBgRow(*bg, pW, 44.0f);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Render principal del tab
// ─────────────────────────────────────────────────────────────────────────────
void LayersBgTab::Render() {
    // Limpiar flag de proteccion contra autoclick UNA vez al inicio del frame
    // (ya se procesaron todos los items del frame anterior)
    m_JustEnteredFolder = false;

    ImGui::Spacing();
    RenderToolbar();
    ImGui::Spacing();
    LPSeparatorLine();

    RenderBreadcrumb();

    if (m_CurrentBgFolder.empty())
        RenderFolderView();
    else
        RenderFilesInFolder();

    // Modales (abrir popups debe ir fuera de los InvisibleButtons)
    RenderCreateFolderModal();
    RenderRenameBgModal();
    RenderRenameFolderModal();
}

} // namespace ProyecThor::UI