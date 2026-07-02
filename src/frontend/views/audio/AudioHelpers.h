#pragma once

// AudioHelpers.h — helpers de encoding UTF-8/UTF-16 y construccion de URIs VLC
// Header-only, incluir solo desde AudioPanel.cpp

#include <string>
#include <cstdio>
#include <cctype>
#include <vector>

// Dependencias gráficas y de interfaz
#include <imgui.h>
#include <GL/gl.h> // Ajusta esto si usas glad, glew, o el loader de ProyecThor

// stb_image para decodificar las portadas
// NOTA: Asegúrate de tener #define STB_IMAGE_IMPLEMENTATION en UNO de tus archivos .cpp 
// antes de incluir stb_image.h, o descoméntalo aquí si estás 100% seguro de que 
// AudioHelpers.h solo se incluye una vez en todo el proyecto.
// #define STB_IMAGE_IMPLEMENTATION 
#include "stb_image.h"

#ifdef _WIN32
    #include <windows.h>
    #include <shlobj.h>
    #include <commdlg.h>
#endif

namespace ProyecThor::Audio {

// ─── UTF-8 / UTF-16 ──────────────────────────────────────────────────────────

#ifdef _WIN32

inline std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring wide(size - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wide.data(), size);
    return wide;
}

inline std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1,
                                   nullptr, 0, nullptr, nullptr);
    std::string utf8(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1,
                        utf8.data(), size, nullptr, nullptr);
    return utf8;
}

inline std::string PathToVLCUri(const std::string& utf8path) {
    std::string uri = "file:///";
    for (unsigned char c : utf8path) {
        if (c == '\\') {
            uri += '/';
        } else if (std::isalnum(c) || c == '/' || c == '.'
                || c == '-' || c == '_' || c == ':') {
            uri += static_cast<char>(c);
        } else {
            char buf[4];
            std::snprintf(buf, sizeof(buf), "%%%02X", c);
            uri += buf;
        }
    }
    return uri;
}

#else

inline std::string WideToUtf8(const std::string& s) { return s; }
inline std::string Utf8ToWide(const std::string& s)  { return s; }

#endif // _WIN32

// ─── Carga de Texturas (OpenGL) ──────────────────────────────────────────────

inline ImTextureID LoadTextureFromMemory(const unsigned char* data, int size) {
    if (!data || size <= 0) return (ImTextureID)0;

    int width, height, channels;
    // Forzar 4 canales (RGBA) para compatibilidad con ImGui
    unsigned char* image_data = stbi_load_from_memory(data, size, &width, &height, &channels, 4);
    if (!image_data) return (ImTextureID)0;

    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    // Parámetros de escalado lineal para que el disco rotatorio se vea suave
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Subir la textura a la memoria de la GPU
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    
    // Liberar la memoria RAM una vez que la imagen está en la GPU
    stbi_image_free(image_data);

    return (ImTextureID)(intptr_t)texture_id;
}

// ─── Ruta de la carpeta de audio ─────────────────────────────────────────────

inline const std::string& GetAudioPath() {
    static std::string s_Path;
    if (!s_Path.empty()) return s_Path;

#ifdef _WIN32
    char buf[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr,
                                   SHGFP_TYPE_CURRENT, buf)))
        s_Path = std::string(buf) + "\\ProyecThor\\assets\\audio";
    else
        s_Path = "assets/audio";
#else
    const char* home = std::getenv("HOME");
    s_Path = home ? std::string(home) + "/.ProyecThor/assets/audio"
                  : "assets/audio";
#endif
    return s_Path;
}

} // namespace ProyecThor::Audio