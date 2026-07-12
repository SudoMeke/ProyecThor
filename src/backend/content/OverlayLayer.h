#pragma once

#include "VLCBasePlayer.h"
#include <filesystem>
#include <vector>
#include <iostream>
#include <GL/glew.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
    #include <limits.h>
#endif

namespace ProyecThor::Core {

    class OverlayLayer {
    private:
        VLCBasePlayer m_Player;
        bool m_IsActive = false;

        std::filesystem::path GetAppDir() {
        #ifdef _WIN32
            wchar_t buffer[MAX_PATH];
            GetModuleFileNameW(NULL, buffer, MAX_PATH);
            return std::filesystem::path(buffer).parent_path();
        #else
            char buffer[PATH_MAX];
            ssize_t count = readlink("/proc/self/exe", buffer, PATH_MAX);
            if (count != -1) {
                return std::filesystem::path(std::string(buffer, count)).parent_path();
            }
            return std::filesystem::current_path();
        #endif
        }

    public:
        OverlayLayer() = default;
        ~OverlayLayer() = default;

        void Update() { 
            if (m_IsActive) {
                m_Player.UpdateTexture(); 
            }
        }

        void* GetTextureID() { 
            return m_Player.GetTextureID(); 
        }
        
        VLCBasePlayer* GetPlayer() { return &m_Player; }
        
        void PlayOverlay(const std::string& path) {
            if (path.empty()) return;

            std::filesystem::path p(path);
            std::filesystem::path finalPath;

            if (std::filesystem::exists(p)) {
                finalPath = std::filesystem::absolute(p);
            } 
            else {
                std::vector<std::filesystem::path> searchLocations = {
                    GetAppDir() / "assets" / "videos" / p.filename(),
                    GetAppDir() / "videos" / p.filename(),
                    std::filesystem::current_path() / "assets" / "videos" / p.filename()
                };

                for (const auto& loc : searchLocations) {
                    if (std::filesystem::exists(loc)) {
                        finalPath = std::filesystem::absolute(loc);
                        break;
                    }
                }
            }

            if (finalPath.empty()) {
                std::cerr << "[Overlay] Error: No se pudo localizar el archivo: " << path << "\n";
                return;
            }

            m_IsActive = true;
            m_Player.Play(finalPath.string(), false); 
        }

        void StopOverlay() {
            m_IsActive = false;
            m_Player.Stop();
        }

        bool IsActive() const { return m_IsActive; }

       void Render() {
    if (!m_IsActive) return;

    void* texID = m_Player.GetTextureID();
    if (!texID) return;

    // BackgroundLayer::Render() SOLO limpia el shader activo (glUseProgram(0))
    // cuando dibuja un fondo de video, via BlitTexture(). Si el fondo es un
    // color solido, BackgroundLayer::Render() retorna temprano sin tocar el
    // shader, y lo que haya quedado bound (tipicamente el shader de ImGui,
    // ya que este Render() corre entre llamadas de construccion de UI)
    // sigue activo. El pipeline fijo de abajo (glBegin/glEnd) no funciona
    // correctamente con un shader program bound: el resultado es
    // indefinido, casi siempre negro. Por eso forzamos glUseProgram(0)
    // aca, sin depender de que BackgroundLayer haya corrido antes.
    GLint prevProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);
    glUseProgram(0);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
    
    glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)texID);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f,  1.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0f,  1.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0f, -1.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, -1.0f);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);

    // Restauramos el shader por si algo despues de esta llamada (otro
    // layer, ImGui, etc.) asume que sigue activo.
    glUseProgram(static_cast<GLuint>(prevProgram));
}
    };

} // namespace ProyecThor::Core