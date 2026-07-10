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

#include "LibraryPlaylists.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cmath>
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
//  GlassIconButton — boton con icono de StyleGeneralApp (fallback a glifo corto)
//  Mismo helper que en LibraryVideos.cpp / LibraryDocuments.cpp, replicado
//  aqui para que el footer de "Letra" (canciones) tambien use iconos en vez
//  de texto, igual que Video y Documentos.
//
//  FIX (tamaños): el icono se recorta como un cuadrado centrado a partir
//  del lado MENOR del boton, para no estirarse en botones anchos y bajos.
// =============================================================================
static bool GlassIconButton(const char* id,
                             const char* iconKey,
                             const char* fallbackGlyph,
                             const char* tooltip,
                             ImVec2      size,
                             ImVec4      tint = ImVec4(0.80f, 0.84f, 0.96f, 1.0f))
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.09f, 0.10f, 0.19f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.18f, 0.32f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.19f, 0.24f, 0.42f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          tint);

    auto it = StyleGeneralApp::Icons.find(iconKey);
    bool hasIcon = (it != StyleGeneralApp::Icons.end() && it->second.textureID != nullptr);
    std::string label = (hasIcon ? "" : std::string(fallbackGlyph)) + "##" + id;

    bool clicked = ImGui::Button(label.c_str(), size);

    if (hasIcon) {
        ImVec2 bMin = ImGui::GetItemRectMin();
        ImVec2 bMax = ImGui::GetItemRectMax();

        // Cuadrado centrado, basado en el lado MENOR del boton (no estira).
        const float minSide  = std::min(size.x, size.y);
        const float iconSide = minSide * 0.48f;
        const ImVec2 center  = { (bMin.x + bMax.x) * 0.5f, (bMin.y + bMax.y) * 0.5f };
        const ImVec2 pMin    = { center.x - iconSide * 0.5f, center.y - iconSide * 0.5f };
        const ImVec2 pMax    = { center.x + iconSide * 0.5f, center.y + iconSide * 0.5f };

        ImGui::GetWindowDrawList()->AddImage(
            it->second.textureID,
            pMin, pMax,
            ImVec2(0, 0), ImVec2(1, 1),
            ImGui::ColorConvertFloat4ToU32(tint));
    }

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();

    if (tooltip && ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tooltip);

    return clicked;
}

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
//  Etiquetas de cancion — songs_tags.ini, mismo patron que songs_authors.ini.
//  Formato: archivo=tag1,tag2,tag3
// =============================================================================
static std::string SongTagsFilePath()
{
    return GetAssetsPath() + "/../songs_tags.ini";
}

static std::vector<std::string> SplitTags(const std::string& raw)
{
    std::vector<std::string> out;
    std::stringstream ss(raw);
    std::string tag;
    while (std::getline(ss, tag, ',')) {
        // trim espacios
        size_t a = tag.find_first_not_of(' ');
        size_t b = tag.find_last_not_of(' ');
        if (a == std::string::npos) continue;
        out.push_back(tag.substr(a, b - a + 1));
    }
    return out;
}

static std::string JoinTags(const std::vector<std::string>& tags)
{
    std::string out;
    for (size_t i = 0; i < tags.size(); i++) {
        if (i) out += ",";
        out += tags[i];
    }
    return out;
}

static std::unordered_map<std::string, std::string> LoadSongTagsRaw()
{
    std::unordered_map<std::string, std::string> map;
    std::ifstream f(U8Path(SongTagsFilePath()));
    if (!f.is_open()) return map;

    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto sep = line.find('=');
        if (sep == std::string::npos) continue;
        map[line.substr(0, sep)] = line.substr(sep + 1);
    }
    return map;
}

std::vector<std::string> GetSongTags(const std::string& filename)
{
    auto map = LoadSongTagsRaw();
    auto it = map.find(filename);
    if (it == map.end()) return {};
    return SplitTags(it->second);
}

void SetSongTags(const std::string& filename, const std::vector<std::string>& tags)
{
    auto map = LoadSongTagsRaw();
    if (tags.empty()) map.erase(filename);
    else              map[filename] = JoinTags(tags);

    std::ofstream f(U8Path(SongTagsFilePath()));
    if (!f.is_open()) return;
    for (const auto& [k, v] : map)
        f << k << "=" << v << "\n";
}

