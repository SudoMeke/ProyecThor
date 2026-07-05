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

} // namespace ProyecThor