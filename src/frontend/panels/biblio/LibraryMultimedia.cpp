#include "LibraryMultimedia.h"
#include "LibraryIcons.h"
#include "LibraryStyles.h"
#include "LibraryHelpers.h"
#include "ui/DesignSystem.h"
#include "frontend/panels/layers/LayersTheme.h"
#include "frontend/views/audio/AudioHelpers.h"
#include "frontend/views/audio/AudioAlbumArt.h"
#include "backend/core/ThumbnailWorker.h"
#include "backend/core/PresentationCore.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <system_error>
#include <cmath>
#include <GL/gl.h>
#include "stb_image.h"

namespace DS = ProyecThor::UI::DS;

namespace ProyecThor::Library {

// =============================================================================
//  Listas y carpetas
// =============================================================================

struct MMItem {
    std::string    filename;
    Core::ItemType type;
};

static std::vector<MMItem> s_Videos, s_Audios, s_Images;

static std::string VideoFolder() { return GetAssetsPath() + "/videos"; }
static std::string ImageFolder() { return GetAssetsPath() + "/images"; }
static std::string AudioFolder() { return ProyecThor::Audio::GetAudioPath(); }

static std::string ItemFullPath(const MMItem& it) {
    switch (it.type) {
        case Core::ItemType::Video: return VideoFolder() + "/" + it.filename;
        case Core::ItemType::Image: return ImageFolder() + "/" + it.filename;
        default:                    return AudioFolder() + "/" + it.filename;
    }
}

static void ScanFolder(const std::string& folder, const std::vector<std::string>& exts,
                        Core::ItemType type, std::vector<MMItem>& out)
{
    out.clear();
    std::error_code ec;
    fs::path dir = U8Path(folder);
    if (!fs::exists(dir, ec)) return;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        if (std::find(exts.begin(), exts.end(), ext) == exts.end()) continue;
        out.push_back({ PathToUtf8(entry.path().filename()), type });
    }
    std::sort(out.begin(), out.end(),
              [](const MMItem& a, const MMItem& b) { return a.filename < b.filename; });
}

void RefreshMultimediaLists()
{
    ScanFolder(VideoFolder(), { ".mp4", ".mkv", ".avi", ".mov" }, Core::ItemType::Video, s_Videos);
    ScanFolder(ImageFolder(), { ".jpg", ".jpeg", ".png" }, Core::ItemType::Image, s_Images);
    ScanFolder(AudioFolder(),
              { ".mp3", ".flac", ".wav", ".ogg", ".aac", ".m4a", ".wma", ".opus", ".aiff" },
              Core::ItemType::Audio, s_Audios);
}

// =============================================================================
//  Miniaturas — una cache por tipo, mismo criterio que el resto de Biblioteca
//  (cada seccion la suya, no compartida).
// =============================================================================

static ImTextureID LoadThumbFromDisk(const char* path) {
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

// ── Video: primer frame real via ThumbnailWorker, con cache en disco ────────
static fs::path VideoThumbCacheDir() {
    fs::path dir = fs::path(GetAssetsPath()) / "thumbnails";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}
static std::string VideoThumbCachePathFor(const std::string& absVideoPath) {
    std::error_code ec;
    auto sz = fs::file_size(absVideoPath, ec);
    size_t h = std::hash<std::string>{}(absVideoPath + "|" + std::to_string(ec ? 0 : sz));
    return (VideoThumbCacheDir() / (std::to_string(h) + ".png")).string();
}

static std::unordered_map<std::string, ImTextureID> s_VideoThumbCache;
static ProyecThor::Core::ThumbnailWorker            s_VideoThumbWorker;

static ImTextureID GetVideoThumbnail(const std::string& path) {
    auto it = s_VideoThumbCache.find(path);
    if (it != s_VideoThumbCache.end()) return it->second;

    std::string abs = fs::absolute(fs::path(path)).string();
    std::string cachePath = VideoThumbCachePathFor(abs);
    std::error_code ec;
    if (fs::exists(cachePath, ec)) {
        ImTextureID t = LoadThumbFromDisk(cachePath.c_str());
        if (t) { s_VideoThumbCache[path] = t; return t; }
    }
    s_VideoThumbWorker.Request(path, abs, cachePath);
    return 0;
}

static void DrainVideoThumbnails() {
    std::vector<ProyecThor::Core::ThumbnailWorker::Result> results;
    s_VideoThumbWorker.DrainResults(results);
    for (auto& r : results) {
        ImTextureID t = 0;
        if (!r.pixels.empty() && r.width > 0 && r.height > 0) {
            GLuint tex; glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)r.width, (GLsizei)r.height,
                        0, GL_RGBA, GL_UNSIGNED_BYTE, r.pixels.data());
            t = (ImTextureID)(intptr_t)tex;
        }
        s_VideoThumbCache[r.key] = t;
    }
}

