#include "PostProcessorGrain.h"
#include <algorithm>

namespace ProyecThor::Shaders {

static std::string BuildGrainFragSrc() {
    return R"GLSL(
#version 330 core
in vec2 v_UV;
out vec4 fragColor;

uniform sampler2D u_InputTex;
uniform float u_Time;
uniform float u_Intensity;

float Hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233)) + u_Time) * 43758.5453);
}

void main() {
    vec4 src = texture(u_InputTex, v_UV);
    float n  = Hash(gl_FragCoord.xy) - 0.5;
    fragColor = vec4(src.rgb + n * u_Intensity, src.a);
}
)GLSL";
}

bool PostProcessorGrain::Init(int w, int h) {
    return m_Pipeline.Init(w, h, BuildGrainFragSrc());
}

void PostProcessorGrain::SetIntensity(float v) {
    m_Intensity = std::clamp(v, 0.0f, 1.0f);
}

GLuint PostProcessorGrain::Process(GLuint srcTex, double timeSeconds) {
    if (!m_Enabled || !m_Pipeline.IsInitialized()) return srcTex;

    return m_Pipeline.Process(srcTex, [&](GLuint prog) {
        glUniform1f(glGetUniformLocation(prog, "u_Time"), (float)timeSeconds);
        glUniform1f(glGetUniformLocation(prog, "u_Intensity"), m_Intensity);
    });
}

} // namespace ProyecThor::Shaders