// =============================================================================
//  ApplySongSelection
// =============================================================================
void ApplySongSelection(LibraryContext& ctx, const std::string& filename)
{
    Core::LibrarySelection s;
    s.title       = filename;
    s.type        = Core::ItemType::Song;
    s.contentData = ctx.loadSongVerses(filename);

    Core::PresentationCore::Get().SetSelection(s);
    ApplyDefaultStyleIfSet(ctx);

    auto it = std::find(ctx.items.begin(), ctx.items.end(), filename);
    if (it != ctx.items.end())
        ctx.selectedIndex = (int)std::distance(ctx.items.begin(), it);
}

// =============================================================================
//  RenderPaneHeader
//  Titulo discreto de columna del grid (Canciones / Playlists), con un
//  contador opcional. Da aire respecto al borde superior del panel y marca
//  visualmente donde empieza cada seccion.
// =============================================================================
static void RenderPaneHeader(const char* label, int count)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.52f, 0.56f, 0.78f, 1.0f));
    if (count >= 0) ImGui::Text("%s   ·   %d", label, count);
    else            ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

// =============================================================================
//  RenderToolbarRow
//  Fila superior con el combo de "Estilo por defecto" (si la categoria
//  activa lo soporta) y el boton de actualizar la lista, siempre en la
//  esquina derecha, con un poco de aire respecto al combo o al borde.
// =============================================================================
static void RenderToolbarRow(LibraryContext& ctx)
{
    const float iconBtnW = DS::ButtonHeight;
    const float sp       = 10.0f;

    const bool hasCombo = (ctx.currentCategoryInt == kCat_Songs ||
                           ctx.currentCategoryInt == kCat_Bibles);

    if (hasCombo)
    {
        RenderDefaultStyleCombo(ctx, iconBtnW + sp);
        ImGui::SameLine(0.f, sp);
    }
    else
    {
        // Sin combo en esta categoria: el boton igual se ubica en la
        // esquina derecha para mantener el layout consistente entre
        // categorias.
        float avail  = ImGui::GetContentRegionAvail().x;
        float target = ImGui::GetCursorPosX() + avail - iconBtnW;
        if (target > ImGui::GetCursorPosX())
            ImGui::SetCursorPosX(target);
    }

    if (GlassIconButton("refreshTop", "repeat", "R", "Actualizar", { iconBtnW, DS::ButtonHeight }))
        ctx.refreshList();
}

