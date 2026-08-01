#pragma once
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <cstdlib>
#include <filesystem>
#endif

namespace ProyecThor {
    inline const std::string& GetAssetsPath() {
        static std::string s_AssetsPath;
        if (!s_AssetsPath.empty()) return s_AssetsPath;

#ifdef _WIN32
        char appDataBuf[MAX_PATH] = {};
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appDataBuf))) {
            s_AssetsPath = std::string(appDataBuf) + "\\ProyecThor\\assets";
        } else {
            s_AssetsPath = "assets";
        }
#else
        const char* home = std::getenv("HOME");
        if (home) {
            std::string dir = std::string(home) + "/.local/share/ProyecThor/assets";
            std::filesystem::create_directories(dir);
            s_AssetsPath = dir;
        } else {
            s_AssetsPath = "assets";
        }
#endif
        return s_AssetsPath;
    }

    inline std::string SongsPath()     { return GetAssetsPath() + "/songs/";     }
    inline std::string VideosPath()    { return GetAssetsPath() + "/videos/";    }
    inline std::string ImagesPath()    { return GetAssetsPath() + "/images/";    }
    inline std::string BiblesPath()    { return GetAssetsPath() + "/bibles/";    }
    inline std::string DocumentsPath() { return GetAssetsPath() + "/documents/"; }

    // Imagenes propias de la app (ej. el Logo de pantalla de carga, ver
    // Ajustes > Proyeccion): igual que Fondos (LayersBgTab::BgRootDir), los
    // archivos elegidos se COPIAN aca en vez de guardar la ruta externa tal
    // cual — asi quedan junto con el resto de los datos de la app y no se
    // rompen si el archivo original se mueve/borra/no existe en otra
    // maquina.
    inline std::string BrandingPath()  { return GetAssetsPath() + "/branding/";  }

    // Raiz real de AppData\ProyecThor (un nivel arriba de assets/): ahi
    // tambien viven settings.json, songs_authors.ini, themes/, etc. Usada
    // por SyncServer para sincronizar TODO el arbol de datos del usuario,
    // no solo assets/ -- ver SyncServer.cpp.
    inline std::string GetAppDataRoot() {
        std::string assets = GetAssetsPath(); // ".../ProyecThor/assets"
        const std::string suffix = "/assets";
        if (assets.size() > suffix.size() &&
            assets.compare(assets.size() - suffix.size(), suffix.size(), suffix) == 0) {
            return assets.substr(0, assets.size() - suffix.size());
        }
        return assets;
    }

} // namespace ProyecThor