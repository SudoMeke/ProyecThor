#include "LibraryVideos.h"
#include "LibraryIcons.h"
#include "LibraryStyles.h"
#include "LibraryHelpers.h"
#include "ui/DesignSystem.h"
#include "frontend/ui/bin/StyleGeneralApp.h"

#include <imgui.h>
#include <imgui_internal.h>
#include "backend/core/PresentationCore.h"
#include "monitor/MonitorView.h"

#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cmath>

namespace DS = ProyecThor::UI::DS;

namespace ProyecThor::Library {

// =============================================================================
//  GlassIconButton — boton con icono de StyleGeneralApp (fallback a glifo corto)
//  Mismo helper que en LibrarySongs.cpp: reemplaza texto largo ("Importar",
//  "Actualizar", "Eliminar", etc.) por iconos + tooltip.
//
//  FIX (tamaños): antes el icono se recortaba con el mismo "pad" en X e Y,
//  lo que en botones anchos y bajos (como los del footer, ancho/3) dejaba
//  un rectangulo horizontal en vez de un icono cuadrado -> se veia
//  "estirado"/deforme. Ahora se calcula un cuadrado a partir del lado MENOR
//  del boton y se centra, sin importar que tan ancho o bajo sea el boton.
// =============================================================================
static bool GlassIconButton(const char* id,
                             const char* iconKey,
                             const char* fallbackGlyph,
                             const char* tooltip,
                             ImVec2      size,
                             ImVec4      tint = ImGui::ColorConvertU32ToFloat4(DS::TextPrimary))
{
    // Exactamente 4 PushStyleColor
    ImGui::PushStyleColor(ImGuiCol_Button,        ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImGui::ColorConvertU32ToFloat4(DS::AccentColor));
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

    // Exactamente 4 PopStyleColor (y se eliminó el PopStyleVar huérfano)
    ImGui::PopStyleColor(4);

    if (tooltip && ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tooltip);

    return clicked;
}

// =============================================================================
//  RenderVideoSection — tabs Archivos / Stream
// =============================================================================
void RenderVideoSection(LibraryContext& ctx)
{
    static constexpr ImVec4 k_Tab      = { 0.06f, 0.06f, 0.11f, 1.00f };
    static constexpr ImVec4 k_TabHov   = { 0.12f, 0.14f, 0.24f, 1.00f };
    static constexpr ImVec4 k_TabSel   = { 0.14f, 0.24f, 0.60f, 1.00f };
    static constexpr ImVec4 k_TabSelTx = { 0.80f, 0.90f, 1.00f, 1.00f };
    static constexpr ImVec4 k_TabTx    = { 0.44f, 0.48f, 0.62f, 1.00f };

    ImGui::PushStyleColor(ImGuiCol_Tab,                k_Tab);
    ImGui::PushStyleColor(ImGuiCol_TabHovered,         k_TabHov);
    ImGui::PushStyleColor(ImGuiCol_TabActive,          k_TabSel);
    ImGui::PushStyleColor(ImGuiCol_TabUnfocused,       k_Tab);
    ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, k_TabSel);
    ImGui::PushStyleVar(ImGuiStyleVar_TabRounding,  8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.f, 5.f));

    if (ImGui::BeginTabBar("##video_tabs"))
    {
        bool tab1Active = (ImGui::GetCurrentTabBar()->SelectedTabId
                           == ImGui::GetID("Archivos##vt"));
        ImGui::PushStyleColor(ImGuiCol_Text, tab1Active ? k_TabSelTx : k_TabTx);
        if (ImGui::BeginTabItem("Archivos##vt")) {
            ImGui::PopStyleColor();
            ImGui::Spacing();
            RenderLocalVideoList(ctx);
            ImGui::EndTabItem();
        } else { ImGui::PopStyleColor(); }

        ImGui::PushStyleColor(ImGuiCol_Text, k_TabTx);
        if (ImGui::BeginTabItem("Stream##vt")) {
            ImGui::PopStyleColor();
            ImGui::Spacing();
            RenderStreamURLSection(ctx);
            ImGui::EndTabItem();
        } else { ImGui::PopStyleColor(); }

        ImGui::EndTabBar();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(5);
}

