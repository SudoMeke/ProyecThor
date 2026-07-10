#include "BackgroundLayer.h"
#include "frontend/windowing/SecondaryOutputWindow.h"
#include <iostream>
#include <chrono>
#include <GL/glew.h>
#include <unordered_map>

namespace ProyecThor::Core {

    namespace {
        struct BlitResources {
            GLuint vao = 0, vbo = 0, prog = 0;
        };

        static std::unordered_map<GLFWwindow*, BlitResources> s_ResourcesPerContext;

        // Registrado una sola vez: cuando una ventana secundaria (Proyector,
        // Stage, o cualquier otra a futuro) se destruye, purgamos su entrada
        // del cache. Sin esto, si GLFW reutiliza esa direccion de puntero
        // para una ventana nueva, BlitTexture() bindearia un VAO de un
        // contexto GL que ya no existe.
        static bool s_DestroyHookRegistered = [] {
            SecondaryOutputWindow::RegisterContextDestroyCallback(
                [](GLFWwindow* ctx) { s_ResourcesPerContext.erase(ctx); });
            return true;
        }();

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
uniform float     u_Alpha;
void main() {
    vec4 c = texture(u_Tex, v_UV);
    fragColor = vec4(c.rgb, c.a * u_Alpha);
}
)GLSL";

        static BlitResources& EnsureBlitResources()
        {
            GLFWwindow* ctx = glfwGetCurrentContext();
            auto it = s_ResourcesPerContext.find(ctx);
            if (it != s_ResourcesPerContext.end())
                return it->second;

            BlitResources res;
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

            GLuint vert = compile(GL_VERTEX_SHADER,   k_BlitVert);
            GLuint frag = compile(GL_FRAGMENT_SHADER, k_BlitFrag);
            res.prog = glCreateProgram();
            glAttachShader(res.prog, vert);
            glAttachShader(res.prog, frag);
            glLinkProgram(res.prog);
            glDeleteShader(vert);
            glDeleteShader(frag);

            glGenVertexArrays(1, &res.vao);
            glGenBuffers(1, &res.vbo);
            glBindVertexArray(res.vao);
            glBindBuffer(GL_ARRAY_BUFFER, res.vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(k_QuadVerts), k_QuadVerts, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
            glBindVertexArray(0);

            return s_ResourcesPerContext.emplace(ctx, res).first->second;
        }

        static void BlitTexture(GLuint tex, float alpha = 1.0f)
        {
            BlitResources& res = EnsureBlitResources();
            glUseProgram(res.prog);
            glUniform1i(glGetUniformLocation(res.prog, "u_Tex"), 0);
            glUniform1f(glGetUniformLocation(res.prog, "u_Alpha"), alpha);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex);
            glBindVertexArray(res.vao);
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
    } // anonymous namespace   <-- ESTO FALTABA

    BackgroundLayer::BackgroundLayer(bool forceSilentAudio)
        : m_PlayerA(2, true, forceSilentAudio)
        , m_PlayerB(2, true, forceSilentAudio)
    {
    }

    VLCBasePlayer& BackgroundLayer::Active()  { return m_ActiveIsA ? m_PlayerA : m_PlayerB; }
    VLCBasePlayer& BackgroundLayer::Standby() { return m_ActiveIsA ? m_PlayerB : m_PlayerA; }

void BackgroundLayer::Update()
{
    Active().UpdateTexture();
    Active().EnforceSilenceIfNeeded();
    Standby().EnforceSilenceIfNeeded();

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

      if (m_SwapPending && Standby().HasVideoFrame())
        {
            GLuint standbyTex = static_cast<GLuint>(reinterpret_cast<uintptr_t>(Standby().GetTextureID()));
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glViewport(viewX, viewY, viewW, viewH);
            BlitTexture(finalTex,   1.0f - m_TransitionProgress);
            BlitTexture(standbyTex, m_TransitionProgress);
            glDisable(GL_BLEND);
        }
        else
        {
            glDisable(GL_BLEND);
            glViewport(viewX, viewY, viewW, viewH);
            BlitTexture(finalTex);
        }

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

   void BackgroundLayer::SetVideo(const std::string& path, bool allowAudio)
{
    m_IsVideo = true;
    m_ContentAllowsAudio = allowAudio;   // <-- se fija ANTES de reproducir

    if (m_SwapPending || GetTextureID() != nullptr)
    {
        Standby().Play(path, /*loop=*/false, /*startMuted=*/true);
        Standby().SetAudioActive(false);
        m_SwapPending      = true;
        m_PendingSwapStart = NowSeconds();
    }
    else
    {
        Active().Play(path, /*loop=*/false, /*startMuted=*/true);
        if (!m_IsLiveToPublic || !allowAudio)
            Active().SetAudioActive(false);
    }
}

void BackgroundLayer::PerformSwap()
{
    VLCBasePlayer& oldActive = Active();
    m_ActiveIsA = !m_ActiveIsA;
    VLCBasePlayer& newActive = Active();

    // Ahora el swap respeta el permiso asociado al contenido que se esta
    // por mostrar, no solo el estado global "al aire".
    if (m_IsLiveToPublic && m_ContentAllowsAudio)
    {
        newActive.SetAudioActive(true);
        newActive.SetMute(m_TargetMuted);
        newActive.SetVolume(m_TargetMuted ? 0 : m_TargetVolume);
    }
    else
    {
        newActive.SetAudioActive(false);
    }
    newActive.SetPause(false);

    oldActive.SetAudioActive(false);
    oldActive.SetMute(true);
    oldActive.Stop();

    m_SwapPending = false;
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

    void BackgroundLayer::SetPubliclyLive(bool live)
    {
        m_IsLiveToPublic = live;

        if (live)
        {
            // Al pasar a "en vivo", el player activo adopta el target de
            // volumen/mute que el operador ya haya configurado (ver
            // SetLiveVolume/SetLiveMute). El standby se mantiene mudo:
            // solo el que el publico ve puede sonar.
            Active().SetAudioActive(true);
            Active().SetMute(m_TargetMuted);
            Active().SetVolume(m_TargetMuted ? 0 : m_TargetVolume);
            Standby().SetAudioActive(false);
        }
        else
        {
            // Cortar audio de raiz en ambos players, sin importar el
            // volumen/mute configurado.
            m_PlayerA.SetAudioActive(false);
            m_PlayerB.SetAudioActive(false);
        }
    }

    void BackgroundLayer::SetLiveVolume(int volume0to200)
    {
        m_TargetVolume = volume0to200;
        if (m_IsLiveToPublic)
            Active().SetVolume(m_TargetMuted ? 0 : m_TargetVolume);
    }

    void BackgroundLayer::SetLiveMute(bool mute)
    {
        m_TargetMuted = mute;
        if (m_IsLiveToPublic)
        {
            Active().SetMute(mute);
            Active().SetVolume(mute ? 0 : m_TargetVolume);
        }
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