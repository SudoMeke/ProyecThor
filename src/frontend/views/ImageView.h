#pragma once

#ifdef _WIN32
    #include <windows.h>
#endif
#include <GL/glew.h>
#include <string>

namespace ProyecThor::UI {

    struct ImageAdjustments {
        float brightness  =  0.0f;   // -1.0 .. +1.0
        float contrast    =  1.0f;   //  0.0 .. +3.0
        float saturation  =  1.0f;   //  0.0 .. +3.0
        float hue         =  0.0f;   // -180 .. +180  (grados)
        float temperature =  0.0f;   // -1.0 .. +1.0  (frio/calido)
        float sharpness   =  0.0f;   //  0.0 .. +1.0
        float gamma       =  1.0f;   //  0.1 .. +3.0
        float vignette    =  0.0f;   //  0.0 .. +1.0
    };

    class ImageView {
    public:
        ImageView();
        ~ImageView();

        bool LoadImageFromFile(const std::string& path);
        void Render(float maxWidth, float maxHeight);
        void RenderAdjustmentsPanel();
        void ResetAdjustments();
        void Clear();

        GLuint GetTextureID() const { return m_TextureID; }

    private:
        void InitShader();
        void InitQuad();
        void DestroyShader();
        void DestroyQuad();
        void ApplyRenderWithShader(float renderWidth, float renderHeight,
                                   float offsetX, float offsetY);

        GLuint m_TextureID   = 0;
        int    m_Width       = 0;
        int    m_Height      = 0;

        // Shader pipeline
        GLuint m_ShaderProgram = 0;
        GLuint m_VAO           = 0;
        GLuint m_VBO           = 0;
        GLuint m_FBO           = 0;
        GLuint m_OutputTex     = 0;
        int    m_FBOWidth      = 0;
        int    m_FBOHeight     = 0;

        // Uniform locations (cacheados)
        GLint m_uTexture     = -1;
        GLint m_uBrightness  = -1;
        GLint m_uContrast    = -1;
        GLint m_uSaturation  = -1;
        GLint m_uHue         = -1;
        GLint m_uTemperature = -1;
        GLint m_uSharpness   = -1;
        GLint m_uGamma       = -1;
        GLint m_uVignette    = -1;
        GLint m_uTexSize     = -1;

        ImageAdjustments m_Adj;
        bool m_ShaderReady = false;
    };

} // namespace ProyecThor::UI