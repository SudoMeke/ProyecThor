#include "OverlayCanvasEditor.h"
#include "OverlayExportService.h"
#include "OverlayLayerRender.h"
#include "styles/CanvaStyleEditor.h"
#include "layers/LayersTheme.h"
#include "frontend/ui/IconRail.h"
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
    m_SelectedLayer      = m_Doc.layers.empty() ? -1 : 0;
    m_SaveFailed         = false;
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
//  Render principal — SIEMPRE a pantalla completa (debajo de la toolbar de
//  modos, que UIManager deja dibujada aparte -- ver
//  UIManager::EnterFullscreenEditor). El llamador ya se encargo de ocultar
//  el resto de los paneles para este frame.
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::Render(OnSaveCallback onSave, OnCancelCallback onClose) {
    if (!m_IsOpen) return;

    ImGuiViewport* vp    = ImGui::GetMainViewport();
    float          railH = IconRailThickness(false);

    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + railH));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, vp->WorkSize.y - railH));
    ImGui::SetNextWindowViewport(vp->ID);

    // NoScrollbar/NoScrollWithMouse son clave aca: el header/footer se
    // dibujan en coordenadas de PANTALLA (winPos + offset fijo) pero el
    // resto del contenido (canvas/sidebar) usa SetCursorPos, que es relativo
    // al scroll de la ventana -- si esta ventana llegaba a scrollear (ej.
    // contenido mas alto que la pantalla), el footer/header quedaban fijos
    // en pantalla mientras los widgets reales (input de nombre, botones)
    // se desplazaban con el scroll, separandose del fondo que los acompaña.
    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoDecoration      |
        ImGuiWindowFlags_NoMove            |
        ImGuiWindowFlags_NoSavedSettings   |
        ImGuiWindowFlags_NoDocking         |
        ImGuiWindowFlags_NoScrollbar       |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, CanvaPalette::Surface0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("##ovCanvasFullscreen", nullptr, kFlags);

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImDrawList* dl      = ImGui::GetWindowDrawList();
    ImVec2      winPos  = ImGui::GetWindowPos();
    ImVec2      winSize = ImGui::GetWindowSize();

    RenderHeader(dl, winPos, winSize);

    constexpr float kHeaderH  = 40.0f;
    constexpr float kFooterH  = 52.0f;
    constexpr float kPadH     = 14.0f;
    constexpr float kSidebarW = 260.0f;
    constexpr float kGap      = 14.0f;

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

    RenderFooter(winPos, winSize, onSave, onClose);

    ImGui::End();
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderHeader
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::RenderHeader(ImDrawList* dl, ImVec2 winPos, ImVec2 winSize) {
    constexpr float kHeaderH = 40.0f;

    // Fill plano (sin degradado) — mas minimalista, y no compite visualmente
    // con el canvas/preview del overlay que se edita.
    dl->AddRectFilled(winPos, ImVec2(winPos.x + winSize.x, winPos.y + kHeaderH),
                      CanvaPalette::ToU32(CanvaPalette::Surface1));

    float dotY = winPos.y + kHeaderH * 0.5f;
    dl->AddCircleFilled(ImVec2(winPos.x + 22.0f, dotY), 5.0f, CanvaPalette::ToU32(CanvaPalette::Pink));

    std::string title = m_IsEditingExisting
        ? (std::string("Editar overlay — ") + m_Name)
        : "Nuevo overlay";

    dl->AddText(ImGui::GetFont(), 14.0f,
        ImVec2(winPos.x + 36.0f, winPos.y + (kHeaderH - 14.0f) * 0.5f),
        CanvaPalette::ToU32(CanvaPalette::Text), title.c_str());

    dl->AddLine(
        ImVec2(winPos.x, winPos.y + kHeaderH),
        ImVec2(winPos.x + winSize.x, winPos.y + kHeaderH),
        CanvaPalette::ToU32(CanvaPalette::Border), 1.0f);

    ImGui::Dummy(ImVec2(0.0f, kHeaderH));
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderFloatingToolbar — pastilla flotante arriba del canvas: Texto/Forma/
//  Imagen/Eliminar. "Eliminar" actua sobre la capa seleccionada (seleccionar
//  una capa se hace haciendo click en ella dentro del canvas o en la lista
//  de capas del sidebar).
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::RenderFloatingToolbar(ImVec2 canvasPos, ImVec2 canvasSize) {
    constexpr float kBtnSz = 34.0f;
    constexpr float kGap   = 6.0f;
    constexpr float kPad   = 8.0f;
    const int       kCount = 5;

    float barW = kPad * 2.0f + kBtnSz * kCount + kGap * (kCount - 1);
    ImVec2 barPos = ImVec2(canvasPos.x + (canvasSize.x - barW) * 0.5f, canvasPos.y - kBtnSz - 18.0f);

    ImGui::SetCursorScreenPos(barPos);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(CanvaPalette::Surface1.x, CanvaPalette::Surface1.y, CanvaPalette::Surface1.z, 0.96f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kPad, kPad));
    ImGui::BeginChild("##ovFloatingToolbar", ImVec2(barW, kBtnSz + kPad * 2.0f), true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    bool hasSelection = (m_SelectedLayer >= 0 && m_SelectedLayer < (int)m_Doc.layers.size());
    bool hasClock     = (FindClockLayer(m_Doc) != nullptr);

    if (LPCornerIconBtn("##ovAddText", +[](ImDrawList* dl, ImVec2 c, float r, ImU32 col){
            dl->AddLine({c.x - r*0.6f, c.y - r*0.55f}, {c.x + r*0.6f, c.y - r*0.55f}, col, 2.0f);
            dl->AddLine({c.x, c.y - r*0.55f}, {c.x, c.y + r*0.6f}, col, 2.0f);
        }, "Anadir texto", {kBtnSz, kBtnSz})) {
        OverlayLayer nl;
        nl.kind = OverlayLayerKind::Text;
        m_Doc.layers.push_back(nl);
        m_SelectedLayer = (int)m_Doc.layers.size() - 1;
    }
    ImGui::SameLine(0.0f, kGap);

    if (LPCornerIconBtn("##ovAddShape", +[](ImDrawList* dl, ImVec2 c, float r, ImU32 col){
            dl->AddRect({c.x - r*0.65f, c.y - r*0.5f}, {c.x + r*0.05f, c.y + r*0.15f}, col, 2.0f, 0, 1.6f);
            dl->AddCircle({c.x + r*0.28f, c.y + r*0.1f}, r*0.32f, col, 0, 1.6f);
        }, "Anadir forma", {kBtnSz, kBtnSz}))
        ImGui::OpenPopup("##ovAddShapePop");
    ImGui::SameLine(0.0f, kGap);

    if (LPCornerIconBtn("##ovAddImage", +[](ImDrawList* dl, ImVec2 c, float r, ImU32 col){
            dl->AddRect({c.x - r*0.7f, c.y - r*0.55f}, {c.x + r*0.7f, c.y + r*0.55f}, col, 2.0f, 0, 1.6f);
            dl->AddCircleFilled({c.x - r*0.32f, c.y - r*0.2f}, r*0.16f, col);
            dl->AddTriangleFilled({c.x - r*0.55f, c.y + r*0.5f}, {c.x - r*0.05f, c.y}, {c.x + r*0.55f, c.y + r*0.5f}, col);
        }, "Anadir imagen", {kBtnSz, kBtnSz}))
        ImGui::OpenPopup("##ovAddImagePop");
    ImGui::SameLine(0.0f, kGap);

    if (hasClock) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.4f);
    bool addClockClicked = LPCornerIconBtn("##ovAddClock", +[](ImDrawList* dl, ImVec2 c, float r, ImU32 col){
            dl->AddCircle(c, r * 0.62f, col, 0, 1.6f);
            dl->AddLine(c, {c.x, c.y - r * 0.4f}, col, 1.6f);
            dl->AddLine(c, {c.x + r * 0.3f, c.y}, col, 1.6f);
        }, hasClock ? "Ya hay un cuadro de reloj" : "Anadir cuadro de reloj", {kBtnSz, kBtnSz});
    if (hasClock) ImGui::PopStyleVar();
    if (addClockClicked && !hasClock) {
        OverlayLayer nl;
        nl.kind = OverlayLayerKind::Clock;
        nl.text = "00:00:00";
        m_Doc.layers.push_back(nl);
        m_SelectedLayer = (int)m_Doc.layers.size() - 1;
    }
    ImGui::SameLine(0.0f, kGap);

    if (!hasSelection) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.4f);
    bool delClicked = LPCornerIconBtn("##ovDelSel", +[](ImDrawList* dl, ImVec2 c, float r, ImU32 col){
            dl->AddRect({c.x - r*0.45f, c.y - r*0.25f}, {c.x + r*0.45f, c.y + r*0.6f}, col, 1.5f, 0, 1.6f);
            dl->AddLine({c.x - r*0.65f, c.y - r*0.4f}, {c.x + r*0.65f, c.y - r*0.4f}, col, 1.6f);
            dl->AddLine({c.x - r*0.2f, c.y - r*0.4f}, {c.x - r*0.2f, c.y - r*0.6f}, col, 1.6f);
            dl->AddLine({c.x + r*0.2f, c.y - r*0.4f}, {c.x + r*0.2f, c.y - r*0.6f}, col, 1.6f);
        }, "Eliminar seleccionado", {kBtnSz, kBtnSz});
    if (!hasSelection) ImGui::PopStyleVar();
    if (delClicked && hasSelection) {
        m_Doc.layers.erase(m_Doc.layers.begin() + m_SelectedLayer);
        m_SelectedLayer = -1;
    }

    if (ImGui::BeginPopup("##ovAddShapePop")) {
        if (ImGui::Selectable("Rectangulo")) {
            OverlayLayer nl;
            nl.kind      = OverlayLayerKind::Shape;
            nl.shapeKind = OverlayShapeKind::Rectangle;
            nl.color[0] = CanvaPalette::Accent.x; nl.color[1] = CanvaPalette::Accent.y;
            nl.color[2] = CanvaPalette::Accent.z; nl.color[3] = 0.85f;
            m_Doc.layers.push_back(nl);
            m_SelectedLayer = (int)m_Doc.layers.size() - 1;
        }
        if (ImGui::Selectable("Elipse")) {
            OverlayLayer nl;
            nl.kind      = OverlayLayerKind::Shape;
            nl.shapeKind = OverlayShapeKind::Ellipse;
            nl.color[0] = CanvaPalette::Accent.x; nl.color[1] = CanvaPalette::Accent.y;
            nl.color[2] = CanvaPalette::Accent.z; nl.color[3] = 0.85f;
            m_Doc.layers.push_back(nl);
            m_SelectedLayer = (int)m_Doc.layers.size() - 1;
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("##ovAddImagePop")) {
        AddImageLayerFromMenu();
        ImGui::EndPopup();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void OverlayCanvasEditor::AddImageLayerFromMenu() {
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
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderCanvas — area central: fondo transparente + capas arrastrables
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::RenderCanvas(float availW, float availH) {
    constexpr float kToolbarReserve = 62.0f; // espacio para la toolbar flotante arriba

    float aspect = (float)m_Doc.canvasW / (float)std::max(1, m_Doc.canvasH);
    float availCanvasH = availH - kToolbarReserve;
    float cw = availW;
    float ch = cw / aspect;
    if (ch > availCanvasH) { ch = availCanvasH; cw = ch * aspect; }
    cw = std::max(cw, 100.0f);
    ch = std::max(ch, 60.0f);

    float offsetX = std::max(0.0f, (availW - cw) * 0.5f);

    ImGui::Dummy(ImVec2(0.0f, kToolbarReserve));
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
    // contener el DISEÑO real (fondo + capas). Cualquier chrome de edicion
    // (marco de seleccion, handles) va en el foreground draw list, que nunca
    // se incluye en la exportacion.
    ImDrawList* dl   = ImGui::GetWindowDrawList();
    ImDrawList* fgDl = ImGui::GetForegroundDrawList();
    ImVec2 p0 = m_CanvasScreenPos;
    ImVec2 p1 = ImVec2(p0.x + m_CanvasScreenSize.x, p0.y + m_CanvasScreenSize.y);

    // Fondo a cuadros (checkerboard) SOLO como guia visual de "sin fondo",
    // igual que Photoshop/Canva. Se dibuja en "dl" -- la EXPORTACION ya NO
    // lee este draw list en absoluto (ver DrawLayersForExport/RenderFooter):
    // arma su propio ImDrawList aparte con solo las capas reales, asi que el
    // cuadriculado puede vivir aca (visible, con el z-order correcto: por
    // debajo de las capas que se dibujan despues en esta misma lista) sin
    // ningun riesgo de terminar horneado en el PNG.
    {
        const float cell = 14.0f;
        ImU32 c1 = IM_COL32(38, 38, 44, 255), c2 = IM_COL32(30, 30, 35, 255);
        int cols = (int)std::ceil(m_CanvasScreenSize.x / cell);
        int rows = (int)std::ceil(m_CanvasScreenSize.y / cell);
        dl->PushClipRect(p0, p1, true);
        for (int ry = 0; ry < rows; ry++)
            for (int rx = 0; rx < cols; rx++) {
                ImVec2 a = { p0.x + rx * cell, p0.y + ry * cell };
                ImVec2 b = { std::min(p1.x, a.x + cell), std::min(p1.y, a.y + cell) };
                dl->AddRectFilled(a, b, ((rx + ry) % 2 == 0) ? c1 : c2);
            }
        dl->PopClipRect();
    }

    if (m_Doc.bgColor[3] > 0.001f) {
        ImU32 bg = ImGui::ColorConvertFloat4ToU32(
            ImVec4(m_Doc.bgColor[0], m_Doc.bgColor[1], m_Doc.bgColor[2], m_Doc.bgColor[3]));
        dl->AddRectFilled(p0, p1, bg);
    }
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
        bool  isText  = (layer.kind == OverlayLayerKind::Text);
        bool  isImage = (layer.kind == OverlayLayerKind::Image);
        bool  isShape = (layer.kind == OverlayLayerKind::Shape);
        bool  isClock = (layer.kind == OverlayLayerKind::Clock);

        ImFont* font = nullptr;
        float   displaySize = 0.0f;
        ImVec2  blockSz;

        if (isText || isClock) {
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
            DrawOverlayLayerStyledText(dl, font, displaySize, tl, blockSz, layer, layer.text.c_str(), scale);
        } else if (isClock) {
            // Cuadro-flag: se previsualiza en el editor (placeholder + marco
            // punteado) pero SOLO en el foreground draw list -- nunca en
            // "dl", asi que nunca queda horneado en el PNG exportado. En
            // vivo, el reloj real se dibuja en esta misma posicion/estilo
            // sobre el overlay ya proyectado (ver LiveContentRenderer.cpp/
            // UIManager.cpp), no sobre el PNG.
            DrawOverlayLayerStyledText(fgDl, font, displaySize, tl, blockSz, layer, layer.text.c_str(), scale);

            constexpr float kDash = 5.0f;
            ImU32 dashCol = CanvaPalette::ToU32(CanvaPalette::Accent);
            for (float x = tl.x; x < br.x; x += kDash * 2.0f) {
                fgDl->AddLine({x, tl.y}, {std::min(x + kDash, br.x), tl.y}, dashCol, 1.5f);
                fgDl->AddLine({x, br.y}, {std::min(x + kDash, br.x), br.y}, dashCol, 1.5f);
            }
            for (float y = tl.y; y < br.y; y += kDash * 2.0f) {
                fgDl->AddLine({tl.x, y}, {tl.x, std::min(y + kDash, br.y)}, dashCol, 1.5f);
                fgDl->AddLine({br.x, y}, {br.x, std::min(y + kDash, br.y)}, dashCol, 1.5f);
            }
            const char* tag = "RELOJ";
            ImVec2 tagSz = ImGui::CalcTextSize(tag);
            fgDl->AddRectFilled({tl.x, tl.y - tagSz.y - 4.0f}, {tl.x + tagSz.x + 8.0f, tl.y - 2.0f},
                                dashCol, 3.0f);
            fgDl->AddText({tl.x + 4.0f, tl.y - tagSz.y - 2.0f}, IM_COL32(20, 20, 24, 255), tag);
        } else if (isImage) {
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
        } else { // isShape
            ImVec2 center = ImVec2((tl.x + br.x) * 0.5f, (tl.y + br.y) * 0.5f);
            float hw = blockSz.x * 0.5f, hh = blockSz.y * 0.5f;
            float rotRad = layer.rotation * (float)M_PI / 180.0f;
            ImU32 fillCol = ImGui::ColorConvertFloat4ToU32(
                ImVec4(layer.color[0], layer.color[1], layer.color[2], layer.color[3]));
            ImU32 strokeCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
                layer.outlineColor[0], layer.outlineColor[1], layer.outlineColor[2], layer.outlineColor[3]));
            float ow = std::max(0.5f, layer.outlineWidth * scale);

            if (layer.shapeKind == OverlayShapeKind::Ellipse) {
                if (layer.shapeFilled) dl->AddEllipseFilled(center, ImVec2(hw, hh), fillCol, rotRad, 0);
                if (layer.outlineEnabled) dl->AddEllipse(center, ImVec2(hw, hh), strokeCol, rotRad, 0, ow);
            } else if (std::fabs(layer.rotation) < 0.01f) {
                float rounding = layer.shapeRounding * scale;
                if (layer.shapeFilled) dl->AddRectFilled(tl, br, fillCol, rounding);
                if (layer.outlineEnabled) dl->AddRect(tl, br, strokeCol, rounding, 0, ow);
            } else {
                float cs = cosf(rotRad), sn = sinf(rotRad);
                auto Rot = [&](float lx, float ly) {
                    return ImVec2(center.x + lx * cs - ly * sn, center.y + lx * sn + ly * cs);
                };
                ImVec2 qTL = Rot(-hw, -hh), qTR = Rot(hw, -hh), qBR = Rot(hw, hh), qBL = Rot(-hw, hh);
                if (layer.shapeFilled) dl->AddQuadFilled(qTL, qTR, qBR, qBL, fillCol);
                if (layer.outlineEnabled) dl->AddQuad(qTL, qTR, qBR, qBL, strokeCol, ow);
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

        // Handles de redimension y rotacion — capas de imagen/forma, y solo
        // si esta seleccionada (para no saturar el canvas de agarres). Los
        // handles en si se mantienen sin rotar (ejes del bounding box) para
        // no complicar el hit-testing; solo el contenido visual rota.
        if (!isText && !isClock && isSel) {
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

    // La toolbar flotante se dibuja DESPUES del canvas (usa su posicion) pero
    // visualmente queda arriba porque RenderCanvas reservo kToolbarReserve.
    RenderFloatingToolbar(ImVec2(p0.x, p0.y - kToolbarReserve + 4.0f), ImVec2(cw, kToolbarReserve));

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
    ImGui::Text("%d x %d — sin fondo (transparente). Arrastra una capa para posicionarla",
               m_Doc.canvasW, m_Doc.canvasH);
    ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────────────────────
//  DrawLayersForExport — misma geometria/estilo que el canvas en vivo (ver
//  RenderCanvas) pero SOLO el contenido real: sin cuadriculado, sin chrome
//  de edicion, sin capas Clock (esas nunca se hornean). Usado exclusivamente
//  al exportar (ver RenderFooter), en un ImDrawList propio que no comparte
//  nada con lo que se ve en pantalla ese mismo frame.
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::DrawLayersForExport(ImDrawList* dl, ImVec2 p0, ImVec2 canvasScreenSize) {
    if (m_Doc.bgColor[3] > 0.001f) {
        ImU32 bg = ImGui::ColorConvertFloat4ToU32(
            ImVec4(m_Doc.bgColor[0], m_Doc.bgColor[1], m_Doc.bgColor[2], m_Doc.bgColor[3]));
        dl->AddRectFilled(p0, ImVec2(p0.x + canvasScreenSize.x, p0.y + canvasScreenSize.y), bg);
    }

    auto&  core  = Core::PresentationCore::Get();
    float  scale = canvasScreenSize.x / (float)std::max(1, m_Doc.canvasW);

    for (const auto& layer : m_Doc.layers) {
        if (layer.kind == OverlayLayerKind::Clock) continue; // cuadro-flag, nunca se hornea

        bool isText  = (layer.kind == OverlayLayerKind::Text);
        bool isImage = (layer.kind == OverlayLayerKind::Image);

        ImFont* font = nullptr;
        float   displaySize = 0.0f;
        ImVec2  blockSz;

        if (isText) {
            font = core.GetImGuiFont(layer.fontName, layer.fontSize);
            if (!font) font = ImGui::GetFont();
            displaySize = std::max(4.0f, layer.fontSize * scale);
            blockSz = font->CalcTextSizeA(displaySize, FLT_MAX, FLT_MAX, layer.text.c_str());
        } else {
            blockSz = ImVec2(std::max(4.0f, layer.sizeW * canvasScreenSize.x),
                              std::max(4.0f, layer.sizeH * canvasScreenSize.y));
        }

        ImVec2 lcenter = ImVec2(p0.x + layer.posX * canvasScreenSize.x,
                                 p0.y + layer.posY * canvasScreenSize.y);
        ImVec2 tl = ImVec2(lcenter.x - blockSz.x * 0.5f, lcenter.y - blockSz.y * 0.5f);
        ImVec2 br = ImVec2(tl.x + blockSz.x, tl.y + blockSz.y);

        if (isText) {
            DrawOverlayLayerStyledText(dl, font, displaySize, tl, blockSz, layer, layer.text.c_str(), scale);
        } else if (isImage) {
            ImTextureID tex = GetImageTexture(layer.imagePath);
            ImVec2 center = ImVec2((tl.x + br.x) * 0.5f, (tl.y + br.y) * 0.5f);
            float rotRad  = layer.rotation * (float)M_PI / 180.0f;
            float cs = cosf(rotRad), sn = sinf(rotRad);
            float hw = blockSz.x * 0.5f, hh = blockSz.y * 0.5f;
            auto Rot = [&](float lx, float ly) {
                return ImVec2(center.x + lx * cs - ly * sn, center.y + lx * sn + ly * cs);
            };
            ImVec2 qTL = Rot(-hw, -hh), qTR = Rot(hw, -hh), qBR = Rot(hw, hh), qBL = Rot(-hw, hh);
            if (tex) dl->AddImageQuad(tex, qTL, qTR, qBR, qBL);
        } else { // Shape
            ImVec2 center = ImVec2((tl.x + br.x) * 0.5f, (tl.y + br.y) * 0.5f);
            float hw = blockSz.x * 0.5f, hh = blockSz.y * 0.5f;
            float rotRad = layer.rotation * (float)M_PI / 180.0f;
            ImU32 fillCol = ImGui::ColorConvertFloat4ToU32(
                ImVec4(layer.color[0], layer.color[1], layer.color[2], layer.color[3]));
            ImU32 strokeCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
                layer.outlineColor[0], layer.outlineColor[1], layer.outlineColor[2], layer.outlineColor[3]));
            float ow = std::max(0.5f, layer.outlineWidth * scale);

            if (layer.shapeKind == OverlayShapeKind::Ellipse) {
                if (layer.shapeFilled) dl->AddEllipseFilled(center, ImVec2(hw, hh), fillCol, rotRad, 0);
                if (layer.outlineEnabled) dl->AddEllipse(center, ImVec2(hw, hh), strokeCol, rotRad, 0, ow);
            } else if (std::fabs(layer.rotation) < 0.01f) {
                float rounding = layer.shapeRounding * scale;
                if (layer.shapeFilled) dl->AddRectFilled(tl, br, fillCol, rounding);
                if (layer.outlineEnabled) dl->AddRect(tl, br, strokeCol, rounding, 0, ow);
            } else {
                float cs = cosf(rotRad), sn = sinf(rotRad);
                auto Rot = [&](float lx, float ly) {
                    return ImVec2(center.x + lx * cs - ly * sn, center.y + lx * sn + ly * cs);
                };
                ImVec2 qTL = Rot(-hw, -hh), qTR = Rot(hw, -hh), qBR = Rot(hw, hh), qBL = Rot(-hw, hh);
                if (layer.shapeFilled) dl->AddQuadFilled(qTL, qTR, qBR, qBL, fillCol);
                if (layer.outlineEnabled) dl->AddQuad(qTL, qTR, qBR, qBL, strokeCol, ow);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderResizeHandle — agarre en una esquina de una capa de imagen/forma. Al
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
//  RenderRotateHandle — agarre flotante arriba de la capa (capas de imagen/
//  forma); al arrastrar, gira alrededor de su propio centro. El angulo se
//  mide como atan2(dx, -dy) para que 0° = arriba, coherente con la rotacion
//  aplicada al dibujar (ver RenderCanvas).
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
//  RenderSidebar — lista de capas (objetos) + propiedades de la seleccionada.
//  Anadir capas ahora vive en la toolbar flotante (ver RenderFloatingToolbar)
//  -- este panel solo lista/edita lo que ya existe.
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::RenderSidebar(float w, float h) {
    CanvaStyleEditor::SectionLabel("CAPAS");
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    ImGui::PushStyleColor(ImGuiCol_ChildBg, CanvaPalette::Surface1);
    ImGui::BeginChild("##ovLayerList", ImVec2(w, 150.0f), true, ImGuiWindowFlags_NoScrollWithMouse);
    if (m_Doc.layers.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
        ImGui::TextWrapped("Sin capas todavia. Usa la toolbar de arriba del canvas para anadir texto, formas, imagenes o un reloj.");
        ImGui::PopStyleColor();
    }
    for (int i = 0; i < (int)m_Doc.layers.size(); i++) {
        ImGui::PushID(i);
        bool isSel = (m_SelectedLayer == i);
        const auto& l = m_Doc.layers[i];

        std::string label;
        const char* tag = "[T] ";
        if (l.kind == OverlayLayerKind::Text) {
            label = l.text.empty() ? "(vacio)" : l.text;
            for (auto& c : label) if (c == '\n') c = ' ';
            tag = "[T] ";
        } else if (l.kind == OverlayLayerKind::Image) {
            label = l.imagePath;
            if (auto pos = label.find_last_of("/\\"); pos != std::string::npos)
                label = label.substr(pos + 1);
            if (label.empty()) label = "(imagen)";
            tag = "[I] ";
        } else if (l.kind == OverlayLayerKind::Clock) {
            label = "Reloj";
            tag = "[R] ";
        } else {
            label = (l.shapeKind == OverlayShapeKind::Ellipse) ? "Elipse" : "Rectangulo";
            tag = "[F] ";
        }
        if (label.size() > 20) label = label.substr(0, 17) + "...";
        label = std::string(tag) + label;

        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(
            CanvaPalette::Accent.x * 0.28f, CanvaPalette::Accent.y * 0.28f,
            CanvaPalette::Accent.z * 0.55f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, CanvaPalette::Surface2);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  CanvaPalette::Surface2);
        bool clicked = ImGui::Selectable(label.c_str(), isSel, 0, ImVec2(w - 76.0f, 0.0f));
        ImGui::PopStyleColor(3);
        if (clicked) m_SelectedLayer = i;

        // Reordenar (subir/bajar en z-order) -- swap con el vecino, no
        // cambia la cantidad de capas asi que es seguro seguir iterando.
        ImGui::SameLine(w - 68.0f);
        ImGui::BeginDisabled(i == 0);
        if (ImGui::SmallButton("^")) {
            std::swap(m_Doc.layers[i], m_Doc.layers[i - 1]);
            if      (m_SelectedLayer == i)     m_SelectedLayer = i - 1;
            else if (m_SelectedLayer == i - 1) m_SelectedLayer = i;
        }
        ImGui::EndDisabled();

        ImGui::SameLine(w - 48.0f);
        ImGui::BeginDisabled(i == (int)m_Doc.layers.size() - 1);
        if (ImGui::SmallButton("v")) {
            std::swap(m_Doc.layers[i], m_Doc.layers[i + 1]);
            if      (m_SelectedLayer == i)     m_SelectedLayer = i + 1;
            else if (m_SelectedLayer == i + 1) m_SelectedLayer = i;
        }
        ImGui::EndDisabled();

        ImGui::SameLine(w - 24.0f);
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

    // Scrolleable por separado del resto del sidebar (CAPAS queda fijo
    // arriba) -- esta seccion antes no tenia altura acotada y quedaba
    // cortada contra el borde inferior de la ventana (que ya no scrollea
    // como conjunto, ver kFlags en Render()), sin forma de ver los
    // controles que no entraban (sombra/contorno/fondo, etc).
    float propsH = std::max(80.0f, h - 212.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::BeginChild("##ovPropsScroll", ImVec2(w, propsH), false);

    if (m_SelectedLayer < 0 || m_SelectedLayer >= (int)m_Doc.layers.size()) {
        ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
        ImGui::TextWrapped("Selecciona o crea una capa para editar sus propiedades.");
        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    auto& layer = m_Doc.layers[m_SelectedLayer];

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        CanvaPalette::Surface1);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, CanvaPalette::Surface2);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

    constexpr ImGuiColorEditFlags kSwatchFlags = ImGuiColorEditFlags_AlphaBar |
        ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueWheel |
        ImGuiColorEditFlags_AlphaPreviewHalf;

    if (layer.kind == OverlayLayerKind::Text) {
        char textBuf[512];
        size_t len = std::min(layer.text.size(), sizeof(textBuf) - 1);
        memcpy(textBuf, layer.text.c_str(), len);
        textBuf[len] = '\0';
        ImGui::SetNextItemWidth(w);
        if (ImGui::InputTextMultiline("##ovLayerText", textBuf, sizeof(textBuf), ImVec2(w, 54.0f)))
            layer.text = textBuf;
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        RenderTextStyleProperties(layer, w);
    } else if (layer.kind == OverlayLayerKind::Clock) {
        ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
        ImGui::TextWrapped("Se reemplaza en vivo por el reloj/contador activo (ver panel Contadores).");
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        RenderTextStyleProperties(layer, w);
    } else if (layer.kind == OverlayLayerKind::Image) {
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
    } else { // Shape
        ImGui::TextUnformatted("Tipo de forma");
        ImGui::SetNextItemWidth(w);
        int shapeIdx = (layer.shapeKind == OverlayShapeKind::Ellipse) ? 1 : 0;
        const char* shapeNames[] = { "Rectangulo", "Elipse" };
        if (ImGui::BeginCombo("##ovShapeKind", shapeNames[shapeIdx])) {
            for (int s = 0; s < 2; s++) {
                bool sel = (shapeIdx == s);
                if (ImGui::Selectable(shapeNames[s], sel))
                    layer.shapeKind = (s == 1) ? OverlayShapeKind::Ellipse : OverlayShapeKind::Rectangle;
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::TextUnformatted("Tamano");
        ImGui::SetNextItemWidth(w);
        float sizePct[2] = { layer.sizeW * 100.0f, layer.sizeH * 100.0f };
        if (ImGui::DragFloat2("##ovShapeSize", sizePct, 0.5f, 2.0f, 100.0f, "%.0f%%")) {
            layer.sizeW = sizePct[0] / 100.0f;
            layer.sizeH = sizePct[1] / 100.0f;
        }

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::TextUnformatted("Rotacion");
        ImGui::SetNextItemWidth(w);
        ImGui::DragFloat("##ovShapeRotation", &layer.rotation, 0.5f, -180.0f, 180.0f, "%.0f grados");

        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::Checkbox("Relleno", &layer.shapeFilled);
        if (layer.shapeFilled) {
            ImGui::SameLine(w - 26.0f);
            ImGui::ColorEdit4("##ovShapeFillColor", layer.color, kSwatchFlags);
        }

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::Checkbox("Borde", &layer.outlineEnabled);
        if (layer.outlineEnabled) {
            ImGui::SameLine(w - 26.0f);
            ImGui::ColorEdit4("##ovShapeStrokeColor", layer.outlineColor, kSwatchFlags);
            ImGui::SetNextItemWidth(w);
            ImGui::DragFloat("##ovShapeStrokeWidth", &layer.outlineWidth, 0.2f, 0.5f, 20.0f, "%.1f px");
        }

        if (layer.shapeKind == OverlayShapeKind::Rectangle) {
            ImGui::Dummy(ImVec2(0.0f, 6.0f));
            ImGui::TextUnformatted("Redondeo de esquinas");
            ImGui::SetNextItemWidth(w);
            ImGui::DragFloat("##ovShapeRounding", &layer.shapeRounding, 0.2f, 0.0f, 200.0f, "%.0f px");
        }
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderTextStyleProperties — font/tamano/color/sombra/contorno/fondo,
//  compartido entre capas Text y Clock (ver RenderSidebar).
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::RenderTextStyleProperties(OverlayLayer& layer, float w) {
    constexpr ImGuiColorEditFlags kSwatchFlags = ImGuiColorEditFlags_AlphaBar |
        ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueWheel |
        ImGuiColorEditFlags_AlphaPreviewHalf;

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
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderFooter — nombre + Cancelar/Guardar (dispara la captura a PNG)
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::RenderFooter(ImVec2 winPos, ImVec2 winSize,
                                       OnSaveCallback& onSave, OnCancelCallback& onClose) {
    constexpr float kFooterH = 52.0f;
    float footerY = winSize.y - kFooterH;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddLine(
        ImVec2(winPos.x, winPos.y + footerY),
        ImVec2(winPos.x + winSize.x, winPos.y + footerY),
        CanvaPalette::ToU32(CanvaPalette::Border), 1.0f);
    dl->AddRectFilled(
        ImVec2(winPos.x, winPos.y + footerY),
        ImVec2(winPos.x + winSize.x, winPos.y + winSize.y),
        CanvaPalette::ToU32(CanvaPalette::Surface1));

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

    if (CanvaStyleEditor::GhostButton("  Cancelar  ", ImVec2(110.0f, 36.0f))) {
        m_IsOpen = false;
        if (onClose) onClose();
    }

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

        // Ventana invisible dedicada SOLO para conseguir un ImDrawList
        // correctamente inicializado via la API publica de ImGui (Begin/
        // GetWindowDrawList), en vez de armar uno a mano con internals
        // (fragil entre versiones de ImGui) -- se posiciona en el MISMO
        // lugar/tamano que el canvas real (necesario para que su clip rect
        // no recorte nada), pero sin fondo/inputs y solo dura este frame
        // (Guardar cierra el editor de inmediato despues).
        ImGui::SetNextWindowPos(m_CanvasScreenPos);
        ImGui::SetNextWindowSize(m_CanvasScreenSize);
        ImGui::Begin("##ovExportCapture", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoBringToFrontOnFocus);
        DrawLayersForExport(ImGui::GetWindowDrawList(), m_CanvasScreenPos, m_CanvasScreenSize);
        auto exportDl = std::make_shared<ImDrawList>(*ImGui::GetWindowDrawList());
        ImGui::End();

        // Captura por VALOR (no por referencia): onSave/onClose/name/docCopy
        // deben sobrevivir hasta ProcessPending() mas adelante en este mismo
        // frame, momento en el que este Render() ya retorno.
        OverlayExportService::Get().RequestCapture(
            exportDl, m_CanvasScreenPos, m_CanvasScreenSize,
            pngPath, expW, expH,
            [this, name, docCopy, onSave, onClose](bool ok) {
                if (ok) {
                    if (onSave) onSave(name, docCopy);
                    m_IsOpen = false;
                    if (onClose) onClose();
                } else {
                    m_SaveFailed = true;
                }
            });
    }
}

} // namespace ProyecThor::UI
