#include "MonitorView.h"
#include "MonitorQueueHelpers.h"
#include "MonitorTheme.h"
#include "backend/core/PresentationCore.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <string>
#include <cmath>
#include <cinttypes>
#include "backend/core/AppPaths.h"
#include "backend/core/AppPaths.h"
#include "frontend/ui/bin/StyleGeneralApp.h" 

namespace ProyecThor::UI {

using namespace MonitorTheme;
using namespace QueueHelpers;

static constexpr float k_RowH = 44.0f;

static void FormatTime(int64_t ms, char* buf, size_t bufSz)
{
    if (ms < 0) { snprintf(buf, bufSz, "--:--"); return; }
    int64_t s   = ms / 1000;
    int64_t min = s / 60;
    int64_t sec = s % 60;
    snprintf(buf, bufSz, "%" PRId64 ":%02" PRId64, min, sec);
}

struct RowColors {
    ImU32 bg;
    ImU32 accent;
    ImU32 text;
    bool  drawLeftBar;
};

static RowColors GetRowColors(bool isPlaying, bool isSelected, int rowIndex)
{
    RowColors c{};
    c.text        = ImGui::ColorConvertFloat4ToU32(k_TextPrimary);
    c.drawLeftBar = false;
    c.accent      = 0;

    if (isPlaying) {
        c.bg          = ImGui::ColorConvertFloat4ToU32(ImVec4(k_QueueAccent.x, k_QueueAccent.y, k_QueueAccent.z, 0.33f));
        c.accent      = ImGui::ColorConvertFloat4ToU32(k_QueueAccent);
        c.text        = ImGui::ColorConvertFloat4ToU32(k_QueueAccent);
        c.drawLeftBar = true;
    } else if (isSelected) {
        c.bg = ImGui::ColorConvertFloat4ToU32(ImVec4(k_PrevAccent.x, k_PrevAccent.y, k_PrevAccent.z, 0.35f));
    } else if (rowIndex % 2 == 0) {
        c.bg = ImGui::ColorConvertFloat4ToU32(ImVec4(k_TextPrimary.x, k_TextPrimary.y, k_TextPrimary.z, 0.02f));
    } else {
        c.bg = 0;
    }
    return c;
}
static void DrawBtnIcon(const char* iconName, float leftPad = 12.0f, float size = 18.0f)
{
    if (StyleGeneralApp::Icons.count(iconName) == 0) return;
    void* icon = StyleGeneralApp::Icons[iconName].textureID;
    if (!icon) return;

    ImVec2 bMin = ImGui::GetItemRectMin();
    ImVec2 bMax = ImGui::GetItemRectMax();
    float  bH   = bMax.y - bMin.y;
    float  sz   = std::min(size, bH - 10.0f);
    float  y    = bMin.y + (bH - sz) * 0.5f;

    ImGui::GetWindowDrawList()->AddImage(
        icon,
        { bMin.x + leftPad, y },
        { bMin.x + leftPad + sz, y + sz });
}
void MonitorView::RenderQueue(float w)
{
    // El avance de la cola (MonitorQueueEngine::Update) ya NO se llama
    // aca: si este panel no esta visible, RenderQueue() ni se ejecuta, y
    // la cola se quedaba pegada apenas el operador miraba otra cosa. Ahora
    // corre incondicionalmente desde MonitorView::Update() (ver
    // HomePanel::Render(), pump incondicional junto a OClock::Update()).
    {
        Core::VLCBasePlayer* queuePlayer = Core::PresentationCore::Get().GetBackgroundPlayer();
        m_LivePlaying = m_QueueEngine.IsActive() && queuePlayer && !queuePlayer->IsPaused();
    }

    const auto& items   = m_QueueEngine.Items();
    int currentIdx       = m_QueueEngine.CurrentIndex();
    int selectedIdx       = m_QueueEngine.SelectedIndex();

    const float totalH   = ImGui::GetContentRegionAvail().y;
    const float headerH  = ImGui::GetTextLineHeightWithSpacing() + 16.0f;
    const float spacing  = ImGui::GetStyle().ItemSpacing.y;
    const float btnH     = 30.0f;
    const float apBtnH   = 38.0f;
    const float btnAreaH = apBtnH + btnH * 2.0f + spacing * 3.0f + 10.0f;
    const float listH    = totalH - headerH - btnAreaH - k_PadLg * 2.0f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg,  k_Bg3);
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(k_QueueAccent.x, k_QueueAccent.y, k_QueueAccent.z, 0.20f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   k_R);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   { k_Pad, k_Pad });

    ImGui::BeginChild("##queue_outer", { w, totalH }, true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const float innerW = w - k_Pad * 2.0f;

    // ── Encabezado ────────────────────────────────────────────────────────────
    {
        ImGui::PushStyleColor(ImGuiCol_Text, k_QueueAccent);
        char title[64];
        snprintf(title, sizeof(title), "Cola  (%d)", static_cast<int>(items.size()));
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();

        bool isActive = (currentIdx >= 0 && currentIdx < static_cast<int>(items.size()));
        if (isActive)
        {
            float pulse = 0.65f + 0.35f * std::abs(std::sin((float)ImGui::GetTime() * 2.8f));
            ImGui::SameLine();
            float badgeX = innerW - ImGui::CalcTextSize("ON AIR").x;
            ImGui::SetCursorPosX(badgeX);
            ImGui::PushStyleColor(ImGuiCol_Text,
                ImVec4(k_QueueAccent.x * pulse,
                       k_QueueAccent.y * pulse,
                       k_QueueAccent.z * pulse, 1.0f));
            ImGui::TextUnformatted("ON AIR");
            ImGui::PopStyleColor();
        }
    }

    {
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(k_QueueAccent.x, k_QueueAccent.y, k_QueueAccent.z, 0.18f));
        ImGui::Separator();
        ImGui::PopStyleColor();
    }

    // ── Progreso del clip activo ──────────────────────────────────────────────
    int64_t activeLenMs = -1;
    int64_t activeCurMs = -1;
    if (currentIdx >= 0)
    {
        Core::VLCBasePlayer* bg = Core::PresentationCore::Get().GetBackgroundPlayer();
        if (bg) {
            activeLenMs = bg->GetLength();
            activeCurMs = bg->GetTime();
        }
    }

    // ── Lista scrolleable ─────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::BeginChild("##queue_list", { 0.0f, listH }, false);
    ImGui::PopStyleColor();

    ImDrawList* dl     = ImGui::GetWindowDrawList();
    float       availW = ImGui::GetContentRegionAvail().x;

    // Drop zone antes del primer item → mover al inicio (reorden) o agregar
    // (si el payload viene de la Biblioteca: video o URL).
    ImGui::Dummy({ availW, 3.0f });
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("QUEUE_ITEM"))
        {
            int src = *(const int*)p->Data;
            m_QueueEngine.Move(src, 0);
        }
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("VIDEO_TO_QUEUE"))
        {
            std::string path(static_cast<const char*>(p->Data), p->DataSize - 1);
            m_QueueEngine.Add(path);
        }
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("URL_TO_QUEUE"))
        {
            std::string url(static_cast<const char*>(p->Data), p->DataSize - 1);
            m_QueueEngine.AddURL(url);
        }
        ImGui::EndDragDropTarget();
    }

    int removeRequest = -1;

    for (int i = 0; i < static_cast<int>(items.size()); i++)
    {
        const std::string& entry     = items[i];
        char               pfx       = QueuePfx(entry);
        std::string        disp      = QueueDisplayName(entry);
        bool               isPlaying = (i == currentIdx);
        bool               isSel     = (i == selectedIdx);

        ImGui::PushID(i);

        ImVec2    rowMin = ImGui::GetCursorScreenPos();
        RowColors rc     = GetRowColors(isPlaying, isSel, i);

        if (rc.bg)
            dl->AddRectFilled(rowMin, { rowMin.x + availW, rowMin.y + k_RowH }, rc.bg, 4.0f);
        if (rc.drawLeftBar)
            dl->AddRectFilled(rowMin, { rowMin.x + 3.0f, rowMin.y + k_RowH }, rc.accent, 2.0f);

        if (isPlaying && activeLenMs > 0 && activeCurMs >= 0)
        {
            float progress = std::min(1.0f,
                static_cast<float>(activeCurMs) / static_cast<float>(activeLenMs));
            float barY = rowMin.y + k_RowH - 3.0f;
            dl->AddRectFilled({ rowMin.x + 3.0f, barY },
                              { rowMin.x + availW, barY + 3.0f },
                              ImGui::ColorConvertFloat4ToU32(ImVec4(k_TextPrimary.x, k_TextPrimary.y, k_TextPrimary.z, 0.07f)), 1.5f);
            dl->AddRectFilled({ rowMin.x + 3.0f, barY },
                              { rowMin.x + 3.0f + (availW - 3.0f) * progress, barY + 3.0f },
                              ImGui::ColorConvertFloat4ToU32(k_QueueAccent), 1.5f);
        }

        if (isPlaying)
            DrawPlayingBars(dl, rowMin, k_RowH, rowMin.x + 8.0f);

        {
            char num[8];
            snprintf(num, sizeof(num), "%d", i + 1);
            ImVec2 numSz = ImGui::CalcTextSize(num);
            dl->AddText({ rowMin.x + 22.0f, rowMin.y + (k_RowH - numSz.y) * 0.5f },
                        isPlaying ? rc.accent
                                  : ImGui::ColorConvertFloat4ToU32(ImVec4(k_TextSecondary.x, k_TextSecondary.y, k_TextSecondary.z, 0.7f)),
                        num);
        }

        {
            const char* tag      = (pfx == k_PfxURL) ? "URL" : "VID";
            ImU32       tagColor = (pfx == k_PfxURL)
                                   ? ImGui::ColorConvertFloat4ToU32(ImVec4(k_PrevAccent.x, k_PrevAccent.y, k_PrevAccent.z, 0.78f))
                                   : ImGui::ColorConvertFloat4ToU32(ImVec4(k_QueueAccent.x, k_QueueAccent.y, k_QueueAccent.z, 0.78f));
            ImU32       tagBg    = (pfx == k_PfxURL)
                                   ? ImGui::ColorConvertFloat4ToU32(ImVec4(k_PrevAccent.x, k_PrevAccent.y, k_PrevAccent.z, 0.35f))
                                   : ImGui::ColorConvertFloat4ToU32(ImVec4(k_QueueAccent.x, k_QueueAccent.y, k_QueueAccent.z, 0.35f));
            ImVec2 tagSz = ImGui::CalcTextSize(tag);
            float  tagX  = rowMin.x + 44.0f;
            float  tagY  = rowMin.y + (k_RowH - tagSz.y) * 0.5f;
            dl->AddRectFilled({ tagX - 3.0f, tagY - 1.0f },
                              { tagX + tagSz.x + 3.0f, tagY + tagSz.y + 1.0f },
                              tagBg, 3.0f);
            dl->AddText({ tagX, tagY }, tagColor, tag);
        }

        {
            float nameX    = rowMin.x + 86.0f;
            float nameMaxW = availW - 90.0f;

            if (isPlaying && activeLenMs > 0 && activeCurMs >= 0)
            {
                char timeBuf[24];
                int64_t remaining = std::max<int64_t>(0, activeLenMs - activeCurMs);
                FormatTime(remaining, timeBuf, sizeof(timeBuf));
                float timeW = ImGui::CalcTextSize(timeBuf).x + 6.0f;
                nameMaxW -= timeW;
                dl->AddText(
                    { rowMin.x + availW - timeW,
                      rowMin.y + (k_RowH - ImGui::GetTextLineHeight()) * 0.5f },
                    ImGui::ColorConvertFloat4ToU32(ImVec4(k_TextSecondary.x, k_TextSecondary.y, k_TextSecondary.z, 0.7f)), timeBuf);
            }

            std::string name = disp;
            while (name.size() > 4 &&
                   ImGui::CalcTextSize(name.c_str()).x > nameMaxW)
                name.resize(name.size() - 1);
            if (name != disp) name += "...";

            dl->AddText(
                { nameX, rowMin.y + (k_RowH - ImGui::GetTextLineHeight()) * 0.5f },
                rc.text, name.c_str());
        }

        {
            ImGui::SetCursorScreenPos(rowMin);
            ImGui::PushStyleColor(ImGuiCol_Header,        { 0,0,0,0 });
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, { 1,1,1,0.05f });
            ImGui::PushStyleColor(ImGuiCol_HeaderActive,  { 1,1,1,0.09f });
            char rowId[16];
            snprintf(rowId, sizeof(rowId), "##row%d", i);
            if (ImGui::Selectable(rowId, isPlaying || isSel,
                                  ImGuiSelectableFlags_AllowDoubleClick |
                                  ImGuiSelectableFlags_AllowOverlap,
                                  { availW, k_RowH }))
            {
                m_QueueEngine.SetSelectedIndex(i);
                if (ImGui::IsMouseDoubleClicked(0))
                    PlayQueueItem(i);
            }
            ImGui::PopStyleColor(3);
        }

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", QueuePath(entry).c_str());

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        {
            ImGui::SetDragDropPayload("QUEUE_ITEM", &i, sizeof(int));
            ImGui::PushStyleColor(ImGuiCol_Text, k_QueueAccent);
            ImGui::TextUnformatted(disp.c_str());
            ImGui::PopStyleColor();
            m_DragSrcIndex = i;
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("QUEUE_ITEM"))
            {
                int src = *(const int*)p->Data;
                m_QueueEngine.Move(src, i);
                m_DragSrcIndex = -1;
            }
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("VIDEO_TO_QUEUE"))
            {
                std::string path(static_cast<const char*>(p->Data), p->DataSize - 1);
                m_QueueEngine.Add(path);
            }
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("URL_TO_QUEUE"))
            {
                std::string url(static_cast<const char*>(p->Data), p->DataSize - 1);
                m_QueueEngine.AddURL(url);
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextItem("##ctx_q"))
        {
            if (ImGui::MenuItem("Reproducir ahora")) PlayQueueItem(i);
            ImGui::Separator();
            bool canUp   = (i > 0);
            bool canDown = (i < static_cast<int>(items.size()) - 1);
            if (!canUp) ImGui::BeginDisabled();
            if (ImGui::MenuItem("Subir")) m_QueueEngine.Move(i, i - 1);
            if (!canUp) ImGui::EndDisabled();
            if (!canDown) ImGui::BeginDisabled();
            if (ImGui::MenuItem("Bajar")) m_QueueEngine.Move(i, i + 1);
            if (!canDown) ImGui::EndDisabled();
            ImGui::Separator();
            if (ImGui::MenuItem("Quitar de la cola")) removeRequest = i;
            ImGui::EndPopup();
        }

        ImGui::PopID();

        ImVec2 sepY = ImGui::GetCursorScreenPos();
        dl->AddLine({ rowMin.x, sepY.y }, { rowMin.x + availW, sepY.y },
                    ImGui::ColorConvertFloat4ToU32(ImVec4(k_TextPrimary.x, k_TextPrimary.y, k_TextPrimary.z, 0.025f)));
    }

    if (removeRequest >= 0)
    {
        m_QueueEngine.Remove(removeRequest);
        m_LivePlaying = m_QueueEngine.IsActive();
    }

    if (items.empty())
    {
        float ey = listH * 0.35f;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ey);
        ImGui::PushStyleColor(ImGuiCol_Text, k_TextDim);
        auto center = [&](const char* txt){
            float tw = ImGui::CalcTextSize(txt).x;
            ImGui::SetCursorPosX((availW - tw) * 0.5f);
            ImGui::TextUnformatted(txt);
        };
        center("Sin videos en la cola");
        center("Agrega con el boton  +  Agregar");
        ImGui::PopStyleColor();
    }

    // Drop zone al final → mover al final (reorden) o agregar (Biblioteca).
    // Esta es tambien la zona que cubre la mayor parte del area vacia de la
    // lista cuando no hay items todavia, asi que es el target mas usado la
    // primera vez que se arrastra un video/URL a una cola vacia.
    ImGui::Dummy({ availW, k_RowH * 0.5f });
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("QUEUE_ITEM"))
        {
            int src     = *(const int*)p->Data;
            int lastIdx = static_cast<int>(items.size()) - 1;
            if (lastIdx >= 0)
                m_QueueEngine.Move(src, lastIdx);
        }
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("VIDEO_TO_QUEUE"))
        {
            std::string path(static_cast<const char*>(p->Data), p->DataSize - 1);
            m_QueueEngine.Add(path);
        }
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("URL_TO_QUEUE"))
        {
            std::string url(static_cast<const char*>(p->Data), p->DataSize - 1);
            m_QueueEngine.AddURL(url);
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::EndChild(); // queue_list

    {
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(k_QueueAccent.x, k_QueueAccent.y, k_QueueAccent.z, 0.12f));
        ImGui::Separator();
        ImGui::PopStyleColor();
    }

  // ── Botones ───────────────────────────────────────────────────────────────
    {
        const float gap      = ImGui::GetStyle().ItemSpacing.x;
        const float bw2      = std::floor((innerW - gap) / 2.0f);
        bool        isEmpty  = items.empty();
        bool        isActive = (currentIdx >= 0 && currentIdx < static_cast<int>(items.size()));

        {
            ImVec4 apBase = isActive
                ? ImVec4(k_QueueAccent.x, k_QueueAccent.y, k_QueueAccent.z, 0.50f)
                : ImVec4(k_QueueAccent.x, k_QueueAccent.y, k_QueueAccent.z, 0.34f);
            ImVec4 apHov = isActive
                ? ImVec4(k_QueueAccent.x, k_QueueAccent.y, k_QueueAccent.z, 0.64f)
                : ImVec4(k_QueueAccent.x, k_QueueAccent.y, k_QueueAccent.z, 0.46f);
            ImVec4 apAct = ImVec4(k_QueueAccent.x, k_QueueAccent.y, k_QueueAccent.z, 0.27f);
            const char* apLabel = isActive
                ? "        Detener reproduccion"
                : "        Reproducir cola";

            if (isEmpty) ImGui::BeginDisabled();
            bool apClicked = QueueColorBtn(apLabel, { innerW, apBtnH }, apBase, apHov, apAct,
                              k_QueueAccent, k_R * 0.7f);
            DrawBtnIcon(isActive ? "stop" : "play", 14.0f, 20.0f);
            if (apClicked)
            {
                m_QueueEngine.TogglePlayStop();
                m_LivePlaying = m_QueueEngine.IsActive();
            }
            if (isEmpty) ImGui::EndDisabled();
        }

        ImGui::Spacing();

        {
            bool hasPrev = isActive && (currentIdx > 0);
            bool hasNext = isActive && (currentIdx < static_cast<int>(items.size()) - 1);

            if (!hasPrev) ImGui::BeginDisabled();
            bool prevClicked = QueueColorBtn("      Anterior", { bw2, btnH },
                              k_BtnGreen, k_BtnGreenH, k_BtnGreenA, k_QueueAccent, k_R * 0.7f);
            DrawBtnIcon("skip_prev", 10.0f, 16.0f);
            if (prevClicked)
                PlayQueueItem(currentIdx - 1);
            if (!hasPrev) ImGui::EndDisabled();

            ImGui::SameLine();

            if (!hasNext) ImGui::BeginDisabled();
            bool nextClicked = QueueColorBtn("      Siguiente", { bw2, btnH },
                              k_BtnGreen, k_BtnGreenH, k_BtnGreenA, k_QueueAccent, k_R * 0.7f);
            DrawBtnIcon("skip_next", 10.0f, 16.0f);
            if (nextClicked)
                PlayQueueItem(currentIdx + 1);
            if (!hasNext) ImGui::EndDisabled();
        }

        ImGui::Spacing();

        {
            bool addClicked = QueueColorBtn("      Agregar", { bw2, btnH },
                              k_BtnNeutral, k_BtnNeutralH, k_BtnNeutralA,
                              k_BtnNeutralT, k_R * 0.7f);
            DrawBtnIcon("add_to_queue", 10.0f, 16.0f);
            if (addClicked)
            {
                auto sel = Core::PresentationCore::Get().PeekSelection();
                if (!sel.title.empty() && sel.type == Core::ItemType::Video)
                {
                    std::string path = sel.title;
                    if (path.rfind("http", 0) == 0)
                        m_QueueEngine.AddURL(path);
                    else
                        m_QueueEngine.Add(VideosPath() + path);
                }
            }

            ImGui::SameLine();

            if (isEmpty) ImGui::BeginDisabled();
            bool clearClicked = QueueColorBtn("      Limpiar", { bw2, btnH },
                              k_BtnDel, k_BtnDelH, k_BtnDelA, k_BtnDelT, k_R * 0.7f);
            DrawBtnIcon("cleaning_services", 10.0f, 16.0f);
            if (clearClicked)
            {
                m_QueueEngine.Clear();
                m_LivePlaying = false;
            }
            if (isEmpty) ImGui::EndDisabled();
        }
    }

    ImGui::EndChild(); // queue_outer
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

} // namespace ProyecThor::UI