// ── Imagen: la imagen misma, escalada por ImGui al dibujar ──────────────────
static std::unordered_map<std::string, ImTextureID> s_ImageThumbCache;

static ImTextureID GetImageThumbnail(const std::string& path) {
    auto it = s_ImageThumbCache.find(path);
    if (it != s_ImageThumbCache.end()) return it->second;
    ImTextureID t = LoadThumbFromDisk(path.c_str());
    s_ImageThumbCache[path] = t;
    return t;
}

// ── Audio: portada embebida (ID3/FLAC/M4A/OGG), generica si no hay ──────────
static std::unordered_map<std::string, ImTextureID> s_AudioArtCache;

static ImTextureID GetAudioThumbnail(const std::string& path) {
    auto it = s_AudioArtCache.find(path);
    if (it != s_AudioArtCache.end()) return it->second;

    ImTextureID t = 0;
    Audio::AlbumArt art = Audio::ExtractAlbumArt(path);
    if (art.HasData()) {
        Audio::UploadAlbumArtToGL(art);
        if (art.HasTexture()) t = (ImTextureID)(intptr_t)art.texID;
    }
    s_AudioArtCache[path] = t;
    return t;
}

// =============================================================================
//  Renombrar / Eliminar — estado propio (no reusa ctx.showRenameModal: ese
//  modal resuelve la carpeta por m_CurrentCategory, que aca es "Multimedia"
//  y no alcanza para saber a que carpeta pertenece el item).
// =============================================================================

static bool        s_ShowRenameModal = false;
static MMItem       s_RenameItem;
static char         s_RenameBuffer[256]{};

static bool        s_ShowDeleteModal = false;
static MMItem       s_DeleteItem;

static void RequestRename(const MMItem& item) {
    s_RenameItem = item;
    std::string ext;
    std::string stem = SplitExtension(item.filename, ext);
    std::memset(s_RenameBuffer, 0, sizeof(s_RenameBuffer));
    std::strncpy(s_RenameBuffer, stem.c_str(), sizeof(s_RenameBuffer) - 1);
    s_ShowRenameModal = true;
}

static void RequestDelete(const MMItem& item) {
    s_DeleteItem      = item;
    s_ShowDeleteModal = true;
}

