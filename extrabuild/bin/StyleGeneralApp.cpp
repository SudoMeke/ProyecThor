#include "StyleGeneralApp.h"
#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "stb_image.h" 

void* LoadTextureFromFile(const char* filename, int* width, int* height) {
    int channels;
    // Forzamos 4 canales (RGBA) para asegurar compatibilidad con GL_RGBA
    unsigned char* data = stbi_load(filename, width, height, &channels, 4);
    
    if (data == NULL) {
        // stbi_failure_reason() te dirá EXACTAMENTE por qué falló (ej. "can't fopen")
        std::cerr << "[ERROR] stb_image no pudo cargar: " << filename 
                  << " | Motivo: " << stbi_failure_reason() << std::endl;
        return NULL;
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    // IMPORTANTE: Evita errores con imágenes que no son potencia de 2 (NPOT)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Filtrado (Linear para suavizado, cambiar a GL_NEAREST para Pixel Art puro)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Subir a la VRAM
    // Nota: Como forzamos 4 canales en stbi_load, es seguro usar GL_RGBA
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, *width, *height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    
    stbi_image_free(data);

    std::cout << "[INFO] Textura cargada con éxito: " << filename << " (ID: " << texture << ")" << std::endl;

    return (void*)(intptr_t)texture;
}

void StyleGeneralApp::LoadAppIcon(const std::string& name, const std::string& path) {

    int width = 0, height = 0;
    
    // Verificamos si hay un contexto OpenGL activo antes de intentar generar la textura
    if (glfwGetCurrentContext() == nullptr) {
        std::cerr << "[FATAL ERROR] Intentando cargar icono '" << name 
                  << "' pero NO hay contexto OpenGL activo." << std::endl;
        return;
    }

    void* texId = LoadTextureFromFile(path.c_str(), &width, &height);
    if (texId) {
        Icons[name] = { texId, width, height };
    } else {
        std::cerr << "[ERROR] Fallo al registrar el icono: " << name << std::endl;
    }
}