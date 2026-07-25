#pragma once
#include <string>

namespace ProyecThor::Core {

// ─────────────────────────────────────────────────────────────────────────────
//  GitHubRelease — utilidades minimas para consultar la API publica de
//  GitHub (releases). Pensado para chequeos de version de apps hermanas
//  (ver LayersOverlayTab::RenderFoudreVueDownloadModal). En _WIN32 usa
//  WinHTTP (mismo mecanismo que el autoupdater de CategoryUpdates.cpp); en
//  el resto de plataformas usa "curl" via popen, igual criterio que ya usa
//  el propio codebase para apoyarse en herramientas externas (zenity en
//  LayersOverlayTab::ImportBundle, xdg-open en OpenURL.cpp) en vez de
//  agregarle soporte OpenSSL a httplib solo para esto.
// ─────────────────────────────────────────────────────────────────────────────

// GET a https://api.github.com<path>. Devuelve el body, o "" si fallo.
std::string FetchGitHubJson(const std::string& path);

// Extrae el primer valor string de "key" en un JSON (parseo por texto,
// suficiente para los campos planos que necesitamos: tag_name, html_url,
// published_at, message).
std::string ExtractJsonField(const std::string& json, const std::string& key);

// Busca, dentro del array "assets" de un release, el asset cuyo "name" sea
// exactamente assetName y devuelve su "browser_download_url" (o "" si el
// release no tiene ese asset). Pensado para instalacion automatica de apps
// hermanas (ver LayersOverlayTab::InstallAndLaunchFoudreVue).
std::string ExtractAssetUrl(const std::string& json, const std::string& assetName);

// Descarga "url" (siguiendo redirecciones, ej. hacia el CDN de GitHub) y lo
// guarda en destPath. Solo implementado en _WIN32 por ahora (via WinHTTP);
// en el resto de plataformas siempre devuelve false.
bool DownloadFile(const std::string& url, const std::string& destPath);

} // namespace ProyecThor::Core
