#include "GitHubRelease.h"

#if defined(_WIN32)
#include <windows.h>
#include <winhttp.h>
#include <fstream>
#include <vector>
#pragma comment(lib, "winhttp.lib")
#else
#include <cstdio>
#endif

namespace ProyecThor::Core {

#if defined(_WIN32)
std::string FetchGitHubJson(const std::string& path) {
    std::wstring wpath(path.begin(), path.end());

    HINTERNET hSession = WinHttpOpen(L"ProyecThor",
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(hSession, L"api.github.com",
                                         INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wpath.c_str(),
                                             NULL, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES,
                                             WINHTTP_FLAG_SECURE);
    std::string result;
    if (hRequest) {
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
    }

    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}
#else
std::string FetchGitHubJson(const std::string& path) {
    std::string cmd = "curl -s -H \"User-Agent: ProyecThor\" "
                       "\"https://api.github.com" + path + "\" 2>/dev/null";
    std::string result;
    char buffer[4096];
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), pipe)) > 0)
        result.append(buffer, n);
    pclose(pipe);
    return result;
}
#endif

std::string ExtractJsonField(const std::string& json, const std::string& key) {
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

std::string ExtractAssetUrl(const std::string& json, const std::string& assetName) {
    // El campo "browser_download_url" siempre aparece despues de "name"
    // dentro del mismo objeto de asset, asi que alcanza con buscar el
    // "name" pedido y extraer el proximo browser_download_url a partir de
    // ahi (funciona aunque el release tenga varios assets).
    std::string marker = "\"name\": \"" + assetName + "\"";
    auto pos = json.find(marker);
    if (pos == std::string::npos) {
        marker = "\"name\":\"" + assetName + "\""; // por si viene minificado
        pos = json.find(marker);
        if (pos == std::string::npos) return "";
    }
    return ExtractJsonField(json.substr(pos), "browser_download_url");
}

#if defined(_WIN32)
bool DownloadFile(const std::string& url, const std::string& destPath) {
    std::wstring wurl(url.begin(), url.end());

    wchar_t hostName[256] = {};
    wchar_t urlPath[2048] = {};
    URL_COMPONENTS uc = {};
    uc.dwStructSize    = sizeof(uc);
    uc.lpszHostName    = hostName;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath     = urlPath;
    uc.dwUrlPathLength = 2048;
    if (!WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.size(), 0, &uc)) return false;

    HINTERNET hSession = WinHttpOpen(L"ProyecThor",
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(hSession, uc.lpszHostName, uc.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", uc.lpszUrlPath, NULL,
                                             WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    bool ok = false;
    if (hRequest) {
        if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                               WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(hRequest, NULL)) {
            std::ofstream out(destPath, std::ios::binary);
            if (out) {
                DWORD size = 0;
                std::vector<char> buf;
                while (WinHttpQueryDataAvailable(hRequest, &size) && size > 0) {
                    buf.resize(size);
                    DWORD read = 0;
                    if (!WinHttpReadData(hRequest, buf.data(), size, &read)) break;
                    out.write(buf.data(), read);
                }
                ok = true;
            }
        }
        WinHttpCloseHandle(hRequest);
    }
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return ok;
}
#else
bool DownloadFile(const std::string&, const std::string&) {
    return false;
}
#endif

} // namespace ProyecThor::Core