// =============================================================================
//  RenderPlaylistsSection — contenido de la columna "Playlists".
//  Lista de playlists, o el detalle (tarjetas) de la playlist abierta con
//  soporte para reordenar, quitar cancion, renombrar, eliminar y agregar
//  canciones.
// =============================================================================
static void RenderPlaylistsSection(LibraryContext& ctx)
{
    static std::string openPlaylist;
    static bool        showNewModal      = false;
    static bool        showRenameModal   = false;
    static bool        showAddSongsModal = false;
    static char        nameBuffer[256]   = {};

    std::vector<std::string> playlists = ctx.listPlaylists();

    if (!openPlaylist.empty() &&
        std::find(playlists.begin(), playlists.end(), openPlaylist) == playlists.end())
        openPlaylist.clear();

    const float footerH = DS::ButtonHeight + ImGui::GetStyle().ItemSpacing.y * 2.0f + 8.0f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(1.f, 1.f, 1.f, 0.06f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   DS::RadiusMedium);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(12.f, 10.f));

    if (ImGui::BeginChild("PlaylistsChild", { 0.f, -footerH }, true))
    {
        if (openPlaylist.empty())
        {
            if (playlists.empty()) {
                ImVec2 avail = ImGui::GetContentRegionAvail();
                ImGui::SetCursorPos({ std::floor(avail.x * 0.5f - 80.f), std::floor(avail.y * 0.5f - 10.f) });
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.38f, 0.55f, 1.0f));
                ImGui::TextUnformatted("Sin playlists todavia");
                ImGui::PopStyleColor();
            }

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 6.f));
            for (const auto& name : playlists)
            {
                auto songs = ctx.loadPlaylistSongs(name);
                std::string disp = name + "  (" + std::to_string(songs.size()) + ")";
                if (DS::GlassListRow(disp.c_str(), ctx.activePlaylistName == name))
                    openPlaylist = name;

                if (ImGui::BeginPopupContextItem(("##ctx_pl" + name).c_str()))
                {
                    if (ImGui::MenuItem("Renombrar")) {
                        ctx.renameOldName = name;
                        memset(nameBuffer, 0, sizeof(nameBuffer));
                        strncpy(nameBuffer, name.c_str(), sizeof(nameBuffer) - 1);
                        showRenameModal = true;
                    }
                    ImGui::Separator();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.45f, 0.40f, 1.0f));
                    if (ImGui::MenuItem("Eliminar")) {
                        ctx.deletePlaylist(name);
                        if (ctx.activePlaylistName == name) ctx.activePlaylistName.clear();
                    }
                    ImGui::PopStyleColor();
                    ImGui::EndPopup();
                }
            }
            ImGui::PopStyleVar();
        }
        else
        {
            // ── Header: boton volver + titulo + contador ────────────────────
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.60f, 0.80f, 1.0f));
            if (ImGui::SmallButton("< Volver")) openPlaylist.clear();
            ImGui::PopStyleColor();

            ImGui::SameLine(0.f, 10.f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.92f, 1.00f, 1.0f));
            ImGui::TextUnformatted(openPlaylist.c_str());
            ImGui::PopStyleColor();

            auto songs = ctx.loadPlaylistSongs(openPlaylist);
            std::string countLabel = std::to_string(songs.size()) + " canciones";
            float countW = ImGui::CalcTextSize(countLabel.c_str()).x;
            float rightX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - countW;
            if (rightX > ImGui::GetCursorPosX())
                ImGui::SameLine(rightX);
            else
                ImGui::NewLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.48f, 0.62f, 1.0f));
            ImGui::TextUnformatted(countLabel.c_str());
            ImGui::PopStyleColor();

            ImGui::Spacing();
            DS::GlassSeparator();
            ImGui::Spacing();
            ImGui::Spacing();

            // ── Tarjetas de canciones, con controles de orden y borrado ─────
            const float cardH = 54.0f;
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 8.f));

            for (int i = 0; i < (int)songs.size(); i++)
            {
                ImGui::PushID(i);

                bool isActive = (ctx.activePlaylistName == openPlaylist && ctx.activePlaylistIndex == i);

                ImVec2 p_min = ImGui::GetCursorScreenPos();
                float  cardW = ImGui::GetContentRegionAvail().x;
                ImVec2 p_max = { p_min.x + cardW, p_min.y + cardH };

                // Tamaños de los controles y ancho de la zona clickeable de
                // seleccion. IMPORTANTE: el InvisibleButton de seleccion NO
                // debe cubrir la zona de controles (subir/bajar/quitar), o
                // captura el click primero y los botones nunca se activan
                // (ImGui resuelve el mouse-capture por orden de declaracion,
                // no por z-order visual). Por eso el InvisibleButton se
                // limita a "selectW" en vez de ocupar todo "cardW".
                const ImVec2 ctrlSize(28.f, 26.f);
                const float  ctrlGap   = 8.0f;
                const float  ctrlZoneW = 3.0f * ctrlSize.x + 2.0f * ctrlGap + 16.0f;
                const float  selectW   = std::max(cardW - ctrlZoneW, cardW * 0.35f);

                ImGui::SetCursorScreenPos(p_min);
                ImGui::InvisibleButton("##card", { selectW, cardH });
                bool clicked   = ImGui::IsItemClicked();
                bool isHovered = ImGui::IsItemHovered();

                ImDrawList* dl = ImGui::GetWindowDrawList();

                ImU32 bg = isActive ? IM_COL32(20, 55, 30, 255)
                         : isHovered ? IM_COL32(26, 28, 42, 255)
                         : IM_COL32(16, 17, 28, 255);
                ImU32 border = isActive ? IM_COL32(80, 200, 100, 200) : IM_COL32(255, 255, 255, 20);

                dl->AddRectFilled(p_min, p_max, bg, 8.0f);
                dl->AddRect(p_min, p_max, border, 8.0f, 0, isActive ? 1.5f : 1.0f);

                // Divisor sutil entre la zona de titulo/seleccion y la zona
                // de controles, para que se entienda que son dos areas
                // distintas.
                dl->AddLine({ p_min.x + selectW, p_min.y + 8.f },
                            { p_min.x + selectW, p_max.y - 8.f },
                            IM_COL32(255, 255, 255, 16), 1.0f);

                // Numero a la izquierda, en badge redondo
                const float badgeR = 14.0f;
                ImVec2 badgeC = { p_min.x + 26.f, p_min.y + cardH * 0.5f };
                dl->AddCircleFilled(badgeC, badgeR, IM_COL32(40, 44, 68, 255), 16);
                std::string numStr = std::to_string(i + 1);
                ImVec2 numSz = ImGui::CalcTextSize(numStr.c_str());
                dl->AddText({ badgeC.x - numSz.x * 0.5f, badgeC.y - numSz.y * 0.5f },
                            IM_COL32(190, 200, 255, 255), numStr.c_str());

                // Titulo, recortado para no invadir el divisor
                std::string title = StripExtension(songs[i]);
                ImVec2 titlePos = { p_min.x + 50.f, p_min.y + cardH * 0.5f - ImGui::GetTextLineHeight() * 0.5f };
                dl->PushClipRect(p_min, { p_min.x + selectW - 10.f, p_max.y }, true);
                dl->AddText(titlePos, IM_COL32(230, 232, 245, 255), title.c_str());
                dl->PopClipRect();

                if (clicked) ctx.selectPlaylistSong(openPlaylist, i);

                // ── Controles: subir / bajar / quitar ────────────────────────
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ctrlGap, 0.f));
                ImGui::SetCursorScreenPos({ p_min.x + selectW + 10.0f, p_min.y + (cardH - ctrlSize.y) * 0.5f });

                ImGui::BeginDisabled(i == 0);
                if (PillButton("^", ctrlSize, k_BtnNeutral, k_BtnNeutralH, k_BtnNeutralA, k_BtnNeutralT))
                    ctx.movePlaylistSong(openPlaylist, i, -1);
                ImGui::EndDisabled();

                ImGui::SameLine();
                ImGui::BeginDisabled(i == (int)songs.size() - 1);
                if (PillButton("v", ctrlSize, k_BtnNeutral, k_BtnNeutralH, k_BtnNeutralA, k_BtnNeutralT))
                    ctx.movePlaylistSong(openPlaylist, i, 1);
                ImGui::EndDisabled();

                ImGui::SameLine();
                if (PillButton("X", ctrlSize, k_BtnDel, k_BtnDelH, k_BtnDelA, k_BtnDelT))
                    ctx.removeSongFromPlaylist(openPlaylist, i);

                ImGui::PopStyleVar();

                ImGui::SetCursorScreenPos({ p_min.x, p_max.y });
                ImGui::PopID();
            }

            ImGui::PopStyleVar(); // ItemSpacing tarjetas

            if (songs.empty()) {
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.38f, 0.42f, 0.58f, 1.0f));
                ImGui::TextUnformatted("Esta playlist no tiene canciones todavia");
                ImGui::PopStyleColor();
            }

            ImGui::Spacing();
            ImGui::Spacing();
            if (DS::GlassButton("+ Agregar canciones", { -1.f, DS::ButtonHeight }))
                showAddSongsModal = true;
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);

    ImGui::Spacing();

    if (openPlaylist.empty()) {
        if (DS::GlassButton("+ Nueva playlist", { -1.f, DS::ButtonHeight })) {
            memset(nameBuffer, 0, sizeof(nameBuffer));
            showNewModal = true;
        }
    }
   ImGui::Dummy({ 0.f, 12.f }); 
    // ── Modal nueva playlist ──────────────────────────────────────────────
    if (showNewModal) ImGui::OpenPopup("NuevaPlaylistModal##lib");
    if (ImGui::BeginPopupModal("NuevaPlaylistModal##lib", &showNewModal,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::TextUnformatted("Nombre de la playlist");
        ImGui::SetNextItemWidth(320.f);
        ImGui::InputText("##newPlName", nameBuffer, sizeof(nameBuffer));
        ImGui::Spacing();
        if (DS::GlassButton("Crear", { 150.f, 34.f })) {
            std::string nm(nameBuffer);
            if (!nm.empty() && ctx.createPlaylist(nm)) {
                openPlaylist = nm;
                showNewModal = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (DS::GlassButton("Cancelar", { 150.f, 34.f }, DS::TextSecondary)) {
            showNewModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ── Modal renombrar ───────────────────────────────────────────────────
    if (showRenameModal) ImGui::OpenPopup("RenombrarPlaylistModal##lib");
    if (ImGui::BeginPopupModal("RenombrarPlaylistModal##lib", &showRenameModal,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::TextUnformatted("Nuevo nombre");
        ImGui::SetNextItemWidth(320.f);
        ImGui::InputText("##renPlName", nameBuffer, sizeof(nameBuffer));
        ImGui::Spacing();
        if (DS::GlassButton("Renombrar", { 150.f, 34.f })) {
            std::string nm(nameBuffer);
            if (!nm.empty() && ctx.renamePlaylist(ctx.renameOldName, nm)) {
                if (openPlaylist == ctx.renameOldName) openPlaylist = nm;
                if (ctx.activePlaylistName == ctx.renameOldName) ctx.activePlaylistName = nm;
                showRenameModal = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (DS::GlassButton("Cancelar", { 150.f, 34.f }, DS::TextSecondary)) {
            showRenameModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ── Modal agregar canciones ───────────────────────────────────────────
    if (showAddSongsModal) ImGui::OpenPopup("AgregarCancionesModal##lib");
    if (ImGui::BeginPopupModal("AgregarCancionesModal##lib", &showAddSongsModal,
                               ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::SetWindowSize({ 420.f, 480.f }, ImGuiCond_Appearing);
        ImGui::TextUnformatted("Toca una cancion para agregarla");
        ImGui::Separator();

        if (ImGui::BeginChild("AddSongsList", { 0.f, -50.f }))
        {
            auto currentSongs = ctx.loadPlaylistSongs(openPlaylist);
            for (const auto& item : ctx.items)
            {
                bool already = std::find(currentSongs.begin(), currentSongs.end(), item) != currentSongs.end();
                ImGui::PushStyleColor(ImGuiCol_Text, already
                    ? ImVec4(0.4f, 0.7f, 0.4f, 1.0f) : ImVec4(0.85f, 0.87f, 0.95f, 1.0f));
                std::string label = StripExtension(item) + (already ? "  (ya agregada)" : "");
                if (ImGui::Selectable(label.c_str()) && !already)
                    ctx.addSongToPlaylist(openPlaylist, item);
                ImGui::PopStyleColor();
            }
        }
        ImGui::EndChild();

        if (DS::GlassButton("Listo", { -1.f, 34.f })) {
            showAddSongsModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// =============================================================================
//  RenderItemsListPane
//  Lista con scroll + footer de acciones (Nuevo / Importar / Eliminar) para
//  la categoria activa (Canciones, Video, Imagen, Biblia, Documentos,
//  Audio). Se usa tanto para la columna izquierda del grid de Canciones
//  como para el resto de categorias en pantalla completa.
// =============================================================================
static void RenderItemsListPane(LibraryContext& ctx)
{
    const float itemSpY   = ImGui::GetStyle().ItemSpacing.y;
    const float btnRowH   = DS::ButtonHeight;
    const float reservedH = btnRowH + itemSpY * 2.0f + 10.0f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(1.f, 1.f, 1.f, 0.06f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   DS::RadiusMedium);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(10.f, 8.f));

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
                    for (const auto& t : ctx.getSongTags(item)) {
                        std::string tl = t;
                        std::transform(tl.begin(), tl.end(), tl.begin(),
                                       [](unsigned char c){ return (char)::tolower(c); });
                        if (tl.find(cur) != std::string::npos) { match = true; break; }
                    }
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

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 4.f));

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

                for (const auto& t : ctx.getSongTags(filteredItems[n]))
                    disp += "  #" + t;
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
                                pages.push_back(ProyecThor::Library::PathToUtf8(pe.path()));
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
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);

    ImGui::Spacing();

    // ── Footer: Nuevo / Importar / Eliminar, evenly spaced con aire ────────
    {
        const float avail = ImGui::GetContentRegionAvail().x;
        const float sp    = 10.0f;
        const float bw3   = std::floor((avail - sp * 2.0f) / 3.0f);
        const ImVec2 btnSize(bw3, DS::ButtonHeight);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(sp, sp));

        if (GlassIconButton("newSong", "add", "+", "Nuevo", btnSize))
            CreateNewSong(ctx);
        ImGui::SameLine();
        if (GlassIconButton("importSong", "upload_file", "^", "Importar", btnSize))
            ctx.importFile();
        ImGui::SameLine();
        if (GlassIconButton("deleteSong", "delete", "X", "Eliminar", btnSize,
                            ImGui::ColorConvertU32ToFloat4(DS::DangerColor)))
            ctx.deleteSelectedItem();

        ImGui::PopStyleVar();
    }
}

// =============================================================================
//  RenderSongsAndPlaylistsGrid
//  Muestra Canciones y Playlists a la vez, divididas en grid horizontal
//  (columna izquierda / derecha) cuando hay ancho suficiente, o apiladas
//  verticalmente en paneles angostos. Cada columna tiene margen interno
//  propio (WindowPadding) y estan separadas por una linea sutil, para que
//  ninguna de las dos ocupe mas espacio del que corresponde.
// =============================================================================
static void RenderSongsAndPlaylistsGrid(LibraryContext& ctx)
{
    const float totalW = ImGui::GetContentRegionAvail().x;
    const float totalH = ImGui::GetContentRegionAvail().y;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.f));

    constexpr float kMinWidthForColumns = 340.0f;

    if (totalW >= kMinWidthForColumns)
    {
        // ── Layout horizontal: Canciones | Playlists ─────────────────────────
        const float gap    = 18.0f;
        const float leftW  = std::floor((totalW - gap) * 0.56f);
        const float rightW = totalW - gap - leftW;

        ImGui::BeginChild("SongsPane", ImVec2(leftW, totalH), false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        RenderPaneHeader("Canciones", (int)ctx.items.size());
        RenderItemsListPane(ctx);
        ImGui::EndChild();

        ImGui::SameLine(0.f, 0.f);
        {
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(p0.x + gap * 0.5f, p0.y + 4.f),
                ImVec2(p0.x + gap * 0.5f, p0.y + totalH - 4.f),
                IM_COL32(255, 255, 255, 18), 1.0f);
            ImGui::Dummy(ImVec2(gap, totalH));
        }
        ImGui::SameLine(0.f, 0.f);

        ImGui::BeginChild("PlaylistsPane", ImVec2(rightW, totalH), false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        RenderPaneHeader("Playlists", (int)ctx.listPlaylists().size());
        RenderPlaylistsSection(ctx);
        ImGui::EndChild();
    }
    else
    {
        // ── Layout vertical: Canciones arriba, Playlists abajo ───────────────
        const float gap  = 14.0f;
        const float topH = std::floor((totalH - gap) * 0.55f);
        const float botH = totalH - gap - topH;

        ImGui::BeginChild("SongsPaneV", ImVec2(0.f, topH), false);
        RenderPaneHeader("Canciones", (int)ctx.items.size());
        RenderItemsListPane(ctx);
        ImGui::EndChild();

        {
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            float  w  = ImGui::GetContentRegionAvail().x;
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(p0.x + 4.f,     p0.y + gap * 0.5f),
                ImVec2(p0.x + w - 4.f, p0.y + gap * 0.5f),
                IM_COL32(255, 255, 255, 18), 1.0f);
            ImGui::Dummy(ImVec2(w, gap));
        }

        ImGui::BeginChild("PlaylistsPaneV", ImVec2(0.f, botH), false);
        RenderPaneHeader("Playlists", (int)ctx.listPlaylists().size());
        RenderPlaylistsSection(ctx);
        ImGui::EndChild();
    }

    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
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
//  Todo el contenido vive dentro de un contenedor con padding parejo en
//  los 4 lados (izquierda/derecha/arriba/abajo), para que el buscador, el
//  combo, la grilla y los botones no queden pegados a los bordes del panel.
// =============================================================================
void RenderSideList(LibraryContext& ctx)
{
    const auto& str = ProyecThor::UI::GetUIStrings();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.f, 14.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(8.f, 8.f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::BeginChild("LibraryPad", ImGui::GetContentRegionAvail(), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

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

    // ── Estilo por defecto (si aplica) + boton de actualizar en la esquina ──
    RenderToolbarRow(ctx);

    ImGui::Spacing();

    // ── Canciones: se muestran junto a Playlists en un grid, sin pestañas ───
    if (ctx.currentCategoryInt == kCat_Songs)
        RenderSongsAndPlaylistsGrid(ctx);
    else
        RenderItemsListPane(ctx);

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
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