#include "FilePicker.h"
#include <filesystem>
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#else
#include <cstdio>
#endif

namespace fs = std::filesystem;

namespace ProyecThor::UI {

#ifdef _WIN32
std::string PickImageOrVideoFile() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dlg))))
        return {};

    COMDLG_FILTERSPEC filters[] = {
        {L"Video e Imagen", L"*.mp4;*.mkv;*.avi;*.mov;*.jpg;*.jpeg;*.png"},
        {L"Videos",         L"*.mp4;*.mkv;*.avi;*.mov"},
        {L"Imagenes",       L"*.jpg;*.jpeg;*.png"},
    };
    dlg->SetFileTypes(3, filters);
    dlg->SetFileTypeIndex(1);
    dlg->SetTitle(L"Elegir imagen o video");

    std::string result;
    if (SUCCEEDED(dlg->Show(nullptr))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR pp = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &pp))) {
                int len = WideCharToMultiByte(CP_UTF8, 0, pp, -1, nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    result.resize(len - 1);
                    WideCharToMultiByte(CP_UTF8, 0, pp, -1, result.data(), len, nullptr, nullptr);
                }
                CoTaskMemFree(pp);
            }
            item->Release();
        }
    }
    dlg->Release();
    return result;
}

std::string PickImageFile() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dlg))))
        return {};

    COMDLG_FILTERSPEC filters[] = {
        {L"Imagenes", L"*.jpg;*.jpeg;*.png"},
    };
    dlg->SetFileTypes(1, filters);
    dlg->SetFileTypeIndex(1);
    dlg->SetTitle(L"Elegir imagen");

    std::string result;
    if (SUCCEEDED(dlg->Show(nullptr))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR pp = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &pp))) {
                int len = WideCharToMultiByte(CP_UTF8, 0, pp, -1, nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    result.resize(len - 1);
                    WideCharToMultiByte(CP_UTF8, 0, pp, -1, result.data(), len, nullptr, nullptr);
                }
                CoTaskMemFree(pp);
            }
            item->Release();
        }
    }
    dlg->Release();
    return result;
}
#else
static std::string RunFilePickerCommands(const char* const commands[], size_t count) {
    for (size_t i = 0; i < count; ++i) {
        char buffer[1024];
        std::string result;

        FILE* pipe = popen(commands[i], "r");
        if (!pipe) continue;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) result += buffer;
        int status = pclose(pipe);
        if (status != 0) continue; // cancelado o la herramienta no esta instalada

        while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();
        if (!result.empty()) return result;
    }
    return {};
}

std::string PickImageOrVideoFile() {
    const char* commands[] = {
        "zenity --file-selection --title=\"Elegir imagen o video\" "
        "--file-filter=\"Video e Imagen | *.mp4 *.mkv *.avi *.mov *.jpg *.jpeg *.png\" 2>/dev/null",
        "kdialog --getopenfilename . "
        "\"*.mp4 *.mkv *.avi *.mov *.jpg *.jpeg *.png|Video e Imagen\" 2>/dev/null"
    };
    return RunFilePickerCommands(commands, 2);
}

std::string PickImageFile() {
    const char* commands[] = {
        "zenity --file-selection --title=\"Elegir imagen\" "
        "--file-filter=\"Imagenes | *.jpg *.jpeg *.png\" 2>/dev/null",
        "kdialog --getopenfilename . \"*.jpg *.jpeg *.png|Imagenes\" 2>/dev/null"
    };
    return RunFilePickerCommands(commands, 2);
}
#endif

bool LooksLikeVideoPath(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".mp4" || ext == ".mkv" || ext == ".avi" || ext == ".mov";
}

} // namespace ProyecThor::UI
