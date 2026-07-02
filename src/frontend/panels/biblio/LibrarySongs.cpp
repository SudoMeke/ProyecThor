#include "LibrarySongs.h"
#include "LibraryModals.h"
#include "LibraryIcons.h"
#include "LibraryStyles.h"
#include "LibraryHelpers.h"
#include "ui/DesignSystem.h"

#include <imgui.h>
#include <imgui_internal.h>
#include "backend/core/PresentationCore.h"
#include "UIStrings.h"
#include "frontend/ui/bin/StyleGeneralApp.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cstring>

namespace fs = std::filesystem;
namespace DS = ProyecThor::UI::DS;

static constexpr int kCat_Songs     = 0;
static constexpr int kCat_Videos    = 1;
static constexpr int kCat_Images    = 2;
static constexpr int kCat_Bibles    = 3;
static constexpr int kCat_Documents = 4;

namespace ProyecThor::Library {

// =============================================================================
//  ApplyDefaultStyleIfSet
// =============================================================================
static void ApplyDefaultStyleIfSet(LibraryContext& ctx)
{
    Core::ItemType itemType;
    bool applies = false;

    if (ctx.currentCategoryInt == kCat_Songs) {
        itemType = Core::ItemType::Song;
        applies  = true;
    } else if (ctx.currentCategoryInt == kCat_Bibles) {
        itemType = Core::ItemType::Bible;
        applies  = true;
    }

    if (!applies) return;

    std::string defaultStyle =
        Core::PresentationCore::Get().GetCategoryDefaultStyle(itemType);

    if (defaultStyle.empty()) return;

    ctx.applyStyle(defaultStyle);
}

// =============================================================================
//  LoadSongVerses
// =============================================================================
std::vector<std::string> LoadSongVerses(const std::string& filename)
{
    std::vector<std::string> verses;
    std::ifstream file(U8Path(GetAssetsPath() + "/songs/" + filename),
                       std::ios::binary);
    if (!file.is_open()) {
        verses.push_back("Error: No se pudo abrir el archivo.\nRuta: " +
                         GetAssetsPath() + "/songs/" + filename);
        return verses;
    }

    std::string raw((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
    file.close();
    if (raw.empty()) return verses;

    std::string content = NormalizeToUtf8(raw);
    std::string line, verse;
    std::istringstream stream(content);
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) {
            if (!verse.empty()) { verses.push_back(verse); verse.clear(); }
        } else {
            verse += line + '\n';
        }
    }
    if (!verse.empty()) verses.push_back(verse);
    return verses;
}

// =============================================================================
//  Autor de cancion — persistido aparte en songs_authors.ini
//  (mismo patron que category_styles.ini en PresentationCore). No se mete
//  dentro del .txt de la cancion para no romper el parseo por estrofas de
//  LoadSongVerses (separadas por linea en blanco).
// =============================================================================
static std::string SongAuthorsFilePath()
{
    return GetAssetsPath() + "/../songs_authors.ini";
}

static std::unordered_map<std::string, std::string> LoadSongAuthors()
{
    std::unordered_map<std::string, std::string> authors;
    std::ifstream f(U8Path(SongAuthorsFilePath()));
    if (!f.is_open()) return authors;

    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto sep = line.find('=');
        if (sep == std::string::npos) continue;
        authors[line.substr(0, sep)] = line.substr(sep + 1);
    }
    return authors;
}

void SetSongAuthor(const std::string& filename, const std::string& author)
{
    auto authors = LoadSongAuthors();
    if (author.empty()) authors.erase(filename);
    else                authors[filename] = author;

    std::ofstream f(U8Path(SongAuthorsFilePath()));
    if (!f.is_open()) return;
    for (const auto& [k, v] : authors)
        f << k << "=" << v << "\n";
}

std::string GetSongAuthor(const std::string& filename)
{
    auto authors = LoadSongAuthors();
    auto it = authors.find(filename);
    return it != authors.end() ? it->second : "";
}

// =============================================================================
//  CreateNewSong / SaveSong
// =============================================================================
void CreateNewSong(LibraryContext& ctx)
{
    memset(ctx.editTitle,   0, 256);
    memset(ctx.editContent, 0, 8192);
    memset(ctx.editAuthor,  0, 256);
    ctx.showSongEditor = true;
}

void SaveSong(LibraryContext& ctx,
              const std::string& title,
              const std::string& content,
              const std::string& author)
{
    if (title.empty()) return;
    std::string filename = title;
    if (filename.find(".txt") == std::string::npos) filename += ".txt";

    std::ofstream f(U8Path(GetAssetsPath() + "/songs/" + filename));
    if (f.is_open()) {
        f << "\xEF\xBB\xBF";
        f << content;
        SetSongAuthor(filename, author);
        ctx.refreshList();
    }
}

