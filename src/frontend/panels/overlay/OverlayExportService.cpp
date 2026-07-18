#include "OverlayExportService.h"
#include <imgui_internal.h>
#include <imgui_impl_opengl3.h>
#include <GL/glew.h>
#include <cstring>
#include <vector>
#include "stb_image_write.h"

namespace ProyecThor::UI {

OverlayExportService& OverlayExportService::Get() {
    static OverlayExportService instance;
    return instance;
}

void OverlayExportService::RequestCapture(ImGuiWindow* canvasWindow,
                                          ImVec2 canvasScreenPos, ImVec2 canvasScreenSize,
                                          const std::string& outPngPath,
                                          int exportW, int exportH,
                                          std::function<void(bool)> onDone)
{
    m_Pending.push_back({ canvasWindow, canvasScreenPos, canvasScreenSize,
                          outPngPath, exportW, exportH, std::move(onDone) });
}

bool OverlayExportService::CaptureOne(const PendingCapture& req)
{
    if (!req.canvasWindow || !req.canvasWindow->DrawList) return false;
    if (req.screenSize.x <= 0.0f || req.screenSize.y <= 0.0f) return false;
    if (req.exportW <= 0 || req.exportH <= 0) return false;

    ImDrawList* srcList = req.canvasWindow->DrawList;

    // ImDrawData "prestado": reutiliza el ImDrawList que ImGui ya construyo
    // este frame para la child del canvas (nada de vertices a mano), pero
    // pide al backend que lo renderice a una resolucion mas alta via
    // FramebufferScale — el mismo mecanismo que usa ImGui para HiDPI/Retina.
    ImDrawData dd;
    dd.Clear();
    dd.Valid = true;
    dd.CmdLists.push_back(srcList);
    dd.CmdListsCount    = dd.CmdLists.Size;
    dd.TotalVtxCount    = srcList->VtxBuffer.Size;
    dd.TotalIdxCount    = srcList->IdxBuffer.Size;
    dd.DisplayPos       = req.screenPos;
    dd.DisplaySize      = req.screenSize;
    dd.FramebufferScale = ImVec2(req.exportW / req.screenSize.x, req.exportH / req.screenSize.y);
    dd.OwnerViewport    = ImGui::GetMainViewport();
    dd.Textures         = ImGui::GetDrawData() ? ImGui::GetDrawData()->Textures : nullptr;

    GLint prevFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    GLint prevViewport[4] = { 0, 0, 0, 0 };
    glGetIntegerv(GL_VIEWPORT, prevViewport);

    GLuint fbo = 0, tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, req.exportW, req.exportH, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

    bool ok = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    if (ok) {
        glViewport(0, 0, req.exportW, req.exportH);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(&dd);

        const int stride = req.exportW * 4;
        std::vector<unsigned char> pixels((size_t)stride * req.exportH);
        glReadPixels(0, 0, req.exportW, req.exportH, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

        // glReadPixels entrega fila 0 = abajo; invertimos filas para el PNG,
        // y forzamos alpha opaco (el overlay se usa como fondo completo, no
        // compuesto con transparencia).
        std::vector<unsigned char> flipped(pixels.size());
        for (int y = 0; y < req.exportH; y++) {
            unsigned char*       dst = flipped.data() + (size_t)y * stride;
            const unsigned char* src = pixels.data() + (size_t)(req.exportH - 1 - y) * stride;
            std::memcpy(dst, src, stride);
        }
        for (size_t i = 3; i < flipped.size(); i += 4) flipped[i] = 255;

        ok = stbi_write_png(req.outPngPath.c_str(), req.exportW, req.exportH, 4,
                             flipped.data(), stride) != 0;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFBO);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);

    return ok;
}

void OverlayExportService::ProcessPending()
{
    if (m_Pending.empty()) return;

    std::vector<PendingCapture> batch;
    batch.swap(m_Pending);

    for (auto& req : batch) {
        bool ok = CaptureOne(req);
        if (req.onDone) req.onDone(ok);
    }
}

} // namespace ProyecThor::UI
