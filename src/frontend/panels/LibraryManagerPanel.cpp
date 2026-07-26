#include <GL/glew.h>
#include "LibraryManagerPanel.h"
#include "biblio/LibraryHelpers.h"
#include "frontend/views/audio/AudioHelpers.h"
#include "AppIcons.h"
#include "IconRail.h"
#include "../external/tools/OpenURL.h"
#include <imgui.h>
#include <algorithm>
#include <filesystem>

extern GLuint LoadTextureFromFile(const char* filename);

namespace fs = std::filesystem;

namespace ProyecThor::UI {

using namespace ProyecThor::Library;

LibraryManagerPanel::~LibraryManagerPanel() {
    ClearThumbCache();
}

void LibraryManagerPanel::ClearThumbCache() {
    for (auto& kv : m_ImageThumbCache) {
        GLuint tex = (GLuint)kv.second;
        if (tex) glDeleteTextures(1, &tex);
    }
    m_ImageThumbCache.clear();
}

std::string LibraryManagerPanel::FolderFor(AssetKind kind) const {
    switch (kind) {
        case AssetKind::Video:  return GetAssetsPath() + "/videos/";
        case AssetKind::Image:  return GetAssetsPath() + "/images/";
        case AssetKind::Audio:  return ProyecThor::Audio::GetAudioPath() + "/";
        case AssetKind::Render: return ""; // no es una carpeta de assets, ver RenderConverterSection
    }
    return "";
}

std::vector<std::string> LibraryManagerPanel::ExtensionsFor(AssetKind kind) const {
    switch (kind) {
        case AssetKind::Video:  return { ".mp4", ".mkv", ".avi", ".mov" };
        case AssetKind::Image:  return { ".jpg", ".jpeg", ".png" };
        case AssetKind::Audio:  return { ".mp3", ".flac", ".wav", ".ogg", ".aac", ".m4a", ".wma", ".opus", ".aiff" };
        case AssetKind::Render: return {};
    }
    return {};
}

const char* LibraryManagerPanel::LabelFor(AssetKind kind) const {
    switch (kind) {
        case AssetKind::Video:  return "VID";
        case AssetKind::Image:  return "IMG";
        case AssetKind::Audio:  return "AUD";
        case AssetKind::Render: return "";
    }
    return "";
}

void LibraryManagerPanel::RefreshItems() {
    if (m_Kind != m_LoadedKind) ClearThumbCache();

    m_Items.clear();
    m_SelectedIndex = -1;

    auto      exts = ExtensionsFor(m_Kind);
    fs::path  dir  = U8Path(FolderFor(m_Kind));

    std::error_code ec;
    if (fs::exists(dir, ec) && !ec) {
        for (auto& entry : fs::directory_iterator(dir, ec)) {
            if (ec) break;
            if (!entry.is_regular_file()) continue;

            std::string lowExt = entry.path().extension().string();
            std::transform(lowExt.begin(), lowExt.end(), lowExt.begin(),
                            [](unsigned char c) { return (char)std::tolower(c); });
            if (std::find(exts.begin(), exts.end(), lowExt) == exts.end()) continue;

            m_Items.push_back(PathToUtf8(entry.path().filename()));
        }
    }
    std::sort(m_Items.begin(), m_Items.end());

    m_LoadedKind    = m_Kind;
    m_NeedsRefresh  = false;
}

void LibraryManagerPanel::RenderRail() {
    static const IconRailItem kItems[] = {
        { (int)AssetKind::Video,  AppIcons::DrawIcon_Monitor, "Video"  },
        { (int)AssetKind::Image,  AppIcons::DrawIcon_Layers,  "Imagen" },
        { (int)AssetKind::Audio,  AppIcons::DrawIcon_Mixer,   "Audio"  },
        { (int)AssetKind::Render, AppIcons::DrawIcon_Swap,    "Render" },
    };
    static const float kColors[4][4] = {
        { 0.86f, 0.24f, 0.24f, 1.0f }, // Video
        { 0.24f, 0.86f, 0.39f, 1.0f }, // Imagen
        { 0.16f, 0.75f, 0.75f, 1.0f }, // Audio
        { 0.90f, 0.55f, 0.20f, 1.0f }, // Render
    };

    float railW = IconRailThickness(true);
    ImGui::BeginChild("##libManagerRail", ImVec2(railW, 0.0f), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    int currentIndex = (int)m_Kind;
    RenderIconRail(kItems, 4, currentIndex, IconRailOrientation::Vertical, kColors);
    AssetKind newKind = (AssetKind)currentIndex;
    if (newKind != m_Kind) {
        m_Kind         = newKind;
        m_NeedsRefresh = true;
    }

    ImGui::EndChild();
}

void LibraryManagerPanel::RenderCard(int index, float w, float h) {
    const std::string& filename = m_Items[index];
    std::string fullPath = FolderFor(m_Kind) + filename;

    ImGui::PushID(index);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool sel = (m_SelectedIndex == index);

    dl->AddRectFilled(pos, { pos.x + w, pos.y + h }, IM_COL32(28, 30, 40, 255), 8.0f);
    dl->AddRect(pos, { pos.x + w, pos.y + h },
                sel ? IM_COL32(120, 150, 255, 255) : IM_COL32(60, 63, 80, 255), 8.0f, 0, sel ? 2.0f : 1.0f);

    const float thumbH = h - 34.0f;
    if (m_Kind == AssetKind::Image) {
        auto it = m_ImageThumbCache.find(filename);
        if (it == m_ImageThumbCache.end()) {
            GLuint tex = LoadTextureFromFile(fullPath.c_str());
            it = m_ImageThumbCache.emplace(filename, (unsigned int)tex).first;
        }
        GLuint tex = (GLuint)it->second;
        if (tex) {
            dl->AddImageRounded((ImTextureID)(intptr_t)tex, { pos.x + 4, pos.y + 4 },
                                { pos.x + w - 4, pos.y + thumbH }, {0,0}, {1,1},
                                IM_COL32_WHITE, 6.0f, ImDrawFlags_RoundCornersTop);
        }
    } else {
        ImVec2 ts = ImGui::CalcTextSize(LabelFor(m_Kind));
        ImVec2 c  = { pos.x + w * 0.5f, pos.y + 4 + thumbH * 0.5f };
        dl->AddText({ c.x - ts.x * 0.5f, c.y - ts.y * 0.5f }, IM_COL32(150, 155, 175, 255), LabelFor(m_Kind));
    }

    std::string dn = filename.length() > 20 ? filename.substr(0, 17) + "..." : filename;
    ImVec2 ns = ImGui::CalcTextSize(dn.c_str());
    dl->AddText({ pos.x + (w - ns.x) * 0.5f, pos.y + h - 22.0f }, IM_COL32(220, 222, 230, 255), dn.c_str());

    ImGui::InvisibleButton("##card", { w, h });
    if (ImGui::IsItemClicked()) m_SelectedIndex = index;

    if (ImGui::BeginPopupContextItem("##cardCtx")) {
        if (ImGui::MenuItem("Abrir")) {
            ProyecThor::External::OpenURL(fullPath);
        }
        if (ImGui::MenuItem("Renombrar")) {
            m_ShowRenameModal = true;
            m_RenameIndex     = index;
            m_RenameBuffer    = StripExtension(filename);
            SplitExtension(filename, m_RenameExt);
        }
        if (ImGui::MenuItem("Borrar")) {
            m_ShowDeleteModal = true;
            m_DeleteIndex     = index;
        }
        ImGui::EndPopup();
    }

    ImGui::PopID();
}

void LibraryManagerPanel::RenderGrid() {
    if (m_Kind == AssetKind::Render) {
        RenderConverterSection();
        return;
    }

    if (m_NeedsRefresh || m_Kind != m_LoadedKind) RefreshItems();

    if (!m_StatusMessage.empty()) {
        ImGui::TextColored(m_StatusIsError ? ImVec4(0.90f, 0.35f, 0.35f, 1.0f) : ImVec4(0.40f, 0.85f, 0.55f, 1.0f),
                            "%s", m_StatusMessage.c_str());
        ImGui::Spacing();
    }

    if (m_Items.empty()) {
        ImGui::TextDisabled("No hay archivos importados en esta categoria todavia.");
        return;
    }

    ImGui::TextDisabled("Click derecho sobre un elemento para Abrir / Renombrar / Borrar.");
    ImGui::Spacing();

    const float cardW = 160.0f, cardH = 140.0f, spacing = 12.0f;
    float avail = ImGui::GetContentRegionAvail().x;
    int   cols  = std::max(1, (int)((avail + spacing) / (cardW + spacing)));

    for (int i = 0; i < (int)m_Items.size(); i++) {
        RenderCard(i, cardW, cardH);
        if ((i + 1) % cols != 0 && i + 1 < (int)m_Items.size())
            ImGui::SameLine(0.0f, spacing);
    }
}

void LibraryManagerPanel::RenderRenameModal() {
    if (m_ShowRenameModal) ImGui::OpenPopup("Renombrar##libManager");

    if (ImGui::BeginPopupModal("Renombrar##libManager", &m_ShowRenameModal, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Nuevo nombre:");
        ImGui::SetNextItemWidth(280.0f);

        char buf[256];
        std::snprintf(buf, sizeof(buf), "%s", m_RenameBuffer.c_str());
        if (ImGui::InputText("##renameInput", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
            m_RenameBuffer = buf;
            ImGui::CloseCurrentPopup();
        } else {
            m_RenameBuffer = buf;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", m_RenameExt.c_str());

        ImGui::Spacing();
        if (ImGui::Button("Renombrar", ImVec2(120, 0)) && m_RenameIndex >= 0 &&
            m_RenameIndex < (int)m_Items.size() && !m_RenameBuffer.empty()) {

            std::string oldName = m_Items[m_RenameIndex];
            std::string newName = m_RenameBuffer + m_RenameExt;
            fs::path    dir     = U8Path(FolderFor(m_Kind));
            std::error_code ec;
            fs::rename(dir / U8Path(oldName), dir / U8Path(newName), ec);

            m_StatusIsError  = (bool)ec;
            m_StatusMessage  = ec ? ("No se pudo renombrar: " + ec.message())
                                   : ("Renombrado a \"" + newName + "\".");
            m_NeedsRefresh   = true;
            m_ShowRenameModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(120, 0))) {
            m_ShowRenameModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void LibraryManagerPanel::RenderDeleteModal() {
    if (m_ShowDeleteModal) ImGui::OpenPopup("Borrar archivo##libManager");

    if (ImGui::BeginPopupModal("Borrar archivo##libManager", &m_ShowDeleteModal, ImGuiWindowFlags_AlwaysAutoResize)) {
        std::string target = (m_DeleteIndex >= 0 && m_DeleteIndex < (int)m_Items.size())
                                ? m_Items[m_DeleteIndex] : "";
        ImGui::TextWrapped("Esta accion borra el archivo del disco y no se puede deshacer.");
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.96f, 0.75f, 0.30f, 1.0f), "%s", target.c_str());
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.20f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.28f, 0.28f, 1.0f));
        if (ImGui::Button("Borrar definitivamente", ImVec2(200, 0)) && m_DeleteIndex >= 0 &&
            m_DeleteIndex < (int)m_Items.size()) {

            fs::path dir = U8Path(FolderFor(m_Kind));
            std::error_code ec;
            fs::remove(dir / U8Path(target), ec);

            m_StatusIsError   = (bool)ec;
            m_StatusMessage   = ec ? ("No se pudo borrar: " + ec.message())
                                    : ("\"" + target + "\" borrado.");
            m_ImageThumbCache.erase(target);
            m_NeedsRefresh    = true;
            m_ShowDeleteModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(120, 0))) {
            m_ShowDeleteModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void LibraryManagerPanel::RefreshConvertibleItems() {
    m_ConvertibleItems.clear();

    auto scan = [&](AssetKind kind, bool isVideo) {
        auto     exts = ExtensionsFor(kind);
        fs::path dir  = U8Path(FolderFor(kind));
        std::error_code ec;
        if (!fs::exists(dir, ec) || ec) return;

        for (auto& entry : fs::directory_iterator(dir, ec)) {
            if (ec) break;
            if (!entry.is_regular_file()) continue;

            std::string lowExt = entry.path().extension().string();
            std::transform(lowExt.begin(), lowExt.end(), lowExt.begin(),
                            [](unsigned char c) { return (char)std::tolower(c); });
            if (std::find(exts.begin(), exts.end(), lowExt) == exts.end()) continue;

            m_ConvertibleItems.push_back({ PathToUtf8(entry.path().filename()), isVideo });
        }
    };
    scan(AssetKind::Video, true);
    scan(AssetKind::Audio, false);

    std::sort(m_ConvertibleItems.begin(), m_ConvertibleItems.end(),
              [](const ConvertibleItem& a, const ConvertibleItem& b) { return a.filename < b.filename; });

    if (m_ConvertSourceIndex >= (int)m_ConvertibleItems.size()) m_ConvertSourceIndex = -1;
    m_ConvertibleNeedsRefresh = false;
}

void LibraryManagerPanel::RenderConverterSection() {
    if (m_ConvertibleNeedsRefresh) RefreshConvertibleItems();

    ImGui::TextUnformatted("Render");
    ImGui::SameLine();
    ImGui::TextDisabled("(convertir Video/Audio ya importados a otro formato)");
    ImGui::Spacing();

    // Si termino una conversion desde el ultimo frame, actualizar estado.
    bool        finishedOk = false;
    std::string finishedMsg;
    if (m_Converter.PollFinished(finishedOk, finishedMsg)) {
        m_ConvertStatusIsError = !finishedOk;
        m_ConvertStatus        = finishedMsg;
        m_ConvertibleNeedsRefresh = true; // por si el archivo convertido cae en la misma carpeta
    }

    if (m_ConvertibleItems.empty()) {
        ImGui::TextDisabled("Todavia no importaste ningun Video o Audio para convertir.");
        return;
    }

    bool running = m_Converter.IsRunning();
    if (running) ImGui::BeginDisabled();

    // ── Origen ────────────────────────────────────────────────────────────
    std::string sourcePreview = (m_ConvertSourceIndex >= 0 && m_ConvertSourceIndex < (int)m_ConvertibleItems.size())
        ? m_ConvertibleItems[m_ConvertSourceIndex].filename : "Elegi un archivo...";

    ImGui::SetNextItemWidth(360.0f);
    if (ImGui::BeginCombo("Archivo de origen", sourcePreview.c_str())) {
        for (int i = 0; i < (int)m_ConvertibleItems.size(); i++) {
            const auto& item = m_ConvertibleItems[i];
            std::string label = std::string(item.isVideo ? "[Video] " : "[Audio] ") + item.filename;
            bool sel = (i == m_ConvertSourceIndex);
            if (ImGui::Selectable(label.c_str(), sel)) {
                m_ConvertSourceIndex = i;
                m_ConvertFormatIndex = 0;
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    // ── Formato de destino ───────────────────────────────────────────────
    static const char* kVideoFormats[] = { "mp4", "mkv", "webm", "avi", "mov" };
    static const char* kAudioFormats[] = { "mp3", "wav", "flac", "ogg", "aac", "m4a" };

    const char** formats     = kVideoFormats;
    int          formatCount = (int)(sizeof(kVideoFormats) / sizeof(kVideoFormats[0]));
    bool         haveSource  = (m_ConvertSourceIndex >= 0 && m_ConvertSourceIndex < (int)m_ConvertibleItems.size());
    if (haveSource && !m_ConvertibleItems[m_ConvertSourceIndex].isVideo) {
        formats     = kAudioFormats;
        formatCount = (int)(sizeof(kAudioFormats) / sizeof(kAudioFormats[0]));
    }
    if (m_ConvertFormatIndex >= formatCount) m_ConvertFormatIndex = 0;

    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::BeginCombo("Formato de destino", haveSource ? formats[m_ConvertFormatIndex] : "-")) {
        for (int i = 0; i < formatCount; i++) {
            bool sel = (i == m_ConvertFormatIndex);
            if (ImGui::Selectable(formats[i], sel)) m_ConvertFormatIndex = i;
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (running) ImGui::EndDisabled();

    ImGui::Spacing();
    if (!m_ConvertStatus.empty()) {
        ImGui::TextColored(m_ConvertStatusIsError ? ImVec4(0.90f, 0.35f, 0.35f, 1.0f) : ImVec4(0.40f, 0.85f, 0.55f, 1.0f),
                            "%s", m_ConvertStatus.c_str());
        ImGui::Spacing();
    }

    if (running) {
        ImGui::TextColored(ImVec4(0.96f, 0.75f, 0.30f, 1.0f), "Convirtiendo...");
        return;
    }

    if (!haveSource) { ImGui::BeginDisabled(); }
    if (ImGui::Button("Convertir", ImVec2(160, 36)) && haveSource) {
        const auto& src     = m_ConvertibleItems[m_ConvertSourceIndex];
        AssetKind   srcKind = src.isVideo ? AssetKind::Video : AssetKind::Audio;
        fs::path    dir     = U8Path(FolderFor(srcKind));
        std::string stem    = StripExtension(src.filename);
        std::string ext     = formats[m_ConvertFormatIndex];

        // Nombre de salida unico -- nunca pisa un archivo existente (mismo
        // criterio que LibraryPanel::CreateNewSong).
        std::string outName = stem + "." + ext;
        int suffix = 2;
        std::error_code ec;
        while (fs::exists(dir / U8Path(outName), ec)) {
            outName = stem + " (" + std::to_string(suffix) + ")." + ext;
            suffix++;
        }

        std::string inputPath  = FolderFor(srcKind) + src.filename;
        std::string outputPath = FolderFor(srcKind) + outName;

        std::string err;
        if (m_Converter.Start(inputPath, outputPath, &err)) {
            m_ConvertStatusIsError = false;
            m_ConvertStatus        = "Convirtiendo a " + outName + "...";
        } else {
            m_ConvertStatusIsError = true;
            m_ConvertStatus        = err;
        }
    }
    if (!haveSource) { ImGui::EndDisabled(); }
}

void LibraryManagerPanel::Render() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove       |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("##LibraryManagerRoot", nullptr, flags);
    ImGui::BeginChild("##libManagerContent", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AlwaysUseWindowPadding);

    ImGui::TextUnformatted("Biblioteca");
    ImGui::SameLine();
    ImGui::TextDisabled("(ver y gestionar tus archivos -- esto no proyecta nada)");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    RenderRail();
    ImGui::SameLine();
    ImGui::BeginChild("##libManagerGrid", ImVec2(0.0f, 0.0f));
    RenderGrid();
    ImGui::EndChild();

    RenderRenameModal();
    RenderDeleteModal();

    ImGui::EndChild();
    ImGui::End();
}

} // namespace ProyecThor::UI
