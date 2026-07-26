#include "SettingsPanel.h"
#include "SettingsManager.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <ctime>
#include <fstream>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif
#ifdef _WIN32
#include <shellapi.h>
#endif

#if defined(_WIN32)
    #include <windows.h>
    #include <winhttp.h>
    #pragma comment(lib, "winhttp.lib")
#endif

namespace ProyecThor::UI::Settings {

// ─────────────────────────────────────────────────────────────────────────────
//  Estado global del sistema de actualización
// ─────────────────────────────────────────────────────────────────────────────

enum class UpdateStatus { Idle, Checking, UpToDate, Available, Error };

static std::atomic<UpdateStatus> s_Status{ UpdateStatus::Idle };
static std::string               s_LatestVersion  = "";
static std::string               s_DownloadUrl    = "";
static std::string               s_ErrorMsg       = "";
static std::thread               s_TaskThread;

static std::atomic<bool>         s_IsDownloading{ false };
static std::atomic<float>        s_DownloadProgress{ 0.0f };
static std::atomic<float>        s_DownloadSpeedMBs{ 0.0f };
static std::atomic<float>        s_DownloadedMB{ 0.0f };
static std::atomic<float>        s_TotalMB{ 0.0f };
static std::string               s_InstallerPath  = "";
static bool                      s_ShowModal      = false;

// ─────────────────────────────────────────────────────────────────────────────
//  Funciones internas (sin cambios en lógica de red respecto al original)
// ─────────────────────────────────────────────────────────────────────────────

static bool IsNewer(const std::string& current, const std::string& latest) {
    auto parse = [](const std::string& v) {
        int ma = 0, mi = 0, pa = 0;
        sscanf(v.c_str(), "%d.%d.%d", &ma, &mi, &pa);
        return ma * 10000 + mi * 100 + pa;
    };
    return parse(latest) > parse(current);
}

static std::string ExtractJsonString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find("\"", pos + search.size());
    if (pos == std::string::npos) return "";
    pos++;
    auto end = json.find("\"", pos);
    if (end == std::string::npos) return "";
    return json.substr(pos, end - pos);
}

// Busca dentro del array "assets" del release el asset llamado exactamente
// assetName y devuelve su browser_download_url. Antes se tomaba a ciegas el
// primer "browser_download_url" del JSON entero, lo cual dependia de que el
// release tuviera un unico asset -- desde que se empaqueta con WiX (.msi) en
// vez del .exe de Inno Setup, hace falta apuntar al asset correcto por
// nombre (mismo criterio que GitHubRelease::ExtractAssetUrl).
static std::string ExtractAssetDownloadUrl(const std::string& json, const std::string& assetName) {
    std::string marker = "\"name\": \"" + assetName + "\"";
    auto pos = json.find(marker);
    if (pos == std::string::npos) {
        marker = "\"name\":\"" + assetName + "\""; // por si viene minificado
        pos = json.find(marker);
        if (pos == std::string::npos) return "";
    }
    return ExtractJsonString(json.substr(pos), "browser_download_url");
}

#if defined(_WIN32)
static void ParseURL(const std::wstring& url, std::wstring& host, std::wstring& path) {
    URL_COMPONENTS urlComp;
    ZeroMemory(&urlComp, sizeof(urlComp));
    urlComp.dwStructSize      = sizeof(urlComp);
    urlComp.dwHostNameLength  = (DWORD)-1;
    urlComp.dwUrlPathLength   = (DWORD)-1;
    WinHttpCrackUrl(url.c_str(), (DWORD)url.length(), 0, &urlComp);
    host = std::wstring(urlComp.lpszHostName, urlComp.dwHostNameLength);
    path = std::wstring(urlComp.lpszUrlPath,  urlComp.dwUrlPathLength);
}
#endif

