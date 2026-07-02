#pragma once
#include <string>
#include <windows.h>
#include <shlobj.h>

namespace ProyecThor {
    inline const std::string& GetAssetsPath() {
        static std::string s_AssetsPath;
        if (!s_AssetsPath.empty()) return s_AssetsPath;

        char appDataBuf[MAX_PATH] = {};
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appDataBuf))) {
            s_AssetsPath = std::string(appDataBuf) + "\\ProyecThor\\assets";
        } else {
            s_AssetsPath = "assets";
        }
        return s_AssetsPath;
    }

    inline std::string SongsPath()     { return GetAssetsPath() + "/songs/";     }
    inline std::string VideosPath()    { return GetAssetsPath() + "/videos/";    }
    inline std::string ImagesPath()    { return GetAssetsPath() + "/images/";    }
    inline std::string BiblesPath()    { return GetAssetsPath() + "/bibles/";    }
    inline std::string DocumentsPath() { return GetAssetsPath() + "/documents/"; }

} // namespace ProyecThor
