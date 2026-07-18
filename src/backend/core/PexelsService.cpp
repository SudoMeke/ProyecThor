#include "PexelsService.h"
#include "backend/settings/SettingsManager.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <thread>
#include <functional>
#include <cctype>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace ProyecThor::Core {

static double MonotonicNowSec() {
    using namespace std::chrono;
    static const auto start = steady_clock::now();
    return duration<double>(steady_clock::now() - start).count();
}

PexelsService& PexelsService::Get() {
    static PexelsService instance;
    return instance;
}

bool PexelsService::HasApiKey() const {
    return !ProyecThor::Settings::SettingsManager::Get().GetSettings().integrations.pexelsApiKey.empty();
}

bool PexelsService::CanSearchNow(std::string* reasonOut) const {
    auto setReason = [&](const std::string& s) { if (reasonOut) *reasonOut = s; };

    if (!HasApiKey()) {
        setReason("Configura tu API key de Pexels en Ajustes > Integraciones.");
        return false;
    }
    if (MonotonicNowSec() - m_LastRequestTime < kMinRequestGapSec) {
        setReason("Espera un instante entre busquedas...");
        return false;
    }
    if (m_Quota.known && m_Quota.remaining <= 0) {
        std::time_t resetT = (std::time_t)m_Quota.resetUnixTime;
        std::tm tmBuf{};
#ifdef _WIN32
        localtime_s(&tmBuf, &resetT);
#else
        localtime_r(&resetT, &tmBuf);
#endif
        char buf[32];
        std::strftime(buf, sizeof(buf), "%H:%M", &tmBuf);
        setReason(std::string("Limite de Pexels alcanzado. Se renueva a las ") + buf + ".");
        return false;
    }
    return true;
}

#ifndef _WIN32
// ─────────────────────────────────────────────────────────────────────────────
//  Escapado seguro para shell POSIX: envuelve en comillas simples y escapa
//  cualquier comilla simple embebida. Imprescindible antes de interpolar
//  texto NO confiable (busqueda escrita por el usuario, o la API key) en un
//  comando que se ejecuta via popen() (que corre a traves de /bin/sh).
// ─────────────────────────────────────────────────────────────────────────────
static std::string ShellQuotePosix(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
}

static std::string RunCommandCaptureStdout(const std::string& cmd) {
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return result;
    char buffer[4096];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), pipe)) > 0)
        result.append(buffer, n);
    pclose(pipe);
    return result;
}

static long long ParseHeaderInt(const std::string& headers, const std::string& key) {
    size_t pos = headers.find(key);
    if (pos == std::string::npos) return -1;
    pos += key.size();
    while (pos < headers.size() && (headers[pos] == ' ' || headers[pos] == ':')) pos++;
    return std::atoll(headers.c_str() + pos);
}
#endif

