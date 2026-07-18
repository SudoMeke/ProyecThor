#include "GitHubRelease.h"

#if defined(_WIN32)
#include <windows.h>
#include <winhttp.h>
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

} // namespace ProyecThor::Core
