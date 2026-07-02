#pragma once
#include <string>
#include <filesystem>
#include <imgui.h>
#include <windows.h>
#include <shlobj.h>
#include <cmath>
#include "MonitorTheme.h"

// =============================================================================
//  MonitorQueueHelpers.h
//  Funciones utilitarias para la cola de reproduccion:
//    - prefijos de entrada (Local / URL)
//    - ruta de persistencia
//    - nombres de display truncados
//    - boton con color personalizado
//    - barras animadas de "reproduciendo"
// =============================================================================

namespace ProyecThor::UI::QueueHelpers {

namespace fs = std::filesystem;
using namespace MonitorTheme;

// ── Prefijos de entrada ───────────────────────────────────────────────────────
inline constexpr char k_PfxLocal = 'L';
inline constexpr char k_PfxURL   = 'U';

inline std::string QueueEntry(char pfx, const std::string& s)
{
    return pfx + std::string("|") + s;
}

inline char QueuePfx(const std::string& e)
{
    return e.empty() ? '?' : e[0];
}

inline std::string QueuePath(const std::string& e)
{
    return (e.size() > 2) ? e.substr(2) : "";
}

// ── Truncado de nombres para display ─────────────────────────────────────────
inline std::string TruncPath(const std::string& p, size_t maxLen = 46)
{
    std::string name = fs::path(p).stem().string();
    if (name.size() > maxLen)
        name = name.substr(0, maxLen - 3) + "...";
    return name;
}

inline std::string QueueDisplayName(const std::string& entry)
{
    char pfx        = QueuePfx(entry);
    std::string path = QueuePath(entry);

    if (pfx == k_PfxLocal)
        return TruncPath(path, 46);

    // URL: quitar schema y truncar
    if (path.rfind("https://", 0) == 0)      path = path.substr(8);
    else if (path.rfind("http://", 0) == 0)  path = path.substr(7);
    if (path.size() > 46)                    path = path.substr(0, 43) + "...";
    return path;
}

// ── Ruta de persistencia de la cola ──────────────────────────────────────────
inline const std::string& GetQueueFilePath()
{
    static std::string s_Path;
    if (!s_Path.empty()) return s_Path;

    char buf[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, buf)))
        s_Path = std::string(buf) + "\\ProyecThor\\assets\\play_queue.txt";
    else
        s_Path = "play_queue.txt";

    return s_Path;
}

// ── Barras animadas de "reproduciendo" ───────────────────────────────────────
// Dibuja 3 barras verticales animadas a la izquierda del item activo.
inline void DrawPlayingBars(ImDrawList* dl, ImVec2 rowMin, float rowH, float baseX)
{
    float t        = static_cast<float>(ImGui::GetTime());
    float barBaseY = rowMin.y + rowH * 0.5f;
    for (int b = 0; b < 3; b++) {
        float phase = t * 3.0f + b * 1.2f;
        float barH  = 4.0f + std::abs(std::sin(phase)) * 7.0f;
        float bx    = baseX + static_cast<float>(b) * 5.0f;
        dl->AddRectFilled(
            { bx,        barBaseY - barH * 0.5f },
            { bx + 3.0f, barBaseY + barH * 0.5f },
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.35f, 0.78f, 1.0f, 0.92f)),
            1.5f);
    }
}

// ── Boton coloreado para la cola ──────────────────────────────────────────────
// Aplica colores base/hover/active y opcionalmente color de texto.
// Siempre hace PopStyleColor/PopStyleVar correctamente.
inline bool QueueColorBtn(
    const char* label,
    ImVec2      size,
    ImVec4      base,
    ImVec4      hov,
    ImVec4      act,
    ImVec4      textCol  = { -1.f, -1.f, -1.f, -1.f },
    float       rounding = 6.0f)
{
    ImGui::PushStyleColor(ImGuiCol_Button,        base);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  act);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rounding);

    bool hasTextColor = (textCol.w >= 0.f);
    if (hasTextColor)
        ImGui::PushStyleColor(ImGuiCol_Text, textCol);

    bool pressed = ImGui::Button(label, size);

    if (hasTextColor)
        ImGui::PopStyleColor();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    return pressed;
}

} // namespace ProyecThor::UI::QueueHelpers
