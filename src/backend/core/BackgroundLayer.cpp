#include "BackgroundLayer.h"
#include <iostream>
#include <chrono>
#include <GL/glew.h>

namespace ProyecThor::Core {

    namespace {
        static GLuint s_QuadVAO  = 0;
        static GLuint s_QuadVBO  = 0;
        static GLuint s_BlitProg = 0;

        static const float k_QuadVerts[] = {
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
            -1.0f,  1.0f,  0.0f, 1.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f,
        };

        static const char* k_BlitVert = R"GLSL(
#version 330 core
layout(location = 0) in vec2 a_Pos;
layout(location = 1) in vec2 a_UV;
out vec2 v_UV;
void main() {
    v_UV        = a_UV;
    gl_Position = vec4(a_Pos, 0.0, 1.0);
}
)GLSL";

        static const char* k_BlitFrag = R"GLSL(
#version 330 core
in  vec2      v_UV;
out vec4      fragColor;
uniform sampler2D u_Tex;
void main() {
    fragColor = texture(u_Tex, v_UV);
}
)GLSL";

        static void EnsureBlitResources()
        {
            if (s_QuadVAO != 0) return;

            auto compile = [](GLenum type, const char* src) -> GLuint {
                GLuint id = glCreateShader(type);
                glShaderSource(id, 1, &src, nullptr);
                glCompileShader(id);
                GLint ok = 0;
                glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
                if (!ok) {
                    char log[512];
                    glGetShaderInfoLog(id, 512, nullptr, log);
                    std::cerr << "[BackgroundLayer] Shader error: " << log << "\n";
                    glDeleteShader(id);
                    return 0;
                }
                return id;
            };

            GLuint vert  = compile(GL_VERTEX_SHADER,   k_BlitVert);
            GLuint frag  = compile(GL_FRAGMENT_SHADER, k_BlitFrag);
            s_BlitProg   = glCreateProgram();
            glAttachShader(s_BlitProg, vert);
            glAttachShader(s_BlitProg, frag);
            glLinkProgram(s_BlitProg);
            glDeleteShader(vert);
            glDeleteShader(frag);

            glGenVertexArrays(1, &s_QuadVAO);
            glGenBuffers(1, &s_QuadVBO);
            glBindVertexArray(s_QuadVAO);
            glBindBuffer(GL_ARRAY_BUFFER, s_QuadVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(k_QuadVerts), k_QuadVerts, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                                  reinterpret_cast<void*>(0));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                                  reinterpret_cast<void*>(2 * sizeof(float)));
            glBindVertexArray(0);
        }

        static void BlitTexture(GLuint tex)
        {
            EnsureBlitResources();
            glUseProgram(s_BlitProg);
            glUniform1i(glGetUniformLocation(s_BlitProg, "u_Tex"), 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex);
            glBindVertexArray(s_QuadVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glUseProgram(0);
        }

        static double NowSeconds()
        {
            using namespace std::chrono;
            return duration<double>(steady_clock::now().time_since_epoch()).count();
        }
    } // anonymous namespace

    VLCBasePlayer& BackgroundLayer::Active()  { return m_ActiveIsA ? m_PlayerA : m_PlayerB; }
    VLCBasePlayer& BackgroundLayer::Standby() { return m_ActiveIsA ? m_PlayerB : m_PlayerA; }

    void BackgroundLayer::PerformSwap()
    {
        VLCBasePlayer& oldActive = Active();
        m_ActiveIsA = !m_ActiveIsA;
        VLCBasePlayer& newActive = Active();

        newActive.SetMute(m_TargetMuted);
        newActive.SetVolume(m_TargetMuted ? 0 : m_TargetVolume);
        newActive.SetPause(false);

        oldActive.SetMute(true);
        oldActive.Stop();

        m_SwapPending = false;
    }

    void BackgroundLayer::Update()
    {
        Active().UpdateTexture();

        if (m_SwapPending)
        {
            VLCBasePlayer& standby = Standby();

            bool ready    = standby.HasVideoFrame() && !standby.IsLoading();
            bool timedOut = (NowSeconds() - m_PendingSwapStart) > 3.0;

            if (ready || timedOut)
                PerformSwap();
        }
    }

    void BackgroundLayer::Render(int outputW, int outputH)
    {
        GLuint rawTex = static_cast<GLuint>(
            reinterpret_cast<uintptr_t>(Active().GetTextureID()));

        if (rawTex == 0)
            return;

        int srcW = 0, srcH = 0;
        Active().GetVideoSize(srcW, srcH);

        if (srcW <= 0 || srcH <= 0)
            return;

        int viewX = 0;
        int viewY = 0;
        int viewW = outputW;
        int viewH = outputH;

        if (!m_StretchToFill) {

            float videoRatio  = static_cast<float>(srcW) / static_cast<float>(srcH);
            float screenRatio = static_cast<float>(outputW) / static_cast<float>(outputH);

            if (videoRatio > screenRatio + 0.001f) {
                viewW = outputW;
                viewH = static_cast<int>(static_cast<float>(outputW) / videoRatio);
                viewX = 0;
                viewY = (outputH - viewH) / 2;
            } else if (videoRatio < screenRatio - 0.001f) {
                viewH = outputH;
                viewW = static_cast<int>(static_cast<float>(outputH) * videoRatio);
                viewX = (outputW - viewW) / 2;
                viewY = 0;
            }
        }

        GLuint finalTex = rawTex;

        if (m_FSREnabled) {
            bool needReinit = (!m_FSR.IsInitialized() ||
                               m_FSR.GetOutputW() != viewW ||
                               m_FSR.GetOutputH() != viewH);

            if (needReinit) {
                bool ok = m_FSR.Init(viewW, viewH);
                if (ok) {
                    m_FSR.SetSharpness(m_FSRSharpness);
                    m_FSR.SetEnabled(true);
                } else {
                    m_FSREnabled = false;
                    std::cout << "[BG] FSR no disponible, usando blit directo.\n";
                }
            }

            if (m_FSREnabled && m_FSR.IsInitialized() && (srcW < viewW || srcH < viewH)) {
                GLuint upscaled = m_FSR.Process(rawTex, srcW, srcH);
                if (upscaled != 0)
                    finalTex = upscaled;
            }
        }

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);

        glViewport(viewX, viewY, viewW, viewH);
        BlitTexture(finalTex);

        glViewport(0, 0, outputW, outputH);
    }

    void BackgroundLayer::SetStretchToFill(bool stretch)
    {
        m_StretchToFill = stretch;
    }

    bool BackgroundLayer::GetStretchToFill() const
    {
        return m_StretchToFill;
    }

    void BackgroundLayer::SetFSREnabled(bool enabled)
    {
        m_FSREnabled = enabled;
        m_FSR.SetEnabled(enabled);
    }

    bool BackgroundLayer::GetFSREnabled() const
    {
        return m_FSREnabled;
    }

    void BackgroundLayer::SetFSRSharpness(float sharpness)
    {
        m_FSRSharpness = sharpness;
        m_FSR.SetSharpness(sharpness);
    }

    float BackgroundLayer::GetFSRSharpness() const
    {
        return m_FSRSharpness;
    }

    void* BackgroundLayer::GetTextureID()
    {
        return Active().GetTextureID();
    }

    VLCBasePlayer* BackgroundLayer::GetPlayer()
    {
        return &Active();
    }

    void BackgroundLayer::SetVideo(const std::string& path)
    {
        m_IsVideo = true;

        if (m_SwapPending || GetTextureID() != nullptr)
        {
            // Ya hay algo visible (o un swap en curso): precargar en
            // standby y esperar a que tenga un frame real. El clip que ve
            // el publico sigue reproduciendose sin interrupcion mientras
            // tanto — cero congelamiento, cero corte de audio.
            Standby().Play(path, /*loop=*/false, /*startMuted=*/true);
            m_SwapPending      = true;
            m_PendingSwapStart = NowSeconds();
        }
        else
        {
            // No hay nada visible todavia: reproducir directo, no hay
            // nada que proteger de un corte.
            Active().Play(path, /*loop=*/false, /*startMuted=*/true);
        }
    }

    void BackgroundLayer::SetSolidColor(float r, float g, float b)
    {
        m_IsVideo    = false;
        m_BgColor[0] = r;
        m_BgColor[1] = g;
        m_BgColor[2] = b;

        m_SwapPending = false;
        m_PlayerA.Stop();
        m_PlayerB.Stop();
    }

    void* BackgroundLayer::GetProcessedTexture(int targetW, int targetH) {
        GLuint rawTex = static_cast<GLuint>(reinterpret_cast<uintptr_t>(Active().GetTextureID()));
        if (rawTex == 0 || targetW <= 0 || targetH <= 0)
            return nullptr;

        if (!m_FSREnabled)
            return (void*)(uintptr_t)rawTex;

        int srcW = 0, srcH = 0;
        Active().GetVideoSize(srcW, srcH);

        if (srcW <= 0 || srcH <= 0 || (srcW >= targetW && srcH >= targetH))
            return (void*)(uintptr_t)rawTex;

        bool needReinit = (!m_FSR.IsInitialized() ||
                           m_FSR.GetOutputW() != targetW ||
                           m_FSR.GetOutputH() != targetH);

        if (needReinit) {
            if (m_FSR.Init(targetW, targetH)) {
                m_FSR.SetSharpness(m_FSRSharpness);
                m_FSR.SetEnabled(true);
            } else {
                m_FSREnabled = false;
                return (void*)(uintptr_t)rawTex;
            }
        }

        GLuint upscaled = m_FSR.Process(rawTex, srcW, srcH);
        return upscaled ? (void*)(uintptr_t)upscaled : (void*)(uintptr_t)rawTex;
    }

    void BackgroundLayer::SetLiveVolume(int volume0to200)
    {
        m_TargetVolume = volume0to200;
        Active().SetVolume(m_TargetMuted ? 0 : m_TargetVolume);
    }

    void BackgroundLayer::SetLiveMute(bool mute)
    {
        m_TargetMuted = mute;
        Active().SetMute(mute);
        Active().SetVolume(mute ? 0 : m_TargetVolume);
    }

    void BackgroundLayer::BlockPath(const std::string& path)
    {
        m_PlayerA.BlockPath(path);
        m_PlayerB.BlockPath(path);
    }

    void BackgroundLayer::UnblockPath()
    {
        m_PlayerA.UnblockPath();
        m_PlayerB.UnblockPath();
    }

} // namespace ProyecThor::Core