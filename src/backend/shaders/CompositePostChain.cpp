#include "CompositePostChain.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <vector>

namespace ProyecThor::Shaders {

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
in  vec2 v_UV;
out vec4 fragColor;
uniform sampler2D u_Tex;
void main() { fragColor = texture(u_Tex, v_UV); }
)GLSL";

static GLuint CompileStage(GLenum type, const char* src) {
    GLuint id = glCreateShader(type);
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);
    GLint ok = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(id, 512, nullptr, log);
        std::cerr << "[CompositePostChain] Shader error: " << log << "\n";
        glDeleteShader(id);
        return 0;
    }
    return id;
}

void CompositePostChain::EnsureBlitProgram() {
    if (m_BlitProgram) return;

    GLuint vert = CompileStage(GL_VERTEX_SHADER, k_BlitVert);
    GLuint frag = CompileStage(GL_FRAGMENT_SHADER, k_BlitFrag);
    m_BlitProgram = glCreateProgram();
    glAttachShader(m_BlitProgram, vert);
    glAttachShader(m_BlitProgram, frag);
    glLinkProgram(m_BlitProgram);
    glDeleteShader(vert);
    glDeleteShader(frag);

    glGenVertexArrays(1, &m_BlitVAO);
    glGenBuffers(1, &m_BlitVBO);
    glBindVertexArray(m_BlitVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_BlitVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(k_QuadVerts), k_QuadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
}

void CompositePostChain::BlitToCurrentFramebuffer(unsigned int tex, int w, int h) {
    glViewport(0, 0, w, h);
    glUseProgram(m_BlitProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(glGetUniformLocation(m_BlitProgram, "u_Tex"), 0);
    glBindVertexArray(m_BlitVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

void CompositePostChain::CreateCaptureFBO(int w, int h) {
    glGenTextures(1, &m_CaptureTex);
    glBindTexture(GL_TEXTURE_2D, m_CaptureTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &m_CaptureFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_CaptureFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_CaptureTex, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "[CompositePostChain] Capture FBO incompleto (status=0x" << std::hex << status << std::dec << ")\n";
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CompositePostChain::DestroyCaptureFBO() {
    if (m_CaptureFBO) { glDeleteFramebuffers(1, &m_CaptureFBO); m_CaptureFBO = 0; }
    if (m_CaptureTex) { glDeleteTextures(1, &m_CaptureTex); m_CaptureTex = 0; }
}

void CompositePostChain::EnsureSized(int w, int h, void* platformHandle) {
    bool contextChanged = (platformHandle != m_LastPlatformHandle);
    bool sizeChanged    = (w != m_W || h != m_H);

    if (!contextChanged && !sizeChanged && m_SubEffectsInitialized)
        return;

    if (contextChanged) {
        // La ventana nativa de "ProjectorLive" se recreo (ej. Audiencia
        // apagada/prendida) -- el contexto GL viejo, y todo lo que tenia,
        // ya fue destruido por GLFW. Los IDs numericos que teniamos
        // cacheados son ajenos al contexto nuevo: "olvidarlos" sin
        // glDelete* evita el riesgo de borrar, por coincidencia de
        // numero, un objeto legitimo recien creado en el contexto nuevo.
        m_CaptureFBO = m_CaptureTex = 0;
        m_BlitProgram = m_BlitVAO = m_BlitVBO = 0;
        m_CRT.ForgetGLResources();
        m_Grain.ForgetGLResources();
        m_FXAA.ForgetGLResources();
        m_Saturation.ForgetGLResources();
        m_Vignette.ForgetGLResources();
        m_SubEffectsInitialized = false;
    } else {
        // Mismo contexto: el resize normal, con glDelete* real, es seguro.
        DestroyCaptureFBO();
    }

    m_W = w;
    m_H = h;
    m_LastPlatformHandle = platformHandle;

    CreateCaptureFBO(w, h);
    EnsureBlitProgram();

    if (!m_SubEffectsInitialized) {
        m_CRT.Init(w, h);
        m_Grain.Init(w, h);
        m_FXAA.Init(w, h);
        m_Saturation.Init(w, h);
        m_Vignette.Init(w, h);
        m_SubEffectsInitialized = true;
    } else {
        m_CRT.Resize(w, h);
        m_Grain.Resize(w, h);
        m_FXAA.Resize(w, h);
        m_Saturation.Resize(w, h);
        m_Vignette.Resize(w, h);
    }
}

void CompositePostChain::RenderViewport(ImGuiViewport* viewport,
                                        void (*defaultRenderFn)(ImGuiViewport*, void*))
{
    if (!AnyEnabled()) {
        if (defaultRenderFn) defaultRenderFn(viewport, nullptr);
        return;
    }

    int w = (int)(viewport->Size.x * viewport->FramebufferScale.x);
    int h = (int)(viewport->Size.y * viewport->FramebufferScale.y);
    if (w <= 0 || h <= 0) {
        if (defaultRenderFn) defaultRenderFn(viewport, nullptr);
        return;
    }

    EnsureSized(w, h, viewport->PlatformHandle);

    GLint prevFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    // 1) Captura: todo el drawList de "ProjectorLive" (fondo + overlays +
    //    texto + anuncios + captura) a nuestro FBO en vez del framebuffer
    //    real -- mismo llamado que hace el renderer default de ImGui,
    //    redirigido.
    glBindFramebuffer(GL_FRAMEBUFFER, m_CaptureFBO);
    glViewport(0, 0, m_W, m_H);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(viewport->DrawData);

    // 2) Cadena de efectos sobre el composite ya capturado. Orden: primero
    //    el grado de color (Saturación), después los efectos "de estilo"
    //    (CRT, Grano, Viñeta), y FXAA al final porque suaviza los bordes
    //    que dejo todo lo anterior (incluido el propio degradado del
    //    viñetado).
    GLuint tex = m_CaptureTex;
    if (m_Saturation.IsEnabled()) tex = m_Saturation.Process(tex);
    if (m_CRT.IsEnabled())        tex = m_CRT.Process(tex, m_W, m_H);
    if (m_Grain.IsEnabled())      tex = m_Grain.Process(tex, glfwGetTime());
    if (m_Vignette.IsEnabled())   tex = m_Vignette.Process(tex);
    if (m_FXAA.IsEnabled())       tex = m_FXAA.Process(tex, m_W, m_H);

    // 3) Blit final al framebuffer real de la ventana.
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFBO));
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    BlitToCurrentFramebuffer(tex, m_W, m_H);
}

void CompositePostChain::Destroy() {
    DestroyCaptureFBO();
    if (m_BlitProgram) { glDeleteProgram(m_BlitProgram); m_BlitProgram = 0; }
    if (m_BlitVAO)     { glDeleteVertexArrays(1, &m_BlitVAO); m_BlitVAO = 0; }
    if (m_BlitVBO)     { glDeleteBuffers(1, &m_BlitVBO); m_BlitVBO = 0; }
    m_CRT.Destroy();
    m_Grain.Destroy();
    m_FXAA.Destroy();
    m_Saturation.Destroy();
    m_Vignette.Destroy();
    m_SubEffectsInitialized = false;

    m_PreviewCRT.Destroy();
    m_PreviewGrain.Destroy();
    m_PreviewFXAA.Destroy();
    m_PreviewSaturation.Destroy();
    m_PreviewVignette.Destroy();
    m_PreviewInitialized = false;
}

GLuint CompositePostChain::ProcessBackgroundForPreview(GLuint srcTex, int w, int h) {
    if (srcTex == 0 || w <= 0 || h <= 0) return srcTex;
    if (!AnyEnabled()) return srcTex; // nada activo -- cero costo extra

    if (!m_PreviewInitialized || w != m_PreviewW || h != m_PreviewH) {
        m_PreviewCRT.Destroy();        m_PreviewCRT.Init(w, h);
        m_PreviewGrain.Destroy();      m_PreviewGrain.Init(w, h);
        m_PreviewFXAA.Destroy();       m_PreviewFXAA.Init(w, h);
        m_PreviewSaturation.Destroy(); m_PreviewSaturation.Init(w, h);
        m_PreviewVignette.Destroy();   m_PreviewVignette.Init(w, h);
        m_PreviewW = w;
        m_PreviewH = h;
        m_PreviewInitialized = true;
    }

    // Mismo estado (habilitado/intensidad) que la cadena principal, para
    // que el preview sea un reflejo fiel de lo que ve el público.
    m_PreviewCRT.SetEnabled(m_CRT.IsEnabled());
    m_PreviewCRT.SetScanlineIntensity(m_CRT.GetScanlineIntensity());
    m_PreviewGrain.SetEnabled(m_Grain.IsEnabled());
    m_PreviewGrain.SetIntensity(m_Grain.GetIntensity());
    m_PreviewFXAA.SetEnabled(m_FXAA.IsEnabled());
    m_PreviewSaturation.SetEnabled(m_Saturation.IsEnabled());
    m_PreviewSaturation.SetAmount(m_Saturation.GetAmount());
    m_PreviewVignette.SetEnabled(m_Vignette.IsEnabled());
    m_PreviewVignette.SetIntensity(m_Vignette.GetIntensity());

    // Mismo orden que RenderViewport().
    GLuint tex = srcTex;
    if (m_PreviewSaturation.IsEnabled()) tex = m_PreviewSaturation.Process(tex);
    if (m_PreviewCRT.IsEnabled())        tex = m_PreviewCRT.Process(tex, w, h);
    if (m_PreviewGrain.IsEnabled())      tex = m_PreviewGrain.Process(tex, glfwGetTime());
    if (m_PreviewVignette.IsEnabled())   tex = m_PreviewVignette.Process(tex);
    if (m_PreviewFXAA.IsEnabled())       tex = m_PreviewFXAA.Process(tex, w, h);

    return tex;
}

} // namespace ProyecThor::Shaders