static std::string FetchURL(const std::wstring& host, const std::wstring& path) {
#if defined(_WIN32)
    std::string result;
    HINTERNET hSession = WinHttpOpen(L"ProyecThor Updater",
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                             NULL, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES,
                                             WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(hRequest, NULL)) {
        DWORD size = 0;
        while (WinHttpQueryDataAvailable(hRequest, &size) && size > 0) {
            std::string buf(size, '\0');
            DWORD read = 0;
            WinHttpReadData(hRequest, &buf[0], size, &read);
            result += buf.substr(0, read);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
#else
    return "";
#endif
}

static void DoCheckUpdate(const std::string& currentVersion,
                           const std::string& channel,
                           bool showPopupIfAvailable) {
    s_Status = UpdateStatus::Checking;

    std::wstring host = L"api.github.com";
    std::wstring path = (channel == "beta")
        ? L"/repos/TheVixcho/ProyecThor/releases?per_page=1"
        : L"/repos/TheVixcho/ProyecThor/releases/latest";

    std::string body = FetchURL(host, path);
    if (body.empty()) {
        s_ErrorMsg = "No se pudo conectar al servidor.";
        s_Status   = UpdateStatus::Error;
        return;
    }

    std::string tag = ExtractJsonString(body, "tag_name");
    if (tag.empty()) {
        s_ErrorMsg = "Respuesta inesperada del servidor.";
        s_Status   = UpdateStatus::Error;
        return;
    }
    if (tag[0] == 'v') tag = tag.substr(1);

    s_DownloadUrl    = ExtractAssetDownloadUrl(body, "ProyecThor_Setup.msi");
    s_LatestVersion  = tag;

    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    char timeBuf[32];
    strftime(timeBuf, sizeof(timeBuf), "%d/%m/%Y %H:%M", localtime(&now));
    ProyecThor::Settings::SettingsManager::Get().GetSettings().updates.lastChecked = timeBuf;

    if (IsNewer(currentVersion, tag)) {
        s_Status = UpdateStatus::Available;
        if (showPopupIfAvailable && !s_DownloadUrl.empty())
            s_ShowModal = true;
    } else {
        s_Status = UpdateStatus::UpToDate;
    }
}

static void DoDownloadAndInstall(const std::string& urlStr) {
#if defined(_WIN32)
    s_IsDownloading    = true;
    s_DownloadProgress = 0.0f;
    s_DownloadedMB     = 0.0f;
    s_TotalMB          = 0.0f;

    std::wstring wUrl(urlStr.begin(), urlStr.end());
    std::wstring host, path;
    ParseURL(wUrl, host, path);

    HINTERNET hSession = WinHttpOpen(L"ProyecThor Updater",
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                             NULL, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES,
                                             WINHTTP_FLAG_SECURE);

    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(hRequest, NULL)) {

        // Tamaño total del archivo
        DWORD contentLength = 0;
        DWORD cbSize        = sizeof(contentLength);
        WinHttpQueryHeaders(hRequest,
                            WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX,
                            &contentLength, &cbSize, WINHTTP_NO_HEADER_INDEX);

        float totalBytes = (float)contentLength;
        s_TotalMB        = totalBytes / (1024.0f * 1024.0f);

        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        // .msi, no .exe: desde que se empaqueta con WiX, "abrir" este archivo
        // (ShellExecute con verbo "open") invoca msiexec via la asociacion
        // por defecto de Windows para .msi, igual que hacia antes con el
        // instalador .exe de Inno Setup.
        s_InstallerPath = std::string(tempPath) + "ProyecThor_Update.msi";

        std::ofstream outFile(s_InstallerPath, std::ios::binary);
        float  downloadedBytes = 0.0f;
        auto   lastTime        = std::chrono::steady_clock::now();
        float  bytesSinceLast  = 0.0f;

        DWORD size = 0;
        do {
            if (!WinHttpQueryDataAvailable(hRequest, &size)) break;
            if (size == 0) break;

            std::vector<char> buf(size);
            DWORD read = 0;
            if (WinHttpReadData(hRequest, buf.data(), size, &read)) {
                outFile.write(buf.data(), read);
                downloadedBytes  += (float)read;
                bytesSinceLast   += (float)read;
                s_DownloadedMB    = downloadedBytes / (1024.0f * 1024.0f);

                if (totalBytes > 0.0f)
                    s_DownloadProgress = downloadedBytes / totalBytes;

                // Calcular velocidad cada 300ms
                auto now  = std::chrono::steady_clock::now();
                float sec = std::chrono::duration<float>(now - lastTime).count();
                if (sec >= 0.3f) {
                    s_DownloadSpeedMBs = (bytesSinceLast / (1024.0f * 1024.0f)) / sec;
                    bytesSinceLast     = 0.0f;
                    lastTime           = now;
                }
            }
        } while (size > 0);

        outFile.close();
        ShellExecuteA(NULL, "open", s_InstallerPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
        exit(0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    s_IsDownloading = false;
    s_ShowModal     = false;
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
//  API pública: arranque y modal global
// ─────────────────────────────────────────────────────────────────────────────

void CheckUpdateOnStartup() {
    auto& u = ProyecThor::Settings::SettingsManager::Get().GetSettings().updates;
    if (u.checkOnStartup) {
        if (s_TaskThread.joinable()) s_TaskThread.join();
        s_TaskThread = std::thread(DoCheckUpdate, u.currentVersion, u.updateChannel, true);
        s_TaskThread.detach();
    }
}

void RenderGlobalUpdatePopup() {
    if (s_ShowModal)
        ImGui::OpenPopup("Actualizacion Disponible");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(460, 0));

    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.055f, 0.062f, 0.090f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(24.0f, 22.0f));

    if (ImGui::BeginPopupModal("Actualizacion Disponible", NULL,
                                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {

        // Encabezado del modal
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.92f, 0.93f, 0.97f, 1.0f));
        ImGui::TextUnformatted("Nueva version disponible");
        ImGui::PopStyleColor();

        ImGui::Spacing();

        // Versión con badge de color
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.65f, 0.14f, 1.0f));
        ImGui::Text("v%s", s_LatestVersion.c_str());
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (s_IsDownloading) {
            // ── Estado: descargando ──────────────────────────────────────────

            float pct    = s_DownloadProgress.load();
            float dlMB   = s_DownloadedMB.load();
            float totMB  = s_TotalMB.load();
            float speed  = s_DownloadSpeedMBs.load();

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.72f, 0.86f, 1.0f));
            ImGui::TextUnformatted("Descargando actualizacion...");
            ImGui::PopStyleColor();

            ImGui::Spacing();

            // Barra de progreso con porcentaje integrado
            // Usamos ProgressBar nativo más el texto de MB/velocidad debajo
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.350f, 0.500f, 0.970f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg,       ImVec4(0.10f, 0.114f, 0.160f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::ProgressBar(pct, ImVec2(-1.0f, 10.0f), "");
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);

            ImGui::Spacing();

            // Línea de metadatos: MB descargados, MB totales, velocidad y %
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.47f, 0.60f, 1.0f));
            if (totMB > 0.0f)
                ImGui::Text("%.1f MB / %.1f MB   %.1f MB/s   %d%%",
                            dlMB, totMB, speed, (int)(pct * 100.0f));
            else
                ImGui::Text("Descargando...  %d%%", (int)(pct * 100.0f));
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.80f, 0.40f, 0.40f, 1.0f));
            ImGui::TextUnformatted("No cierres ProyecThor durante la descarga.");
            ImGui::PopStyleColor();

        } else {
            // ── Estado: esperando confirmación ───────────────────────────────
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.72f, 0.86f, 1.0f));
            ImGui::TextWrapped("Hay una nueva version disponible. Al aceptar, ProyecThor "
                               "descargara el instalador y se cerrara automaticamente "
                               "para aplicar la actualizacion.");
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::Spacing();

            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(14.0f, 7.0f));

            // Botón principal
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.350f, 0.500f, 0.970f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.420f, 0.570f, 1.000f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.280f, 0.420f, 0.880f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

            if (ImGui::Button("Descargar e instalar", ImVec2(200.0f, 36.0f))) {
                if (!s_IsDownloading && !s_DownloadUrl.empty()) {
                    if (s_TaskThread.joinable()) s_TaskThread.join();
                    s_TaskThread = std::thread(DoDownloadAndInstall, s_DownloadUrl);
                    s_TaskThread.detach();
                }
            }
            ImGui::PopStyleColor(4);

            ImGui::SameLine(0, 10.0f);

            // Botón secundario
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.10f, 0.112f, 0.160f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.14f, 0.155f, 0.220f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.18f, 0.200f, 0.280f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.60f, 0.62f, 0.76f, 1.0f));

            if (ImGui::Button("Mas tarde", ImVec2(110.0f, 36.0f))) {
                s_ShowModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar(2);
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderCategoryUpdates — panel dentro de SettingsPanel
// ─────────────────────────────────────────────────────────────────────────────

void SettingsPanel::RenderCategoryUpdates() {
    auto& u   = ProyecThor::Settings::SettingsManager::Get().GetSettings().updates;
    auto  st  = s_Status.load();

    // ── Versión instalada ─────────────────────────────────────────────────────
    SectionTitle("Version instalada");

    // Badge de versión
    {
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2      p   = ImGui::GetCursorScreenPos();
        char        vtxt[32];
        snprintf(vtxt, sizeof(vtxt), "  v%s  ", u.currentVersion.c_str());
        ImVec2 tsz = ImGui::CalcTextSize(vtxt);

        dl->AddRectFilled(p, ImVec2(p.x + tsz.x, p.y + tsz.y + 8.0f),
                          IM_COL32(30, 56, 110, 180), 6.0f);
        dl->AddRect(p, ImVec2(p.x + tsz.x, p.y + tsz.y + 8.0f),
                    IM_COL32(61, 127, 245, 100), 6.0f, 0, 1.0f);
        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + 4.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.78f, 1.00f, 1.0f));
        ImGui::TextUnformatted(vtxt);
        ImGui::PopStyleColor();
        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + tsz.y + 8.0f + 8.0f));
    }

    // Fecha de última comprobación
    if (!u.lastChecked.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.38f, 0.40f, 0.54f, 1.0f));
        ImGui::Text("Ultima comprobacion: %s", u.lastChecked.c_str());
        ImGui::PopStyleColor();
    }

    // ── Caja de estado ───────────────────────────────────────────────────────
    SectionTitle("Estado");
    ImGui::Spacing();

    // Dimensiones y colores según estado
    struct StatusStyle {
        ImVec4 borderCol;
        ImVec4 bgCol;
        ImVec4 textCol;
    };

    StatusStyle ss;
    const char* statusTitle  = "";
    const char* statusDetail = "";
    bool        showSpinner  = false;

    switch (st) {
        case UpdateStatus::Idle:
            ss = { ImVec4(0.24f,0.26f,0.38f,1), ImVec4(0.08f,0.09f,0.14f,1), ImVec4(0.50f,0.52f,0.66f,1) };
            statusTitle  = "Sin comprobar";
            statusDetail = "Haz clic en Comprobar ahora para buscar actualizaciones en GitHub.";
            break;
        case UpdateStatus::Checking:
            ss = { ImVec4(0.22f,0.40f,0.88f,0.60f), ImVec4(0.06f,0.10f,0.22f,1), ImVec4(0.55f,0.75f,1.00f,1) };
            statusTitle  = "Comprobando...";
            statusDetail = "Conectando con el servidor de actualizaciones.";
            showSpinner  = true;
            break;
        case UpdateStatus::UpToDate:
            ss = { ImVec4(0.20f,0.66f,0.40f,0.55f), ImVec4(0.05f,0.14f,0.09f,1), ImVec4(0.30f,0.86f,0.56f,1) };
            statusTitle  = "Estas al dia";
            statusDetail = "No hay actualizaciones disponibles en este momento.";
            break;
        case UpdateStatus::Available:
            ss = { ImVec4(0.88f,0.58f,0.10f,0.55f), ImVec4(0.14f,0.10f,0.03f,1), ImVec4(0.96f,0.75f,0.30f,1) };
            statusTitle  = "Nueva version disponible";
            statusDetail = "";
            break;
        case UpdateStatus::Error:
            ss = { ImVec4(0.88f,0.28f,0.28f,0.55f), ImVec4(0.14f,0.04f,0.04f,1), ImVec4(0.96f,0.50f,0.50f,1) };
            statusTitle  = "Error de conexion";
            statusDetail = s_ErrorMsg.c_str();
            break;
    }

    // Dibujar la caja de estado
    ImVec2 boxPos = ImGui::GetCursorScreenPos();
    float  boxW   = ImGui::GetContentRegionAvail().x;
    float  boxH   = (st == UpdateStatus::Available) ? 80.0f : 64.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(boxPos, ImVec2(boxPos.x + boxW, boxPos.y + boxH),
                      ImGui::ColorConvertFloat4ToU32(ss.bgCol), 8.0f);
    dl->AddRect(boxPos, ImVec2(boxPos.x + boxW, boxPos.y + boxH),
                ImGui::ColorConvertFloat4ToU32(ss.borderCol), 8.0f, 0, 1.0f);

    ImGui::SetCursorScreenPos(ImVec2(boxPos.x + 16.0f, boxPos.y + 14.0f));

    if (showSpinner) {
        SpinnerWidget(8.0f, 2.0f, ss.textCol);
        ImGui::SameLine(0, 10.0f);
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ss.textCol);
    ImGui::TextUnformatted(statusTitle);
    ImGui::PopStyleColor();

    if (statusDetail[0] != '\0') {
        ImGui::SetCursorScreenPos(ImVec2(boxPos.x + 16.0f, boxPos.y + 36.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ss.textCol.x, ss.textCol.y, ss.textCol.z, 0.72f));
        ImGui::TextUnformatted(statusDetail);
        ImGui::PopStyleColor();
    }

    if (st == UpdateStatus::Available && !s_LatestVersion.empty()) {
        ImGui::SetCursorScreenPos(ImVec2(boxPos.x + 16.0f, boxPos.y + 34.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.75f, 0.30f, 0.80f));
        ImGui::Text("Version %s disponible", s_LatestVersion.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::SetCursorScreenPos(ImVec2(boxPos.x, boxPos.y + boxH + 14.0f));

    // ── Sección de descarga activa ─────────────────────────────────────────────
    if (s_IsDownloading) {
        float pct   = s_DownloadProgress.load();
        float dlMB  = s_DownloadedMB.load();
        float totMB = s_TotalMB.load();
        float speed = s_DownloadSpeedMBs.load();

        SectionTitle("Descargando actualizacion");

        // Barra de progreso animada personalizada (helper del panel)
        AnimatedProgressBar(pct, ImVec2(ImGui::GetContentRegionAvail().x - 56.0f, 8.0f),
                             ImVec4(0.350f, 0.500f, 0.970f, 1.0f));

        ImGui::Spacing();

        // Metadatos de descarga
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.42f, 0.58f, 1.0f));
        if (totMB > 0.0f)
            ImGui::Text("%.1f MB / %.1f MB   %.1f MB/s", dlMB, totMB, speed);
        else
            ImGui::Text("Descargando...");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.80f, 0.38f, 0.38f, 1.0f));
        ImGui::TextUnformatted("No cierres ProyecThor durante la descarga.");
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    // ── Botones de acción ──────────────────────────────────────────────────────
    ImGui::Spacing();

    bool busy = (st == UpdateStatus::Checking) || s_IsDownloading;
    if (busy) ImGui::BeginDisabled();

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(14.0f, 6.0f));

    // Botón comprobar
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.10f, 0.112f, 0.160f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.14f, 0.155f, 0.220f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.18f, 0.200f, 0.280f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.70f, 0.72f, 0.86f, 1.0f));

    if (ImGui::Button("Comprobar ahora", ImVec2(170.0f, 30.0f))) {
        if (s_TaskThread.joinable()) s_TaskThread.join();
        s_TaskThread = std::thread(DoCheckUpdate, u.currentVersion, u.updateChannel, true);
        s_TaskThread.detach();
    }
    ImGui::PopStyleColor(4);

    // Botón descargar (solo si hay actualización disponible)
    if (st == UpdateStatus::Available && !s_DownloadUrl.empty() && !s_IsDownloading) {
        ImGui::SameLine(0, 10.0f);

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.24f, 0.18f, 0.06f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.42f, 0.32f, 0.10f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.56f, 0.42f, 0.12f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.96f, 0.75f, 0.30f, 1.0f));

        if (ImGui::Button("Descargar ahora", ImVec2(170.0f, 30.0f))) {
            if (s_TaskThread.joinable()) s_TaskThread.join();
            s_TaskThread = std::thread(DoDownloadAndInstall, s_DownloadUrl);
            s_TaskThread.detach();
        }
        ImGui::PopStyleColor(4);
    }

    ImGui::PopStyleVar(2);
    if (busy) ImGui::EndDisabled();

    // ── Configuración ──────────────────────────────────────────────────────────
    SectionTitle("Configuracion");

    ImGui::Checkbox("Comprobar al iniciar", &u.checkOnStartup);
    HelpTooltip("Comprueba actualizaciones automaticamente al abrir ProyecThor.");

    ImGui::Checkbox("Descarga automatica", &u.autoDownload);
    HelpTooltip("Descarga la nueva version en segundo plano sin pedir confirmacion.");

    ImGui::Spacing();

    // Canal
    const char* channels[] = { "stable", "beta" };
    const char* labels[]   = { "Estable", "Beta" };
    int         chIdx      = (u.updateChannel == "beta") ? 1 : 0;

    ImGui::TextUnformatted("Canal de actualizacion:");
    HelpTooltip("'Estable': versiones probadas y recomendadas.\n'Beta': acceso anticipado, puede contener errores.");

    ImGui::Spacing();
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 20.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(14.0f, 5.0f));

    for (int i = 0; i < 2; i++) {
        bool isActive = (chIdx == i);

        if (isActive) {
            ImGui::PushStyleColor(ImGuiCol_Button,
                i == 0 ? ImVec4(0.10f,0.28f,0.14f,1.0f) : ImVec4(0.28f,0.18f,0.04f,1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                i == 0 ? ImVec4(0.14f,0.36f,0.18f,1.0f) : ImVec4(0.36f,0.24f,0.06f,1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                i == 0 ? ImVec4(0.18f,0.44f,0.22f,1.0f) : ImVec4(0.44f,0.30f,0.08f,1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,
                i == 0 ? ImVec4(0.30f,0.86f,0.48f,1.0f) : ImVec4(0.96f,0.65f,0.14f,1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.08f,0.09f,0.14f,1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.11f,0.12f,0.18f,1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.14f,0.15f,0.22f,1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.40f,0.42f,0.56f,1.0f));
        }

        if (i > 0) ImGui::SameLine(0, 6.0f);

        if (ImGui::Button(labels[i], ImVec2(100.0f, 28.0f)))
            u.updateChannel = channels[i];

        ImGui::PopStyleColor(4);
    }

    ImGui::PopStyleVar(2);
}

} // namespace ProyecThor::UI::Settings