// =============================================================================
//  RenderSideList
// =============================================================================
void RenderSideList(LibraryContext& ctx)
{
    const auto& str = ProyecThor::UI::GetUIStrings();

    // ── Barra de búsqueda ──────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.08f, 0.09f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.11f, 0.13f, 0.24f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImVec4(0.14f, 0.16f, 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border,         ImVec4(1.00f, 1.00f, 1.00f, 0.12f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(10.f, 7.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputTextWithHint("##search", str.libSearchHint,
                                 ctx.searchBuffer, ctx.searchBufferSize))
        ForceListUpdate() = true;
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(4);

    // Ícono de búsqueda superpuesto
    if (StyleGeneralApp::Icons.count("search") > 0) {
        void* icon = StyleGeneralApp::Icons["search"].textureID;
        if (icon) {
            constexpr float pad = 5.0f;
            ImVec2 iMin = ImGui::GetItemRectMin();
            ImVec2 iMax = ImGui::GetItemRectMax();
            float  bH   = iMax.y - iMin.y;
            ImGui::GetWindowDrawList()->AddImage(
                icon,
                { iMax.x - bH + pad, iMin.y + pad },
                { iMax.x - pad,       iMax.y - pad },
                { 0, 0 }, { 1, 1 }, IM_COL32(255, 255, 255, 100));
        }
    }

    ImGui::Spacing();
    RenderDefaultStyleCombo(ctx);
    ImGui::Spacing();

    // ── Cálculo de espacio reservado para el footer ────────────────────────
    //
    //  Footer layout:
    //    Fila 1: [  Nuevo  ] [  Importar  ] [  Eliminar  ]   (3 botones iguales)
    //    Fila 2: [            Actualizar             ]        (1 botón ancho)
    //
    const float itemSpY    = ImGui::GetStyle().ItemSpacing.y;
    const float btnRowH    = DS::ButtonHeight;
    const float reservedH  = btnRowH * 2.0f          // 2 filas de botones
                           + itemSpY  * 3.0f          // espacios entre filas
                           + 6.0f;                    // padding extra

    // ── Lista con scroll ───────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg,    ImVec4(0.f,    0.f,    0.f,    0.f));
    ImGui::PushStyleColor(ImGuiCol_Border,     ImVec4(1.f,    1.f,    1.f,    0.06f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   DS::RadiusMedium);

    if (ImGui::BeginChild("ListChild", { 0.f, -reservedH }, true))
    {
        static std::vector<std::string> filteredItems;
        static std::string lastSearch;

        std::string cur(ctx.searchBuffer);
        std::transform(cur.begin(), cur.end(), cur.begin(),
                       [](unsigned char c){ return (char)::tolower(c); });

        if (cur != lastSearch || ForceListUpdate()) {
            filteredItems.clear();
            for (const auto& item : ctx.items) {
                std::string lo = item;
                std::transform(lo.begin(), lo.end(), lo.begin(),
                               [](unsigned char c){ return (char)::tolower(c); });
               bool match = cur.empty() || lo.find(cur) != std::string::npos;

if (!match && ctx.currentCategoryInt == kCat_Songs) {
    std::string author = GetSongAuthor(item);
    std::transform(author.begin(), author.end(), author.begin(),
                   [](unsigned char c){ return (char)::tolower(c); });
    if (author.find(cur) != std::string::npos)
        match = true;
}

if (!match && ctx.currentCategoryInt == kCat_Songs) {
    for (const auto& v : ctx.loadSongVerses(item)) {
        std::string vl = v;
        std::transform(vl.begin(), vl.end(), vl.begin(),
                       [](unsigned char c){ return (char)::tolower(c); });
        if (vl.find(cur) != std::string::npos) { match = true; break; }
    }
}
                if (match) filteredItems.push_back(item);
            }
            lastSearch        = cur;
            ForceListUpdate() = false;
        }

        if (filteredItems.empty()) {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            ImGui::SetCursorPos({
                std::floor(avail.x * 0.5f - 55.f),
                std::floor(avail.y * 0.5f - 10.f) });
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.38f, 0.55f, 1.0f));
            ImGui::TextUnformatted("Sin resultados");
            ImGui::PopStyleColor();
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 1.f));

        for (int n = 0; n < (int)filteredItems.size(); n++)
        {
            auto it = std::find(ctx.items.begin(), ctx.items.end(), filteredItems[n]);
            int origIdx = (it != ctx.items.end())
                ? (int)std::distance(ctx.items.begin(), it) : -1;

            const bool sel   = (ctx.selectedIndex == origIdx);
std::string disp = StripExtension(filteredItems[n]);

if (ctx.currentCategoryInt == kCat_Songs)
{
    std::string author = GetSongAuthor(filteredItems[n]);
    if (!author.empty())
        disp += "  —  " + author;
}

            bool clicked = DS::GlassListRow(disp.c_str(), sel);

            if (clicked)
            {
                ctx.selectedIndex = origIdx;

                Core::LibrarySelection s;
                s.title = filteredItems[n];
                switch (ctx.currentCategoryInt) {
                    case kCat_Songs:     s.type = Core::ItemType::Song;      break;
                    case kCat_Videos:    s.type = Core::ItemType::Video;     break;
                    case kCat_Images:    s.type = Core::ItemType::Image;     break;
                    case kCat_Bibles:    s.type = Core::ItemType::Bible;     break;
                    case kCat_Documents: s.type = Core::ItemType::Documents; break;
                    default:             s.type = Core::ItemType::None;      break;
                }
                if (ctx.currentCategoryInt == kCat_Songs)
                    s.contentData = ctx.loadSongVerses(filteredItems[n]);
                else if (ctx.currentCategoryInt == kCat_Documents) {
                    fs::path docDir =
                        U8Path(GetAssetsPath() + "/documents") / U8Path(filteredItems[n]);
                    if (fs::exists(docDir) && fs::is_directory(docDir)) {
                        std::vector<std::string> pages;
                        for (const auto& pe : fs::directory_iterator(docDir))
                            if (pe.is_regular_file())
                                pages.push_back(WideToUtf8(pe.path().wstring()));
                        std::sort(pages.begin(), pages.end());
                        s.contentData = pages;
                    }
                }

                Core::PresentationCore::Get().SetSelection(s);
                ApplyDefaultStyleIfSet(ctx);
            }

            if (ImGui::BeginPopupContextItem(("##ctx_sl" + std::to_string(n)).c_str()))
            {
                if (ImGui::MenuItem("Renombrar")) {
                    ctx.renameOldName  = filteredItems[n];
                    ctx.renameIsURL    = false;
                    ctx.renameURLIndex = -1;
                    ctx.selectedIndex  = origIdx;
                    std::string stem = SplitExtension(filteredItems[n], ctx.renameExtension);
                    memset(ctx.renameBuffer, 0, 512);
                    strncpy(ctx.renameBuffer, stem.c_str(), 511);
                    ctx.showRenameModal = true;
                }
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.45f, 0.40f, 1.0f));
                if (ImGui::MenuItem("Eliminar")) {
                    ImGui::PopStyleColor();
                    ctx.selectedIndex = origIdx;
                    ImGui::EndPopup();
                    ctx.deleteSelectedItem();
                    break;
                }
                ImGui::PopStyleColor();
                ImGui::EndPopup();
            }
        }

        ImGui::PopStyleVar(); // ItemSpacing
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    ImGui::Spacing();

    // ── Footer con botones ─────────────────────────────────────────────────
    {
        const float avail = ImGui::GetContentRegionAvail().x;
        const float sp    = ImGui::GetStyle().ItemSpacing.x;

        // Fila 1: Nuevo | Importar | Eliminar (tres iguales)
        const float bw3 = std::floor((avail - sp * 2.0f) / 3.0f);

        if (DS::GlassButton(str.newLabel,    { bw3, DS::ButtonHeight }))
            CreateNewSong(ctx);
        ImGui::SameLine();
        if (DS::GlassButton(str.importLabel, { bw3, DS::ButtonHeight }))
            ctx.importFile();
        ImGui::SameLine();
        if (DS::GlassButton(str.deleteLabel, { bw3, DS::ButtonHeight }, DS::DangerColor))
            ctx.deleteSelectedItem();

        ImGui::Spacing();

        // Fila 2: Actualizar — ancho completo para que no quede suelto
        if (DS::GlassButton(str.refresh, { -1.f, DS::ButtonHeight }))
            ctx.refreshList();
    }
}
// =============================================================================
//  RenderSongEditor
// =============================================================================
void RenderSongEditor(LibraryContext& ctx)
{
    const auto& str = ProyecThor::UI::GetUIStrings();

    if (ctx.showSongEditor) ImGui::OpenPopup("SongEditorModal##lib");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, { 0.5f, 0.5f });
    ImGui::SetNextWindowSize({ 720.f, 600.f }, ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints({ 480.f, 420.f }, { FLT_MAX, FLT_MAX });

    // Fondo glass para el modal
    ImGui::PushStyleColor(ImGuiCol_PopupBg,  ImVec4(0.07f, 0.08f, 0.16f, 0.97f));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(1.00f, 1.00f, 1.00f, 0.15f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, DS::RadiusLarge);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(22.f, 18.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    if (ImGui::BeginPopupModal("SongEditorModal##lib", &ctx.showSongEditor,
                               ImGuiWindowFlags_NoSavedSettings))
    {
        ImDrawList* dl   = ImGui::GetWindowDrawList();
        ImVec2 wPos      = ImGui::GetWindowPos();
        ImVec2 wSize     = ImGui::GetWindowSize();
        ImVec2 wMax      = ImVec2(wPos.x + wSize.x, wPos.y + wSize.y);

        // Highlight especular del modal
        dl->AddLine(
            ImVec2(wPos.x + DS::RadiusLarge, wPos.y + 0.5f),
            ImVec2(wMax.x - DS::RadiusLarge, wPos.y + 0.5f),
            DS::GlassHighlight, 1.0f);

        // ── Título de la ventana ──────────────────────────────────────────
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.90f, 1.00f, 1.0f));
        ImGui::TextUnformatted(str.newLabel);
        ImGui::PopStyleColor();

        DS::GlassSeparator();
        ImGui::Spacing();

        // ── Estilo compartido de los campos ────────────────────────────────
        ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.08f, 0.10f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.11f, 0.14f, 0.28f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImVec4(0.14f, 0.18f, 0.35f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border,         ImVec4(1.00f, 1.00f, 1.00f, 0.14f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(10.f, 8.f));

        ImU32 labelCol = ImGui::ColorConvertFloat4ToU32(ImVec4(0.55f, 0.60f, 0.80f, 1.0f));

        // ── Título + Autor en la misma fila ────────────────────────────────
        {
            const float avail = ImGui::GetContentRegionAvail().x;
            const float sp    = ImGui::GetStyle().ItemSpacing.x;
            const float halfW = (avail - sp) * 0.5f;

            ImGui::BeginGroup();
            ImGui::PushStyleColor(ImGuiCol_Text, labelCol);
            ImGui::TextUnformatted("Título");
            ImGui::PopStyleColor();
            ImGui::SetNextItemWidth(halfW);
            ImGui::InputText("##editTitle", ctx.editTitle, 256);
            ImGui::EndGroup();

            ImGui::SameLine(0.f, sp);

            ImGui::BeginGroup();
            ImGui::PushStyleColor(ImGuiCol_Text, labelCol);
            ImGui::TextUnformatted("Autor");
            ImGui::PopStyleColor();
            ImGui::SetNextItemWidth(halfW);
            ImGui::InputText("##editAuthor", ctx.editAuthor, 256);
            ImGui::EndGroup();
        }

        ImGui::Spacing();

        // ── Contenido ───────────────────────────────────────────────────────
        ImGui::PushStyleColor(ImGuiCol_Text, labelCol);
        ImGui::TextUnformatted("Contenido  (separa estrofas con una línea en blanco)");
        ImGui::PopStyleColor();

        // Reserva exacta para: separador + spacing + fila de botones,
        // asi el area de texto se achica sola cuando la ventana es chica
        // y los botones NUNCA quedan cortados fuera de la vista.
        const float footerH = 40.f + ImGui::GetStyle().ItemSpacing.y * 2.0f
                                     + 1.0f  // separador
                                     + 6.0f; // margen extra

        ImGui::InputTextMultiline("##editContent", ctx.editContent, 8192,
                                  { -FLT_MIN, ImGui::GetContentRegionAvail().y - footerH });

        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);

        ImGui::Spacing();
        DS::GlassSeparator();
        ImGui::Spacing();

        // ── Botones ────────────────────────────────────────────────────────
        {
            const float avail = ImGui::GetContentRegionAvail().x;
            const float sp    = ImGui::GetStyle().ItemSpacing.x;
            const float bw2   = std::floor((avail - sp) * 0.5f);

            if (DS::GlassButton(str.save, { bw2, 40.f })) {
                SaveSong(ctx, ctx.editTitle, ctx.editContent, ctx.editAuthor);
                ctx.showSongEditor = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (DS::GlassButton(str.cancel, { bw2, 40.f }, DS::TextSecondary)) {
                ctx.showSongEditor = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}
} // namespace ProyecThor::Library