#include "DatabasePanel.h"

// Ajusta esta ruta al lugar real donde tengas AssetsPath.h en tu proyecto.
// Expone ProyecThor::SongsPath() -> "%APPDATA%/ProyecThor/assets/songs/"
#include "../src/backend/core/AppPaths.h"

#include <imgui.h>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <cctype>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace ProyecThor::UI {

namespace {

// Carpeta donde vive el ejecutable (ej: .../build/default/)
std::filesystem::path GetExecutableDirectory()
{
    char buffer[MAX_PATH] = {};
    DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (len == 0 || len == MAX_PATH)
        return std::filesystem::current_path();

    return std::filesystem::path(buffer).parent_path();
}

// Carpeta con las canciones que vienen incluidas con el programa:
// build/default/songs
std::filesystem::path GetBundledSongsDirectory()
{
    return GetExecutableDirectory() / "songs";
}

std::string ToLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

} // namespace

void DatabasePanel::Open()
{
    m_Show        = true;
    m_ScanPending = true;
    m_LastActionMessage.clear();
    std::memset(m_SearchBuffer, 0, sizeof(m_SearchBuffer));
}

void DatabasePanel::ScanBundledSongs()
{
    m_AvailableSongs.clear();

    std::filesystem::path bundledDir = GetBundledSongsDirectory();
    std::error_code ec;

    if (!std::filesystem::exists(bundledDir, ec))
    {
        m_ScanPending = false;
        return;
    }

    std::filesystem::path destDir = ProyecThor::SongsPath();

    for (auto& entry : std::filesystem::directory_iterator(bundledDir, ec))
    {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".txt") continue;

        SongEntry song;
        song.fileName    = entry.path().filename().string();
        song.displayName = entry.path().stem().string();

        // Reemplaza guiones bajos por espacios para mostrarlo mas legible
        std::replace(song.displayName.begin(), song.displayName.end(), '_', ' ');

        std::filesystem::path destFile = destDir / song.fileName;
        song.alreadyImported = std::filesystem::exists(destFile);

        m_AvailableSongs.push_back(std::move(song));
    }

    std::sort(m_AvailableSongs.begin(), m_AvailableSongs.end(),
              [](const SongEntry& a, const SongEntry& b) {
                  return a.displayName < b.displayName;
              });

    m_ScanPending = false;
}

bool DatabasePanel::ImportSong(const SongEntry& entry)
{
    std::filesystem::path srcFile = GetBundledSongsDirectory() / entry.fileName;

    std::filesystem::path destDir = ProyecThor::SongsPath();
    std::error_code ec;
    std::filesystem::create_directories(destDir, ec);

    std::filesystem::path destFile = destDir / entry.fileName;

    bool ok = std::filesystem::copy_file(
        srcFile, destFile,
        std::filesystem::copy_options::overwrite_existing, ec);

    return ok && !ec;
}

void DatabasePanel::Render()
{
    if (!m_Show) return;

    if (m_ScanPending)
        ScanBundledSongs();

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, { 0.5f, 0.5f });
    ImGui::SetNextWindowSize({ 520.f, 560.f }, ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints({ 380.f, 320.f }, { FLT_MAX, FLT_MAX });

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  { 20.0f, 18.0f });

    bool windowOpen = true;
    ImGui::Begin("Base de Datos de Canciones##dbWin", &windowOpen,
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse);

    ImGui::PopStyleVar(2);

    if (!windowOpen)
    {
        m_Show = false;
        ImGui::End();
        return;
    }

    ImGui::TextDisabled("Canciones incluidas con el programa. Selecciona una para agregarla a tu biblioteca.");
    ImGui::Spacing();

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##dbSearch", "Buscar cancion...", m_SearchBuffer, sizeof(m_SearchBuffer));
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (!m_LastActionMessage.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text,
            m_LastActionSuccess ? ImVec4(0.20f, 0.85f, 0.40f, 1.0f)
                                 : ImVec4(0.90f, 0.30f, 0.30f, 1.0f));
        ImGui::TextWrapped("%s", m_LastActionMessage.c_str());
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    std::string filter = ToLower(m_SearchBuffer);

    ImGui::BeginChild("##dbList", ImVec2(0, 0), true);

    if (m_AvailableSongs.empty())
    {
        ImGui::TextDisabled("No se encontraron canciones en la carpeta 'songs' del programa.");
    }

    for (auto& song : m_AvailableSongs)
    {
        if (!filter.empty() && ToLower(song.displayName).find(filter) == std::string::npos)
            continue;

        ImGui::PushID(song.fileName.c_str());

        ImGui::TextUnformatted(song.displayName.c_str());
        ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowWidth() - 130.0f));

        const char* label = song.alreadyImported ? "Reimportar" : "Agregar";

        if (song.alreadyImported)
        {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(1.0f, 1.0f, 1.0f, 0.050f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.090f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.0f, 1.0f, 1.0f, 0.130f));
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.369f, 0.420f, 1.000f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.500f, 0.550f, 1.000f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.280f, 0.330f, 0.860f, 1.0f));
        }

        if (ImGui::Button(label, ImVec2(110.f, 0.f)))
        {
            bool ok = ImportSong(song);
            m_LastActionSuccess = ok;
            m_LastActionMessage = ok
                ? ("Se agrego \"" + song.displayName + "\" a tu biblioteca.")
                : ("No se pudo agregar \"" + song.displayName + "\".");

            if (ok)
                song.alreadyImported = true;
        }

        ImGui::PopStyleColor(3);
        ImGui::PopID();
    }

    ImGui::EndChild();

    ImGui::End();
}

} // namespace ProyecThor::UI