static void RenderRenameModal() {
    if (!s_ShowRenameModal) return;
    ImGui::OpenPopup("Renombrar##mm");
    ImGui::SetNextWindowSize(ImVec2(360, 0));
    if (ImGui::BeginPopupModal("Renombrar##mm", &s_ShowRenameModal, ImGuiWindowFlags_NoResize)) {
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##mmRenameBuf", s_RenameBuffer, sizeof(s_RenameBuffer));

        if (ImGui::Button("Renombrar", ImVec2(160, 0))) {
            std::string ext;
            SplitExtension(s_RenameItem.filename, ext);
            std::string newName = std::string(s_RenameBuffer) + ext;
            std::string folder = (s_RenameItem.type == Core::ItemType::Video) ? VideoFolder()
                                : (s_RenameItem.type == Core::ItemType::Image) ? ImageFolder()
                                                                                : AudioFolder();
            std::error_code ec;
            fs::rename(U8Path(folder + "/" + s_RenameItem.filename),
                      U8Path(folder + "/" + newName), ec);
            RefreshMultimediaLists();
            s_ShowRenameModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(120, 0))) {
            s_ShowRenameModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

static void RenderDeleteModal() {
    if (!s_ShowDeleteModal) return;
    ImGui::OpenPopup("Eliminar##mm");
    ImGui::SetNextWindowSize(ImVec2(360, 0));
    if (ImGui::BeginPopupModal("Eliminar##mm", &s_ShowDeleteModal, ImGuiWindowFlags_NoResize)) {
        ImGui::TextWrapped("Eliminar \"%s\"? Esta accion no se puede deshacer.",
                           s_DeleteItem.filename.c_str());
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button,        DS::DangerColorDim);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  DS::DangerColor);
        if (ImGui::Button("Eliminar", ImVec2(160, 0))) {
            std::error_code ec;
            fs::remove(U8Path(ItemFullPath(s_DeleteItem)), ec);
            RefreshMultimediaLists();
            s_ShowDeleteModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(120, 0))) {
            s_ShowDeleteModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// =============================================================================
//  Fila de item — mismo lenguaje visual que RenderVideoRow (LibraryVideos.cpp)
// =============================================================================

static std::string s_SelectedFile;

static void RenderMMRow(const MMItem& item, int rowIdx) {
    ImGui::PushID(rowIdx);

    ImTextureID thumb;
    const char* fallbackGlyph;
    switch (item.type) {
        case Core::ItemType::Video: thumb = GetVideoThumbnail(ItemFullPath(item)); fallbackGlyph = "V"; break;
        case Core::ItemType::Image: thumb = GetImageThumbnail(ItemFullPath(item)); fallbackGlyph = "I"; break;
        default:                    thumb = GetAudioThumbnail(ItemFullPath(item)); fallbackGlyph = "A"; break;
    }

    const float thumbSz = 22.0f;
    const float indent  = thumbSz + 16.0f;

    ImVec2      rowPos = ImGui::GetCursorScreenPos();
    bool        sel    = (s_SelectedFile == item.filename);
    std::string disp   = StripExtension(item.filename);

    bool clicked = DS::GlassListRow(disp.c_str(), sel, indent);

    ImDrawList* dl     = ImGui::GetWindowDrawList();
    float       thumbY = rowPos.y + (DS::RowHeight - thumbSz) * 0.5f;
    if (thumb) {
        dl->AddImageRounded(thumb, {rowPos.x + 8.0f, thumbY},
                            {rowPos.x + 8.0f + thumbSz, thumbY + thumbSz},
                            {0,0}, {1,1}, IM_COL32_WHITE, DS::RadiusSmall);
    } else {
        dl->AddRectFilled({rowPos.x + 8.0f, thumbY},
                          {rowPos.x + 8.0f + thumbSz, thumbY + thumbSz},
                          DS::BtnDefaultFill, DS::RadiusSmall);
        ImVec2 ts = ImGui::CalcTextSize(fallbackGlyph);
        dl->AddText({rowPos.x + 8.0f + (thumbSz-ts.x)*0.5f, thumbY + (thumbSz-ts.y)*0.5f},
                    DS::TextSecondary, fallbackGlyph);
    }

    if (clicked) {
        s_SelectedFile = item.filename;
        Core::LibrarySelection s;
        s.title = item.filename;
        s.type  = item.type;
        Core::PresentationCore::Get().SetSelection(s);
    }

    if (item.type == Core::ItemType::Video && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        std::string fullPath = ItemFullPath(item);
        ImGui::SetDragDropPayload("VIDEO_TO_QUEUE", fullPath.c_str(), fullPath.size() + 1);
        ImGui::PushStyleColor(ImGuiCol_Text, DS::SuccessColor);
        ImGui::TextUnformatted(disp.c_str());
        ImGui::PopStyleColor();
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginPopupContextItem(("##ctx_mm" + std::to_string(rowIdx)).c_str())) {
        if (item.type == Core::ItemType::Video) {
            if (ImGui::MenuItem("Enviar al monitor")) {
                Core::PresentationCore::Get().SetBackgroundMedia(ItemFullPath(item), true, /*allowAudio=*/true);
                Core::PresentationCore::Get().SetProjecting(true);
            }
            ImGui::Separator();
        }
        if (ImGui::MenuItem("Renombrar")) RequestRename(item);
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, DS::DangerColor);
        if (ImGui::MenuItem("Eliminar")) RequestDelete(item);
        ImGui::PopStyleColor();
        ImGui::EndPopup();
    }

    ImGui::PopID();
}

// =============================================================================
//  Seccion (encabezado + filas) — todas o solo el tipo filtrado
// =============================================================================

static void RenderMMSection(const char* label, const std::vector<MMItem>& items,
                            const std::string& searchLower, int& rowCounter)
{
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, DS::TextSecondary);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    bool any = false;
    for (const auto& item : items) {
        if (!searchLower.empty()) {
            std::string lo = item.filename;
            std::transform(lo.begin(), lo.end(), lo.begin(),
                           [](unsigned char c) { return (char)std::tolower(c); });
            if (lo.find(searchLower) == std::string::npos) continue;
        }
        RenderMMRow(item, rowCounter++);
        any = true;
    }

    if (!any) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, DS::TextHint);
        ImGui::TextUnformatted("(vacio)");
        ImGui::PopStyleColor();
    }

    ImGui::Dummy(ImVec2(0.0f, 16.0f));
}