PexelsSearchResult PexelsService::Search(const std::string& query, int page, int perPage) {
    PexelsSearchResult out;

    std::string reason;
    if (!CanSearchNow(&reason)) {
        out.ok    = false;
        out.error = reason;
        return out;
    }

    std::string cacheKey = query + "|" + std::to_string(page) + "|" + std::to_string(perPage);
    auto cacheIt = m_Cache.find(cacheKey);
    if (cacheIt != m_Cache.end())
        return cacheIt->second;

#ifdef _WIN32
    out.ok    = false;
    out.error = "La busqueda de Pexels todavia no esta disponible en Windows.";
    return out;
#else
    m_LastRequestTime = MonotonicNowSec();

    const std::string apiKey =
        ProyecThor::Settings::SettingsManager::Get().GetSettings().integrations.pexelsApiKey;

    fs::path headerFile = fs::temp_directory_path() /
        ("proyecthor_pexels_hdr_" + std::to_string((unsigned long)std::hash<std::thread::id>{}(std::this_thread::get_id())) + ".tmp");

    std::ostringstream cmd;
    cmd << "curl -s -m 12 "
        << "-D " << ShellQuotePosix(headerFile.string()) << " "
        << "-G " << ShellQuotePosix("https://api.pexels.com/v1/search") << " "
        << "--data-urlencode " << ShellQuotePosix("query=" + query) << " "
        << "--data-urlencode " << ShellQuotePosix("per_page=" + std::to_string(perPage)) << " "
        << "--data-urlencode " << ShellQuotePosix("page=" + std::to_string(page)) << " "
        << "-H " << ShellQuotePosix("Authorization: " + apiKey) << " "
        << "-w " << ShellQuotePosix("\n__PROYECTHOR_HTTP_STATUS__:%{http_code}");

    std::string raw = RunCommandCaptureStdout(cmd.str());

    // Leer headers (rate limit) del archivo temporal y borrarlo.
    {
        std::ifstream hf(headerFile);
        if (hf.is_open()) {
            std::stringstream ss;
            ss << hf.rdbuf();
            std::string headers = ss.str();
            long long lim = ParseHeaderInt(headers, "X-Ratelimit-Limit");
            long long rem = ParseHeaderInt(headers, "X-Ratelimit-Remaining");
            long long rst = ParseHeaderInt(headers, "X-Ratelimit-Reset");
            if (lim >= 0 || rem >= 0) {
                m_Quota.known = true;
                if (lim >= 0) m_Quota.limit     = (int)lim;
                if (rem >= 0) m_Quota.remaining  = (int)rem;
                if (rst >= 0) m_Quota.resetUnixTime = rst;
            }
        }
        std::error_code ec;
        fs::remove(headerFile, ec);
    }

    // Separar el codigo HTTP (append -w) del cuerpo JSON.
    std::string marker = "__PROYECTHOR_HTTP_STATUS__:";
    size_t markerPos = raw.rfind(marker);
    int httpStatus = 0;
    std::string body = raw;
    if (markerPos != std::string::npos) {
        body = raw.substr(0, markerPos);
        httpStatus = std::atoi(raw.c_str() + markerPos + marker.size());
    }
    while (!body.empty() && (body.back() == '\n' || body.back() == '\r')) body.pop_back();

    if (body.empty()) {
        out.ok    = false;
        out.error = "No se pudo contactar a Pexels (revisa tu conexion).";
        return out;
    }

    json j;
    try {
        j = json::parse(body);
    } catch (...) {
        out.ok    = false;
        out.error = "Respuesta invalida de Pexels.";
        return out;
    }

    if (httpStatus == 401) {
        out.ok    = false;
        out.error = "API key de Pexels invalida.";
        return out;
    }
    if (httpStatus == 429) {
        out.ok    = false;
        out.error = "Se alcanzo el limite de peticiones de Pexels.";
        return out;
    }
    if (httpStatus != 200) {
        out.ok    = false;
        out.error = j.value("error", std::string("Error de Pexels (HTTP ") + std::to_string(httpStatus) + ").");
        return out;
    }

    out.ok           = true;
    out.totalResults = j.value("total_results", 0);
    if (j.contains("photos") && j["photos"].is_array()) {
        for (const auto& jp : j["photos"]) {
            PexelsPhoto photo;
            photo.id              = jp.value("id", 0LL);
            photo.width            = jp.value("width", 0);
            photo.height           = jp.value("height", 0);
            photo.photographer     = jp.value("photographer", "");
            photo.photographerUrl  = jp.value("photographer_url", "");
            if (jp.contains("src")) {
                photo.thumbnailUrl = jp["src"].value("medium", "");
                photo.downloadUrl  = jp["src"].value("large2x", jp["src"].value("large", ""));
            }
            out.photos.push_back(std::move(photo));
        }
    }

    m_Cache[cacheKey] = out;
    return out;
#endif
}

bool PexelsService::DownloadImage(const std::string& url, const std::string& destPath) {
#ifdef _WIN32
    (void)url; (void)destPath;
    return false;
#else
    if (url.empty()) return false;
    std::ostringstream cmd;
    cmd << "curl -s -m 20 -L -o " << ShellQuotePosix(destPath) << " " << ShellQuotePosix(url);
    std::string cmdStr = cmd.str();
    int status = std::system(cmdStr.c_str());
    if (status != 0) return false;
    std::error_code ec;
    return fs::exists(destPath, ec) && fs::file_size(destPath, ec) > 0;
#endif
}

} // namespace ProyecThor::Core
