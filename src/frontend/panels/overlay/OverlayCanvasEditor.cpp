#include "OverlayCanvasEditor.h"
#include "OverlayExportService.h"
#include "styles/CanvaStyleEditor.h"
#include "backend/core/PresentationCore.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <GL/gl.h>
#include "stb_image.h"

namespace ProyecThor::UI {

OverlayCanvasEditor::OverlayCanvasEditor(std::vector<std::string>* fontList,
                                         ResolvePngPathFn resolvePngPath,
                                         ListBgImagesFn listBgImages,
                                         ImportImageFn importImage)
    : m_FontList(fontList)
    , m_ResolvePngPath(std::move(resolvePngPath))
    , m_ListBgImages(std::move(listBgImages))
    , m_ImportImage(std::move(importImage))
{}

ImTextureID OverlayCanvasEditor::GetImageTexture(const std::string& path) {
    if (path.empty()) return 0;
    auto it = m_ImageTexCache.find(path);
    if (it != m_ImageTexCache.end()) return it->second;

    int w, h, n;
    unsigned char* d = stbi_load(path.c_str(), &w, &h, &n, 4);
    ImTextureID tex = 0;
    if (d) {
        GLuint id; glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, d);
        stbi_image_free(d);
        tex = (ImTextureID)(intptr_t)id;
    }
    m_ImageTexCache[path] = tex;
    return tex;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Apertura del editor
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::OpenNew(const OverlayDoc& defaults) {
    m_IsEditingExisting = false;
    m_IsOpen            = true;
    m_Doc                = defaults;
    if (m_Doc.layers.empty())
        m_Doc.layers.push_back(OverlayLayer{});
    m_SelectedLayer = m_Doc.layers.empty() ? -1 : 0;
    m_SaveFailed    = false;
    memset(m_Name, 0, sizeof(m_Name));
}

void OverlayCanvasEditor::OpenEdit(const std::string& existingName, const OverlayDoc& existingDoc) {
    m_IsEditingExisting = true;
    m_IsOpen            = true;
    m_Doc                = existingDoc;
    m_SelectedLayer      = m_Doc.layers.empty() ? -1 : 0;
    m_SaveFailed         = false;

    size_t len = std::min(existingName.size(), sizeof(m_Name) - 1);
    memcpy(m_Name, existingName.c_str(), len);
    m_Name[len] = '\0';
}

// ─────────────────────────────────────────────────────────────────────────────
//  Render principal
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::Render(OnSaveCallback onSave) {
    if (!m_IsOpen) return;

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(1040.0f, 720.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(860.0f, 600.0f), ImVec2(1400.0f, 950.0f));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, CanvaPalette::Surface0);
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.22f, 0.23f, 0.32f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(0.0f, 0.0f));

    bool open = true;
    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoCollapse      |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar     |
        ImGuiWindowFlags_NoScrollWithMouse;

    bool visible = ImGui::Begin("EditorDeOverlay##ovCanvasWindow", &open, kFlags);

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    if (visible) {
        ImDrawList* dl     = ImGui::GetWindowDrawList();
        ImVec2      winPos = ImGui::GetWindowPos();
        ImVec2      winSize= ImGui::GetWindowSize();

        RenderHeader(dl, winPos, winSize);

        constexpr float kHeaderH  = 56.0f;
        constexpr float kFooterH  = 64.0f;
        constexpr float kPadH     = 20.0f;
        constexpr float kSidebarW = 260.0f;
        constexpr float kGap      = 16.0f;

        float contentH = winSize.y - kHeaderH - kFooterH - kPadH * 2.0f;
        float canvasW  = std::max(200.0f, winSize.x - kSidebarW - kGap - kPadH * 2.0f);

        ImGui::SetCursorPos(ImVec2(kPadH, kHeaderH + kPadH));
        ImGui::BeginGroup();
        RenderCanvas(canvasW, contentH);
        ImGui::EndGroup();

        ImGui::SetCursorPos(ImVec2(kPadH + canvasW + kGap, kHeaderH + kPadH));
        ImGui::BeginGroup();
        RenderSidebar(kSidebarW, contentH);
        ImGui::EndGroup();

        RenderFooter(winPos, winSize, onSave);
    }

    ImGui::End();

    if (!open) m_IsOpen = false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderHeader
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::RenderHeader(ImDrawList* dl, ImVec2 winPos, ImVec2 winSize) {
    constexpr float kHeaderH = 56.0f;

    dl->AddRectFilledMultiColor(
        winPos, ImVec2(winPos.x + winSize.x, winPos.y + kHeaderH),
        CanvaPalette::ToU32(ImVec4(0.20f, 0.16f, 0.36f, 1.0f)),
        CanvaPalette::ToU32(ImVec4(0.15f, 0.13f, 0.28f, 1.0f)),
        CanvaPalette::ToU32(ImVec4(0.09f, 0.09f, 0.12f, 1.0f)),
        CanvaPalette::ToU32(ImVec4(0.09f, 0.09f, 0.12f, 1.0f)));

    float dotY = winPos.y + kHeaderH * 0.5f;
    dl->AddCircleFilled(ImVec2(winPos.x + 28.0f, dotY), 7.0f, CanvaPalette::ToU32(CanvaPalette::Pink));
    dl->AddCircleFilled(ImVec2(winPos.x + 28.0f, dotY), 3.5f, IM_COL32(255, 255, 255, 210));

    std::string title = m_IsEditingExisting
        ? (std::string("Editar overlay — ") + m_Name)
        : "Nuevo overlay";

    dl->AddText(ImGui::GetFont(), 15.0f,
        ImVec2(winPos.x + 46.0f, winPos.y + (kHeaderH - 15.0f) * 0.5f),
        CanvaPalette::ToU32(CanvaPalette::Text), title.c_str());

    dl->AddLine(
        ImVec2(winPos.x, winPos.y + kHeaderH),
        ImVec2(winPos.x + winSize.x, winPos.y + kHeaderH),
        CanvaPalette::ToU32(CanvaPalette::Border), 1.0f);

    ImGui::Dummy(ImVec2(0.0f, kHeaderH));
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderCanvas — area central: fondo + capas de texto arrastrables
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::RenderCanvas(float availW, float availH) {
    float aspect = (float)m_Doc.canvasW / (float)std::max(1, m_Doc.canvasH);
    float cw = availW;
    float ch = cw / aspect;
    if (ch > availH) { ch = availH; cw = ch * aspect; }
    cw = std::max(cw, 100.0f);
    ch = std::max(ch, 60.0f);

    float offsetX = std::max(0.0f, (availW - cw) * 0.5f);
    if (offsetX > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("##ovCanvasArea", ImVec2(cw, ch), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // Capturado para el export (ver RenderFooter / OverlayExportService).
    m_CanvasWindowThisFrame = ImGui::GetCurrentWindow();
    m_CanvasScreenPos       = ImGui::GetWindowPos();
    m_CanvasScreenSize      = ImGui::GetWindowSize();

    // "dl" es el draw list de ESTA child — es exactamente lo que
    // OverlayExportService captura y guarda como PNG, asi que solo debe
    // contener el DISEÑO real (fondo + texto). Cualquier chrome de edicion
    // (marco de seleccion, cursor de arrastre) va en el foreground draw
    // list, que nunca se incluye en la exportacion.
    ImDrawList* dl   = ImGui::GetWindowDrawList();
    ImDrawList* fgDl = ImGui::GetForegroundDrawList();
    ImVec2 p0 = m_CanvasScreenPos;
    ImVec2 p1 = ImVec2(p0.x + m_CanvasScreenSize.x, p0.y + m_CanvasScreenSize.y);

    ImU32 bg = ImGui::ColorConvertFloat4ToU32(
        ImVec4(m_Doc.bgColor[0], m_Doc.bgColor[1], m_Doc.bgColor[2], m_Doc.bgColor[3]));
    dl->AddRectFilled(p0, p1, bg);
    fgDl->PushClipRect(p0, p1, true);

    // Click en area vacia = deseleccionar. Va ANTES que las capas para que
    // estas, dibujadas despues, le "roben" el hover en su propia zona — sin
    // AllowOverlap, ImGui le da el hover de toda la zona al primer item
    // sometido (este), y ninguna capa por encima llegaria a recibirlo nunca.
    ImGui::SetCursorScreenPos(p0);
    ImGui::SetNextItemAllowOverlap();
    if (ImGui::InvisibleButton("##ovCanvasBg", m_CanvasScreenSize)) {
        m_SelectedLayer = -1;
        m_DraggingLayer = -1;
    }

    auto&  core  = Core::PresentationCore::Get();
    float  scale = m_CanvasScreenSize.x / (float)std::max(1, m_Doc.canvasW);

    for (int i = 0; i < (int)m_Doc.layers.size(); i++) {
        auto& layer = m_Doc.layers[i];
        bool  isText = (layer.kind == OverlayLayerKind::Text);

        ImFont* font = nullptr;
        float   displaySize = 0.0f;
        ImVec2  blockSz;

        if (isText) {
            font = core.GetImGuiFont(layer.fontName, layer.fontSize);
            if (!font) font = ImGui::GetFont();
            displaySize = std::max(4.0f, layer.fontSize * scale);
            blockSz = font->CalcTextSizeA(displaySize, FLT_MAX, FLT_MAX, layer.text.c_str());
        } else {
            blockSz = ImVec2(std::max(4.0f, layer.sizeW * m_CanvasScreenSize.x),
                              std::max(4.0f, layer.sizeH * m_CanvasScreenSize.y));
        }

        ImVec2 lcenter = ImVec2(p0.x + layer.posX * m_CanvasScreenSize.x,
                                 p0.y + layer.posY * m_CanvasScreenSize.y);
        ImVec2 tl = ImVec2(lcenter.x - blockSz.x * 0.5f, lcenter.y - blockSz.y * 0.5f);
        ImVec2 br = ImVec2(tl.x + blockSz.x, tl.y + blockSz.y);

        if (isText) {
            const char* txt = layer.text.c_str();

            if (layer.bgEnabled) {
                ImU32 bgc = ImGui::ColorConvertFloat4ToU32(
                    ImVec4(layer.bgColor[0], layer.bgColor[1], layer.bgColor[2], layer.bgColor[3]));
                float padX = layer.bgPaddingX * scale, padY = layer.bgPaddingY * scale;
                dl->AddRectFilled(ImVec2(tl.x - padX, tl.y - padY), ImVec2(br.x + padX, br.y + padY),
                                  bgc, layer.bgRounding * scale);
            }
            if (layer.shadowEnabled) {
                ImU32 shc = ImGui::ColorConvertFloat4ToU32(ImVec4(
                    layer.shadowColor[0], layer.shadowColor[1], layer.shadowColor[2], layer.shadowColor[3]));
                ImVec2 so = ImVec2(layer.shadowOffsetX * scale, layer.shadowOffsetY * scale);
                dl->AddText(font, displaySize, ImVec2(tl.x + so.x, tl.y + so.y), shc, txt);
            }
            if (layer.outlineEnabled) {
                ImU32 oc = ImGui::ColorConvertFloat4ToU32(ImVec4(
                    layer.outlineColor[0], layer.outlineColor[1], layer.outlineColor[2], layer.outlineColor[3]));
                float ow = std::max(0.5f, layer.outlineWidth * scale);
                static const ImVec2 kDirs[8] = {
                    {-1,-1},{0,-1},{1,-1}, {-1,0},{1,0}, {-1,1},{0,1},{1,1}
                };
                for (const auto& d : kDirs)
                    dl->AddText(font, displaySize, ImVec2(tl.x + d.x * ow, tl.y + d.y * ow), oc, txt);
            }

            ImU32 col = ImGui::ColorConvertFloat4ToU32(
                ImVec4(layer.color[0], layer.color[1], layer.color[2], layer.color[3]));
            dl->AddText(font, displaySize, tl, col, txt);
        } else {
            ImTextureID tex = GetImageTexture(layer.imagePath);
            ImVec2 center = ImVec2((tl.x + br.x) * 0.5f, (tl.y + br.y) * 0.5f);
            float rotRad  = layer.rotation * (float)M_PI / 180.0f;
            float cs = cosf(rotRad), sn = sinf(rotRad);
            float hw = blockSz.x * 0.5f, hh = blockSz.y * 0.5f;
            auto Rot = [&](float lx, float ly) {
                return ImVec2(center.x + lx * cs - ly * sn, center.y + lx * sn + ly * cs);
            };
            ImVec2 qTL = Rot(-hw, -hh), qTR = Rot(hw, -hh), qBR = Rot(hw, hh), qBL = Rot(-hw, hh);
            if (tex) {
                dl->AddImageQuad(tex, qTL, qTR, qBR, qBL);
            } else {
                dl->AddQuadFilled(qTL, qTR, qBR, qBL, IM_COL32(40, 40, 46, 255));
                dl->AddQuad(qTL, qTR, qBR, qBL, IM_COL32(150, 70, 70, 255));
            }
        }

        ImGui::PushID(i);
        ImGui::SetNextItemAllowOverlap();
        ImGui::SetCursorScreenPos(ImVec2(tl.x - 4.0f, tl.y - 4.0f));
        ImGui::InvisibleButton("##ovLayerHit", ImVec2(blockSz.x + 8.0f, blockSz.y + 8.0f));

        bool isSel  = (m_SelectedLayer == i);
        bool isHov  = ImGui::IsItemHovered();

        // Chrome de edicion (marco de seleccion / hover) — solo en el
        // foreground draw list, nunca en "dl" (lo que se exporta a PNG).
        if (isSel) {
            fgDl->AddRect(ImVec2(tl.x - 4.0f, tl.y - 4.0f),
                          ImVec2(tl.x + blockSz.x + 4.0f, tl.y + blockSz.y + 4.0f),
                          CanvaPalette::ToU32(CanvaPalette::Accent), 3.0f, 0, 1.5f);
        } else if (isHov) {
            fgDl->AddRect(ImVec2(tl.x - 4.0f, tl.y - 4.0f),
                          ImVec2(tl.x + blockSz.x + 4.0f, tl.y + blockSz.y + 4.0f),
                          IM_COL32(255, 255, 255, 90), 3.0f, 0, 1.0f);
        }
        if (isHov) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);

        if (ImGui::IsItemActivated()) {
            m_DragStartMouse = ImGui::GetIO().MousePos;
            m_DragStartPosX  = layer.posX;
            m_DragStartPosY  = layer.posY;
            m_DraggingLayer  = i;
            m_SelectedLayer  = i;
        }
        if (m_DraggingLayer == i && ImGui::IsItemActive() &&
            ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 mouse = ImGui::GetIO().MousePos;
            float dx = (mouse.x - m_DragStartMouse.x) / m_CanvasScreenSize.x;
            float dy = (mouse.y - m_DragStartMouse.y) / m_CanvasScreenSize.y;
            layer.posX = std::clamp(m_DragStartPosX + dx, 0.0f, 1.0f);
            layer.posY = std::clamp(m_DragStartPosY + dy, 0.0f, 1.0f);
        }
        if (ImGui::IsItemDeactivated()) m_DraggingLayer = -1;

        // Handles de redimension y rotacion — solo capas de imagen, y solo si
        // esta seleccionada (para no saturar el canvas de agarres). Los
        // handles en si se mantienen sin rotar (ejes del bounding box) para
        // no complicar el hit-testing; solo el contenido visual rota.
        if (!isText && isSel) {
            RenderResizeHandle(i, layer, 0, ImVec2(tl.x, tl.y), fgDl);
            RenderResizeHandle(i, layer, 1, ImVec2(br.x, tl.y), fgDl);
            RenderResizeHandle(i, layer, 2, ImVec2(tl.x, br.y), fgDl);
            RenderResizeHandle(i, layer, 3, ImVec2(br.x, br.y), fgDl);
            RenderRotateHandle(i, layer, ImVec2((tl.x + br.x) * 0.5f, (tl.y + br.y) * 0.5f),
                              blockSz.y * 0.5f, fgDl);
        }

        ImGui::PopID();
    }

    fgDl->PopClipRect();
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
    ImGui::Text("%d x %d — arrastra una capa para posicionarla", m_Doc.canvasW, m_Doc.canvasH);
    ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderResizeHandle — agarre en una esquina de una capa de imagen. Al
//  arrastrar, la esquina OPUESTA queda fija y size/posicion se recalculan en
//  espacio normalizado (0..1) del canvas, igual que posX/posY.
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::RenderResizeHandle(int layerIdx, OverlayLayer& layer, int corner,
                                             ImVec2 handlePos, ImDrawList* fgDl) {
    constexpr float kHandleR = 5.0f;
    constexpr float kHitR    = 9.0f;

    ImGui::PushID(corner + 100);
    ImGui::SetNextItemAllowOverlap();
    ImGui::SetCursorScreenPos(ImVec2(handlePos.x - kHitR, handlePos.y - kHitR));
    ImGui::InvisibleButton("##ovResizeHandle", ImVec2(kHitR * 2.0f, kHitR * 2.0f));

    bool isHov = ImGui::IsItemHovered();
    if (isHov) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);

    fgDl->AddCircleFilled(handlePos, kHandleR, CanvaPalette::ToU32(CanvaPalette::Accent));
    fgDl->AddCircle(handlePos, kHandleR, IM_COL32(20, 20, 24, 255), 0, 1.5f);

    if (ImGui::IsItemActivated()) {
        m_ResizeStartMouse  = ImGui::GetIO().MousePos;
        m_ResizeStartPosX   = layer.posX;
        m_ResizeStartPosY   = layer.posY;
        m_ResizeStartSizeW  = layer.sizeW;
        m_ResizeStartSizeH  = layer.sizeH;
        m_ResizingLayer     = layerIdx;
        m_ResizeCorner      = corner;
        m_SelectedLayer     = layerIdx;
    }
    if (m_ResizingLayer == layerIdx && m_ResizeCorner == corner && ImGui::IsItemActive() &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        float dx = (mouse.x - m_ResizeStartMouse.x) / m_CanvasScreenSize.x;
        float dy = (mouse.y - m_ResizeStartMouse.y) / m_CanvasScreenSize.y;

        float hw = m_ResizeStartSizeW * 0.5f, hh = m_ResizeStartSizeH * 0.5f;
        float startTLx = m_ResizeStartPosX - hw, startTLy = m_ResizeStartPosY - hh;
        float startBRx = m_ResizeStartPosX + hw, startBRy = m_ResizeStartPosY + hh;

        // Esquina que se mueve con el mouse vs. esquina opuesta, que queda fija.
        float movX = (corner == 1 || corner == 3) ? startBRx + dx : startTLx + dx;
        float movY = (corner == 2 || corner == 3) ? startBRy + dy : startTLy + dy;
        float fixX = (corner == 1 || corner == 3) ? startTLx      : startBRx;
        float fixY = (corner == 2 || corner == 3) ? startTLy      : startBRy;

        constexpr float kMinSize = 0.02f;
        float newW = std::max(kMinSize, std::abs(movX - fixX));
        float newH = std::max(kMinSize, std::abs(movY - fixY));
        layer.sizeW = newW;
        layer.sizeH = newH;
        layer.posX  = std::clamp((movX + fixX) * 0.5f, 0.0f, 1.0f);
        layer.posY  = std::clamp((movY + fixY) * 0.5f, 0.0f, 1.0f);
    }
    if (ImGui::IsItemDeactivated() && m_ResizingLayer == layerIdx && m_ResizeCorner == corner)
        m_ResizingLayer = -1;

    ImGui::PopID();
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderRotateHandle — agarre flotante arriba de la capa (capas de imagen);
//  al arrastrar, gira la imagen alrededor de su propio centro. El angulo se
//  mide como atan2(dx, -dy) para que 0° = arriba, coherente con la rotacion
//  aplicada al dibujar la imagen (ver RenderCanvas).
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::RenderRotateHandle(int layerIdx, OverlayLayer& layer, ImVec2 center,
                                             float halfH, ImDrawList* fgDl) {
    constexpr float kDist = 26.0f;
    constexpr float kR    = 6.0f;
    constexpr float kHitR = 10.0f;

    float rotRad = layer.rotation * (float)M_PI / 180.0f;
    float cs = cosf(rotRad), sn = sinf(rotRad);
    auto Rot = [&](ImVec2 p) {
        return ImVec2(center.x + p.x * cs - p.y * sn, center.y + p.x * sn + p.y * cs);
    };
    ImVec2 topPt    = Rot(ImVec2(0.0f, -halfH));
    ImVec2 handlePt = Rot(ImVec2(0.0f, -(halfH + kDist)));

    fgDl->AddLine(topPt, handlePt, IM_COL32(255, 255, 255, 140), 1.5f);

    ImGui::PushID(200);
    ImGui::SetNextItemAllowOverlap();
    ImGui::SetCursorScreenPos(ImVec2(handlePt.x - kHitR, handlePt.y - kHitR));
    ImGui::InvisibleButton("##ovRotateHandle", ImVec2(kHitR * 2.0f, kHitR * 2.0f));

    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    fgDl->AddCircleFilled(handlePt, kR, CanvaPalette::ToU32(CanvaPalette::Accent));
    fgDl->AddCircle(handlePt, kR, IM_COL32(20, 20, 24, 255), 0, 1.5f);

    if (ImGui::IsItemActivated()) {
        ImVec2 m = ImGui::GetIO().MousePos;
        m_RotateStartAngle    = atan2f(m.x - center.x, -(m.y - center.y));
        m_RotateStartRotation = layer.rotation;
        m_RotatingLayer       = layerIdx;
    }
    if (m_RotatingLayer == layerIdx && ImGui::IsItemActive() &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImVec2 m = ImGui::GetIO().MousePos;
        float angNow   = atan2f(m.x - center.x, -(m.y - center.y));
        float deltaDeg = (angNow - m_RotateStartAngle) * 180.0f / (float)M_PI;
        layer.rotation = m_RotateStartRotation + deltaDeg;
    }
    if (ImGui::IsItemDeactivated() && m_RotatingLayer == layerIdx)
        m_RotatingLayer = -1;

    ImGui::PopID();
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderSidebar — lista de capas + propiedades de la capa seleccionada
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::RenderSidebar(float w, float h) {
    (void)h;

    CanvaStyleEditor::SectionLabel("CAPAS");
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    const float halfW = (w - 8.0f) * 0.5f;
    if (CanvaStyleEditor::PrimaryButton("+ Texto", ImVec2(halfW, 32.0f))) {
        OverlayLayer nl;
        nl.kind = OverlayLayerKind::Text;
        m_Doc.layers.push_back(nl);
        m_SelectedLayer = (int)m_Doc.layers.size() - 1;
    }
    ImGui::SameLine(0.0f, 8.0f);
    if (CanvaStyleEditor::PrimaryButton("+ Imagen", ImVec2(halfW, 32.0f)))
        ImGui::OpenPopup("##ovAddImagePop");

    if (ImGui::BeginPopup("##ovAddImagePop")) {
        ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
        ImGui::TextUnformatted("Anadir imagen");
        ImGui::PopStyleColor();
        ImGui::Separator();

        if (ImGui::BeginMenu("Desde Fondos")) {
            std::vector<std::string> imgs = m_ListBgImages ? m_ListBgImages() : std::vector<std::string>{};
            if (imgs.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
                ImGui::TextUnformatted("No hay imagenes en Fondos.");
                ImGui::PopStyleColor();
            }
            for (const auto& path : imgs) {
                std::string fname = path;
                if (auto pos = fname.find_last_of("/\\"); pos != std::string::npos)
                    fname = fname.substr(pos + 1);
                if (ImGui::Selectable(fname.c_str())) {
                    OverlayLayer nl;
                    nl.kind      = OverlayLayerKind::Image;
                    nl.imagePath = path;
                    m_Doc.layers.push_back(nl);
                    m_SelectedLayer = (int)m_Doc.layers.size() - 1;
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::Selectable("Importar archivo...")) {
            std::string path = m_ImportImage ? m_ImportImage() : std::string();
            if (!path.empty()) {
                OverlayLayer nl;
                nl.kind      = OverlayLayerKind::Image;
                nl.imagePath = path;
                m_Doc.layers.push_back(nl);
                m_SelectedLayer = (int)m_Doc.layers.size() - 1;
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    ImGui::PushStyleColor(ImGuiCol_ChildBg, CanvaPalette::Surface1);
    ImGui::BeginChild("##ovLayerList", ImVec2(w, 150.0f), true, ImGuiWindowFlags_NoScrollWithMouse);
    for (int i = 0; i < (int)m_Doc.layers.size(); i++) {
        ImGui::PushID(i);
        bool isSel = (m_SelectedLayer == i);
        const auto& l = m_Doc.layers[i];

        std::string label;
        if (l.kind == OverlayLayerKind::Text) {
            label = l.text.empty() ? "(vacio)" : l.text;
            for (auto& c : label) if (c == '\n') c = ' ';
        } else {
            label = l.imagePath;
            if (auto pos = label.find_last_of("/\\"); pos != std::string::npos)
                label = label.substr(pos + 1);
            if (label.empty()) label = "(imagen)";
        }
        if (label.size() > 20) label = label.substr(0, 17) + "...";
        label = (l.kind == OverlayLayerKind::Text ? std::string("[T] ") : std::string("[I] ")) + label;

        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(
            CanvaPalette::Accent.x * 0.28f, CanvaPalette::Accent.y * 0.28f,
            CanvaPalette::Accent.z * 0.55f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, CanvaPalette::Surface2);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  CanvaPalette::Surface2);
        bool clicked = ImGui::Selectable(label.c_str(), isSel, 0, ImVec2(w - 40.0f, 0.0f));
        ImGui::PopStyleColor(3);
        if (clicked) m_SelectedLayer = i;

        ImGui::SameLine(w - 30.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::Red);
        bool removeClicked = ImGui::SmallButton("x");
        ImGui::PopStyleColor();
        ImGui::PopID();

        if (removeClicked) {
            m_Doc.layers.erase(m_Doc.layers.begin() + i);
            if (m_SelectedLayer == i)      m_SelectedLayer = -1;
            else if (m_SelectedLayer > i)  m_SelectedLayer--;
            break; // los indices cambiaron: no seguir iterando este frame
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    CanvaStyleEditor::SectionLabel("PROPIEDADES");
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    if (m_SelectedLayer < 0 || m_SelectedLayer >= (int)m_Doc.layers.size()) {
        ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
        ImGui::TextWrapped("Selecciona o crea una capa para editar sus propiedades.");
        ImGui::PopStyleColor();
        return;
    }

    auto& layer = m_Doc.layers[m_SelectedLayer];

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        CanvaPalette::Surface1);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, CanvaPalette::Surface2);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

    if (layer.kind == OverlayLayerKind::Text) {
        char textBuf[512];
        size_t len = std::min(layer.text.size(), sizeof(textBuf) - 1);
        memcpy(textBuf, layer.text.c_str(), len);
        textBuf[len] = '\0';
        ImGui::SetNextItemWidth(w);
        if (ImGui::InputTextMultiline("##ovLayerText", textBuf, sizeof(textBuf), ImVec2(w, 54.0f)))
            layer.text = textBuf;

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::SetNextItemWidth(w);
        if (ImGui::BeginCombo("##ovLayerFont", layer.fontName.c_str())) {
            if (m_FontList) {
                for (const auto& f : *m_FontList) {
                    bool sel = (layer.fontName == f);
                    if (ImGui::Selectable(f.c_str(), sel)) layer.fontName = f;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::SetNextItemWidth(w);
        ImGui::DragFloat("##ovLayerSize", &layer.fontSize, 1.0f, 10.0f, 400.0f, "%.0f px");

        // Swatch de color "a lo Estilos": sin sliders RGBA inline, solo el
        // cuadradito que abre el picker completo en un popup al clickear.
        constexpr ImGuiColorEditFlags kSwatchFlags = ImGuiColorEditFlags_AlphaBar |
            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueWheel |
            ImGuiColorEditFlags_AlphaPreviewHalf;

        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Color");
        ImGui::SameLine(w - 26.0f);
        ImGui::ColorEdit4("##ovLayerColor", layer.color, kSwatchFlags);

        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 4.0f));

        ImGui::Checkbox("Sombra", &layer.shadowEnabled);
        if (layer.shadowEnabled) {
            ImGui::SameLine(w - 26.0f);
            ImGui::ColorEdit4("##ovShadowColor", layer.shadowColor, kSwatchFlags);
            ImGui::SetNextItemWidth(w);
            float shOff[2] = { layer.shadowOffsetX, layer.shadowOffsetY };
            if (ImGui::DragFloat2("##ovShadowOffset", shOff, 0.2f, -20.0f, 20.0f, "%.1f px")) {
                layer.shadowOffsetX = shOff[0];
                layer.shadowOffsetY = shOff[1];
            }
        }

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::Checkbox("Contorno", &layer.outlineEnabled);
        if (layer.outlineEnabled) {
            ImGui::SameLine(w - 26.0f);
            ImGui::ColorEdit4("##ovOutlineColor", layer.outlineColor, kSwatchFlags);
            ImGui::SetNextItemWidth(w);
            ImGui::DragFloat("##ovOutlineWidth", &layer.outlineWidth, 0.2f, 0.5f, 20.0f, "%.1f px");
        }

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::Checkbox("Fondo", &layer.bgEnabled);
        if (layer.bgEnabled) {
            ImGui::SameLine(w - 26.0f);
            ImGui::ColorEdit4("##ovBgColor", layer.bgColor, kSwatchFlags);
            ImGui::SetNextItemWidth(w);
            float pad[2] = { layer.bgPaddingX, layer.bgPaddingY };
            if (ImGui::DragFloat2("##ovBgPadding", pad, 0.2f, 0.0f, 60.0f, "%.0f px")) {
                layer.bgPaddingX = pad[0];
                layer.bgPaddingY = pad[1];
            }
            ImGui::SetNextItemWidth(w);
            ImGui::DragFloat("##ovBgRounding", &layer.bgRounding, 0.2f, 0.0f, 40.0f, "%.0f redondeo");
        }
    } else {
        std::string fname = layer.imagePath;
        if (auto pos = fname.find_last_of("/\\"); pos != std::string::npos)
            fname = fname.substr(pos + 1);
        ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
        ImGui::TextWrapped("%s", fname.c_str());
        ImGui::PopStyleColor();

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::TextUnformatted("Tamano");
        ImGui::SetNextItemWidth(w);
        float sizePct[2] = { layer.sizeW * 100.0f, layer.sizeH * 100.0f };
        if (ImGui::DragFloat2("##ovLayerImgSize", sizePct, 0.5f, 2.0f, 100.0f, "%.0f%%")) {
            layer.sizeW = sizePct[0] / 100.0f;
            layer.sizeH = sizePct[1] / 100.0f;
        }

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::TextUnformatted("Rotacion");
        ImGui::SetNextItemWidth(w);
        ImGui::DragFloat("##ovLayerRotation", &layer.rotation, 0.5f, -180.0f, 180.0f, "%.0f grados");

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
        ImGui::TextWrapped("Tambien podes arrastrar las esquinas (tamano) o el "
                           "handle de arriba (rotacion) en el canvas.");
        ImGui::PopStyleColor();
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderFooter — nombre + Cancelar/Guardar (dispara la captura a PNG)
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::RenderFooter(ImVec2 winPos, ImVec2 winSize, OnSaveCallback& onSave) {
    constexpr float kFooterH = 64.0f;
    float footerY = winSize.y - kFooterH;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddLine(
        ImVec2(winPos.x, winPos.y + footerY),
        ImVec2(winPos.x + winSize.x, winPos.y + footerY),
        CanvaPalette::ToU32(CanvaPalette::Border), 1.0f);
    dl->AddRectFilled(
        ImVec2(winPos.x, winPos.y + footerY),
        ImVec2(winPos.x + winSize.x, winPos.y + winSize.y),
        CanvaPalette::ToU32(ImVec4(0.08f, 0.08f, 0.10f, 1.0f)),
        0.0f, ImDrawFlags_RoundCornersBottom);

    ImGui::SetCursorPos(ImVec2(20.0f, footerY + (kFooterH - 36.0f) * 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Nombre del overlay:");
    ImGui::PopStyleColor();
    ImGui::SameLine(0.0f, 8.0f);

    bool nameEmpty = (strlen(m_Name) == 0);
    if (nameEmpty) {
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.90f, 0.30f, 0.30f, 0.70f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    }
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        CanvaPalette::Surface1);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, CanvaPalette::Surface2);
    ImGui::PushStyleColor(ImGuiCol_Text,           CanvaPalette::Text);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputText("##ovNameInput", m_Name, sizeof(m_Name));
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    if (nameEmpty) { ImGui::PopStyleVar(); ImGui::PopStyleColor(); }

    if (m_SaveFailed) {
        ImGui::SameLine(0.0f, 8.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::Red);
        ImGui::Text("No se pudo guardar. Intenta de nuevo.");
        ImGui::PopStyleColor();
    } else if (nameEmpty) {
        ImGui::SameLine(0.0f, 8.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.40f, 0.40f, 1.0f));
        ImGui::Text("El nombre es obligatorio");
        ImGui::PopStyleColor();
    }

    float btnGroupW = 110.0f + 8.0f + 170.0f;
    ImGui::SameLine(winSize.x - btnGroupW - 20.0f);

    if (CanvaStyleEditor::GhostButton("  Cancelar  ", ImVec2(110.0f, 36.0f)))
        m_IsOpen = false;

    ImGui::SameLine(0.0f, 8.0f);

    bool canSave = !nameEmpty;
    if (!canSave)
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);

    bool clickedSave = CanvaStyleEditor::PrimaryButton("  Guardar overlay  ", ImVec2(170.0f, 36.0f));

    if (!canSave)
        ImGui::PopStyleVar();

    if (clickedSave && canSave && m_CanvasWindowThisFrame) {
        m_SaveFailed = false;

        std::string name    = m_Name;
        std::string pngPath = m_ResolvePngPath ? m_ResolvePngPath(name) : std::string();
        OverlayDoc  docCopy = m_Doc;
        int         expW    = m_Doc.canvasW;
        int         expH    = m_Doc.canvasH;

        // Captura por VALOR (no por referencia): onSave/name/docCopy deben
        // sobrevivir hasta ProcessPending() mas adelante en este mismo frame,
        // momento en el que este Render() ya retorno.
        OverlayExportService::Get().RequestCapture(
            m_CanvasWindowThisFrame, m_CanvasScreenPos, m_CanvasScreenSize,
            pngPath, expW, expH,
            [this, name, docCopy, onSave](bool ok) {
                if (ok) {
                    if (onSave) onSave(name, docCopy);
                    m_IsOpen = false;
                } else {
                    m_SaveFailed = true;
                }
            });
    }
}

} // namespace ProyecThor::UI