// =============================================================================
//  RenderMultimediaSection
// =============================================================================

void RenderMultimediaSection(LibraryContext& ctx, MultimediaFilter& filter)
{
    static bool s_Loaded = false;
    if (!s_Loaded) { RefreshMultimediaLists(); s_Loaded = true; }

    DrainVideoThumbnails();

    // ── Barra de busqueda ────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImGui::ColorConvertU32ToFloat4(DS::AccentColorDim));
    ImGui::PushStyleColor(ImGuiCol_Border,         ImVec4(1.00f, 1.00f, 1.00f, 0.12f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(10.f, 7.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputTextWithHint("##mmsearch", "Buscar...", ctx.searchBuffer, ctx.searchBufferSize);
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(4);

    ImGui::Spacing();

    // ── Filtros: Todos | Video | Audio | Imagen ─────────────────────────
    // Mismo tamano de boton/celda para los 6 (LPCornerIconBtn ya centra y
    // escala cada icono al mismo radio relativo, r = btnSz*0.42), y todos
    // los glifos hechos a mano aca (Todos/Actualizar/Importar) usan el mismo
    // grosor de trazo (~0.09*r) que el resto de iconos de LibraryIcons.h,
    // para que no se vean mas "gruesos" que Video/Audio/Imagen.
    {
        const float btnSz = 26.0f;
        const float gap   = 4.0f;

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 2.0f);
        ImGui::PushID("mmfilters");
        if (UI::LPCornerIconBtn("##mmall", +[](ImDrawList* dl, ImVec2 c, float r, ImU32 col){
                float thick = r * 0.16f;
                for (int i = 0; i < 3; i++) {
                    float ang = i * (3.14159265f / 3.0f);
                    ImVec2 d  = { std::cos(ang) * r * 0.55f, std::sin(ang) * r * 0.55f };
                    dl->AddLine({ c.x - d.x, c.y - d.y }, { c.x + d.x, c.y + d.y }, col, thick);
                }
            }, "Todos", {btnSz,btnSz}, filter == MultimediaFilter::All))
            filter = MultimediaFilter::All;
        ImGui::SameLine(0, gap);
        if (UI::LPCornerIconBtn("##mmvid", DrawIcon_Play, "Solo video", {btnSz,btnSz},
                                filter == MultimediaFilter::Video))
            filter = MultimediaFilter::Video;
        ImGui::SameLine(0, gap);
        if (UI::LPCornerIconBtn("##mmaud", DrawIcon_Audio, "Solo audio", {btnSz,btnSz},
                                filter == MultimediaFilter::Audio))
            filter = MultimediaFilter::Audio;
        ImGui::SameLine(0, gap);
        if (UI::LPCornerIconBtn("##mmimg", DrawIcon_Image, "Solo imagen", {btnSz,btnSz},
                                filter == MultimediaFilter::Image))
            filter = MultimediaFilter::Image;
        ImGui::PopID();

        // Separador chico entre los filtros y las utilidades (Actualizar/
        // Importar), para que se lean como dos grupos distintos.
        ImGui::SameLine(0, gap * 2.0f);
        {
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddLine(
                { p.x, p.y + 4.0f }, { p.x, p.y + btnSz - 4.0f },
                ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.10f)), 1.0f);
            ImGui::Dummy(ImVec2(1.0f, btnSz));
        }
        ImGui::SameLine(0, gap * 2.0f);

        if (UI::LPCornerIconBtn("##mmrefresh", +[](ImDrawList* dl, ImVec2 c, float r, ImU32 col){
                float thick = r * 0.11f;
                dl->PathArcTo(c, r * 0.55f, -IM_PI * 0.15f, IM_PI * 1.55f, 16);
                dl->PathStroke(col, ImDrawFlags_None, thick);
                dl->AddTriangleFilled({c.x+r*0.55f,c.y-r*0.14f}, {c.x+r*0.85f,c.y+r*0.10f}, {c.x+r*0.25f,c.y+r*0.10f}, col);
            }, "Actualizar", {btnSz,btnSz}))
            RefreshMultimediaLists();

        ImGui::SameLine(0, gap);
        if (UI::LPCornerIconBtn("##mmimport", +[](ImDrawList* dl, ImVec2 c, float r, ImU32 col){
                float thick = r * 0.14f;
                dl->AddLine({c.x, c.y-r*0.45f}, {c.x, c.y+r*0.45f}, col, thick);
                dl->AddLine({c.x-r*0.45f, c.y}, {c.x+r*0.45f, c.y}, col, thick);
            }, "Importar archivo", {btnSz,btnSz}))
            ctx.importFile();
    }

    ImGui::Spacing();

    std::string searchLower(ctx.searchBuffer);
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(1.f, 1.f, 1.f, 0.06f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   DS::RadiusMedium);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(4.0f, 10.0f));

    if (ImGui::BeginChild("##mm_list", { 0.f, 0.f }, true, ImGuiChildFlags_AlwaysUseWindowPadding)) {
        int rowCounter = 0;
        if (filter == MultimediaFilter::All) {
            RenderMMSection("VIDEOS",   s_Videos, searchLower, rowCounter);
            RenderMMSection("AUDIO",    s_Audios, searchLower, rowCounter);
            RenderMMSection("IMAGENES", s_Images, searchLower, rowCounter);
        } else if (filter == MultimediaFilter::Video) {
            RenderMMSection("VIDEOS", s_Videos, searchLower, rowCounter);
        } else if (filter == MultimediaFilter::Audio) {
            RenderMMSection("AUDIO", s_Audios, searchLower, rowCounter);
        } else {
            RenderMMSection("IMAGENES", s_Images, searchLower, rowCounter);
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);

    RenderRenameModal();
    RenderDeleteModal();
}

} // namespace ProyecThor::Library