// =============================================================================
//  RenderLocalVideoList — mismo lenguaje glass que RenderSideList (canciones)
// =============================================================================
void RenderLocalVideoList(LibraryContext& ctx)
{
    // ── Barra de búsqueda ──────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImGui::ColorConvertU32ToFloat4(DS::AccentColorDim));
    ImGui::PushStyleColor(ImGuiCol_Border,         ImVec4(1.00f, 1.00f, 1.00f, 0.12f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(10.f, 7.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::SetNextItemWidth(-1.f);
    if (ImGui::InputTextWithHint("##vsearch", "Buscar video...",
                                 ctx.searchBuffer, ctx.searchBufferSize))
        ForceListUpdate() = true;
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(4);

    ImGui::Spacing();

    // ── Espacio reservado para el footer (solo iconos: Importar | Actualizar | Eliminar) ─
    const float itemSpY   = ImGui::GetStyle().ItemSpacing.y;
    const float reservedH = DS::ButtonHeight + itemSpY * 2.0f + 6.0f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(1.f, 1.f, 1.f, 0.06f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   DS::RadiusMedium);

    if (ImGui::BeginChild("##local_vid", { 0.f, -reservedH }, true))
    {
        static std::vector<std::string> filtered;
        static std::string lastQ;
        std::string cur(ctx.searchBuffer);
        std::transform(cur.begin(), cur.end(), cur.begin(),
                       [](unsigned char c){ return (char)::tolower(c); });

        if (cur != lastQ || ForceListUpdate()) {
            filtered.clear();
            for (const auto& item : ctx.items) {
                std::string lo = item;
                std::transform(lo.begin(), lo.end(), lo.begin(),
                               [](unsigned char c){ return (char)::tolower(c); });
                if (cur.empty() || lo.find(cur) != std::string::npos)
                    filtered.push_back(item);
            }
            lastQ             = cur;
            ForceListUpdate() = false;
        }

        if (filtered.empty()) {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            ImGui::SetCursorPos({
                std::floor(avail.x * 0.5f - 70.f),
                std::floor(avail.y * 0.5f - 10.f) });
            ImGui::PushStyleColor(ImGuiCol_Text, DS::TextSecondary);
            ImGui::TextUnformatted("Sin archivos de video");
            ImGui::PopStyleColor();
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 1.f));
        bool deletedInLoop = false;

        for (int n = 0; n < (int)filtered.size(); n++)
        {
            auto it2 = std::find(ctx.items.begin(), ctx.items.end(), filtered[n]);
            int origIdx = (it2 != ctx.items.end())
                ? (int)std::distance(ctx.items.begin(), it2) : -1;

            const bool  sel  = (ctx.selectedIndex == origIdx);
            std::string disp = StripExtension(filtered[n]);

            bool clicked = DS::GlassListRow(disp.c_str(), sel);

            if (clicked && origIdx >= 0)
            {
                ctx.selectedIndex = origIdx;
                Core::LibrarySelection s;
                s.title = filtered[n];
                s.type  = Core::ItemType::Video;
                Core::PresentationCore::Get().SetSelection(s);
            }

            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                std::string fullPath = GetAssetsPath() + "/videos/" + filtered[n];
                ImGui::SetDragDropPayload("VIDEO_TO_QUEUE",
                    fullPath.c_str(), fullPath.size() + 1);
                ImGui::PushStyleColor(ImGuiCol_Text, DS::SuccessColor);
                ImGui::TextUnformatted(disp.c_str());
                ImGui::PopStyleColor();
                ImGui::EndDragDropSource();
            }

            if (ImGui::BeginPopupContextItem(("##ctx_lv" + std::to_string(n)).c_str()))
            {
                if (ImGui::MenuItem("Enviar al monitor")) {
                    std::string fp = GetAssetsPath() + "/videos/" + filtered[n];
                    // FIX: no forzar un Stop() (corte a negro) antes de
                    // SetBackgroundMedia() — SetVideo() ya maneja tanto la
                    // carga en frio como el crossfade sobre lo que esta al
                    // aire. El Stop() previo ademas rompia el guard de
                    // reentrancia de VLCBasePlayer::Play() en clicks
                    // repetidos sobre el mismo video.
                    Core::PresentationCore::Get().SetBackgroundMedia(fp, true, /*allowAudio=*/true);
                    Core::PresentationCore::Get().SetProjecting(true);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Renombrar")) {
                    ctx.renameOldName  = filtered[n];
                    ctx.renameIsURL    = false;
                    ctx.renameURLIndex = -1;
                    ctx.selectedIndex  = origIdx;
                    std::string stem = SplitExtension(filtered[n], ctx.renameExtension);
                    memset(ctx.renameBuffer, 0, 512);
                    strncpy(ctx.renameBuffer, stem.c_str(), 511);
                    ctx.showRenameModal = true;
                }
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, DS::DangerColor);
                if (ImGui::MenuItem("Eliminar")) {
                    ImGui::PopStyleColor();
                    ctx.selectedIndex = origIdx;
                    ImGui::EndPopup();
                    ctx.deleteSelectedItem();
                    ForceListUpdate() = true; // evita que 'filtered' quede con un item ya borrado
                    deletedInLoop = true;
                    break;
                }
                ImGui::PopStyleColor();
                ImGui::EndPopup();
            }

            if (deletedInLoop) break;
        }

        ImGui::PopStyleVar(); // ItemSpacing
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    ImGui::Spacing();

    // ── Footer con botones — solo iconos, universales, con tooltip ─────────
    {
        const float avail = ImGui::GetContentRegionAvail().x;
        const float sp    = ImGui::GetStyle().ItemSpacing.x;
        const float bw3   = std::floor((avail - sp * 2.0f) / 3.0f);
        const ImVec2 btnSize(bw3, DS::ButtonHeight);

        if (GlassIconButton("importVid", "upload_file", "^", "Importar", btnSize)) {
            ctx.importFile();
            ForceListUpdate() = true;
        }
        ImGui::SameLine();
        // NOTA: no existe "refresh.png" en assets/icons/ui, se usa "repeat"
        // (icono ciclico, ya cargado) que visualmente cumple la misma funcion.
        if (GlassIconButton("refreshVid", "repeat", "R", "Actualizar", btnSize)) {
            ctx.refreshList();
            ForceListUpdate() = true;
        }
        ImGui::SameLine();

        // Conversión segura de ImU32 a ImVec4 para evitar el error de tipos en los parámetros
        if (GlassIconButton("deleteVid", "delete", "X", "Eliminar", btnSize, ImGui::ColorConvertU32ToFloat4(DS::DangerColor))) {
            ctx.deleteSelectedItem();
            ForceListUpdate() = true;
        }
    }
}

// =============================================================================
//  RenderStreamURLSection — mismo lenguaje glass
// =============================================================================
void RenderStreamURLSection(LibraryContext& ctx)
{
    // PushStyleColor también acepta ImU32 directamente, o lo convertimos por seguridad
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::DangerColor));
    ImGui::TextWrapped("Pega una URL de YouTube, Twitch o cualquier stream HTTP/RTSP.");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImGui::ColorConvertU32ToFloat4(DS::AccentColorDim));
    ImGui::PushStyleColor(ImGuiCol_Border,         ImVec4(1.00f, 1.00f, 1.00f, 0.12f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(10.f, 7.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::SetNextItemWidth(-1.f);
    bool pressEnter = ImGui::InputTextWithHint(
        "##url_in", "https://www.youtube.com/watch?v=...",
        ctx.urlInputBuffer, ctx.urlInputBufferSize,
        ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(4);

    ImGui::Spacing();

    {
        const float avail = ImGui::GetContentRegionAvail().x;
        const float sp    = ImGui::GetStyle().ItemSpacing.x;
        const float bw2   = std::floor((avail - sp) * 0.5f);
        const ImVec2 btnSize(bw2, DS::ButtonHeight);

        bool doAdd = pressEnter ||
            GlassIconButton("addUrl", "add", "+", "Agregar URL", btnSize);

        if (doAdd) {
            std::string url(ctx.urlInputBuffer);
            if (!url.empty() && url.rfind("http", 0) == 0) {
                if (std::find(ctx.streamURLs.begin(), ctx.streamURLs.end(), url)
                    == ctx.streamURLs.end()) {
                    ctx.streamURLs.push_back(url);
                    ctx.saveStreamURLs();
                }
                memset(ctx.urlInputBuffer, 0, ctx.urlInputBufferSize);
            }
        }
        ImGui::SameLine();

        if (GlassIconButton("delUrl", "delete", "X", "Eliminar URL", btnSize, ImGui::ColorConvertU32ToFloat4(DS::DangerColor)))
        {
            if (ctx.selectedURLIndex >= 0 &&
                ctx.selectedURLIndex < (int)ctx.streamURLs.size()) {
                ctx.streamURLs.erase(ctx.streamURLs.begin() + ctx.selectedURLIndex);
                ctx.selectedURLIndex = -1;
                ctx.saveStreamURLs();
            }
        }
    }

    ImGui::Spacing();
    DS::GlassSeparator();
    ImGui::Spacing();

    float listH = std::clamp(
        (float)ctx.streamURLs.size() * DS::RowHeight + 14.f,
        40.f, 200.f);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(1.f, 1.f, 1.f, 0.06f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   DS::RadiusMedium);

    if (ImGui::BeginChild("##url_list", { 0.f, listH }, true))
    {
        if (ctx.streamURLs.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, DS::TextSecondary);
            ImGui::TextUnformatted("  Sin URLs guardadas");
            ImGui::PopStyleColor();
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 1.f));

        for (int i = 0; i < (int)ctx.streamURLs.size(); i++)
        {
            bool sel = (ctx.selectedURLIndex == i);
            std::string disp = TruncURL(ctx.streamURLs[i]);

            bool clicked = DS::GlassListRow(disp.c_str(), sel);

            if (clicked)
            {
                ctx.selectedURLIndex = i;
                Core::LibrarySelection s;
                s.title = ctx.streamURLs[i];
                s.type  = Core::ItemType::Video;
                Core::PresentationCore::Get().SetSelection(s);

                if (ImGui::IsMouseDoubleClicked(0)) {
                    // FIX: sin Stop() previo (corte a negro) — SetVideo()
                    // ya maneja carga en frio o crossfade, y el guard de
                    // reentrancia en Play() necesita que no se le limpie
                    // la ruta actual en cada click repetido.
                    Core::PresentationCore::Get().SetBackgroundMedia(ctx.streamURLs[i], true, /*allowAudio=*/true);
                    Core::PresentationCore::Get().SetProjecting(true);
                }
            }

            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                const std::string& url = ctx.streamURLs[i];
                ImGui::SetDragDropPayload("URL_TO_QUEUE", url.c_str(), url.size() + 1);
                ImGui::PushStyleColor(ImGuiCol_Text, DS::SuccessColor);
                ImGui::TextUnformatted(TruncURL(url, 38).c_str());
                ImGui::PopStyleColor();
                ImGui::EndDragDropSource();
            }

            if (ImGui::BeginPopupContextItem(("##ctx_url" + std::to_string(i)).c_str()))
            {
                if (ImGui::MenuItem("Enviar al monitor")) {
                    Core::PresentationCore::Get().SetBackgroundMedia(ctx.streamURLs[i], true, /*allowAudio=*/true);
                    Core::PresentationCore::Get().SetProjecting(true);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Renombrar / Editar URL")) {
                    ctx.renameOldName  = ctx.streamURLs[i];
                    ctx.renameURLIndex = i;
                    ctx.renameIsURL    = true;
                    memset(ctx.renameBuffer, 0, 512);
                    strncpy(ctx.renameBuffer, ctx.streamURLs[i].c_str(), 511);
                    ctx.showRenameModal = true;
                }
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, DS::DangerColor);
                if (ImGui::MenuItem("Eliminar URL")) {
                    ImGui::PopStyleColor();
                    ctx.streamURLs.erase(ctx.streamURLs.begin() + i);
                    if (ctx.selectedURLIndex == i) ctx.selectedURLIndex = -1;
                    ctx.saveStreamURLs();
                    ImGui::EndPopup();
                    break;
                }
                ImGui::PopStyleColor();
                ImGui::EndPopup();
            }

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", ctx.streamURLs[i].c_str());
        }

        ImGui::PopStyleVar(); // ItemSpacing
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    ImGui::Spacing();

    bool canAdd = (ctx.selectedURLIndex >= 0 &&
                   ctx.selectedURLIndex < (int)ctx.streamURLs.size());
    if (!canAdd) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.38f);
        GlassIconButton("addQueue", "add_to_queue", "+", "Agregar URL seleccionada a cola",
                        { -1.f, DS::ButtonHeight });
        ImGui::PopStyleVar();
    } else {
        if (GlassIconButton("addQueue2", "add_to_queue", "+",
                            "Agregar URL seleccionada a cola", { -1.f, DS::ButtonHeight }))
        {
            if (ctx.monitorRef)
                ctx.monitorRef->AddURLToQueue(ctx.streamURLs[ctx.selectedURLIndex]);
        }
    }
}

} // namespace ProyecThor::Library