#pragma once
#include <imgui.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace ProyecThor::UI {

class TabTypography;
class TabAlignment;
class TabMargins;

// ─────────────────────────────────────────────────────────────────────────────
//  Paleta de colores del editor
// ─────────────────────────────────────────────────────────────────────────────
struct CanvaPalette {
    static const ImVec4 Accent;
    static const ImVec4 AccentHov;
    static const ImVec4 AccentActive;
    static const ImVec4 Green;
    static const ImVec4 Red;
    static const ImVec4 Surface0;
    static const ImVec4 Surface1;
    static const ImVec4 Surface2;
    static const ImVec4 Border;
    static const ImVec4 Text;
    static const ImVec4 TextMuted;
    static const ImVec4 Gold;
    static const ImVec4 Pink;
    static ImU32 ToU32(const ImVec4& c);
};

// ─────────────────────────────────────────────────────────────────────────────
//  StyleData — todos los parametros de estilo de una presentacion
//
//  textAlignment / vAlignment : alineacion del estilo base (usado en preview
//                               y como valor por defecto si no se sobreescribe)
//  songTextAlignment          : alineacion horizontal para el modulo Canciones
//  songVAlignment             : alineacion vertical  para el modulo Canciones
//  bibleTextAlignment         : alineacion horizontal para el modulo Biblia
//  bibleVAlignment            : alineacion vertical  para el modulo Biblia
//
//  Valores de alineacion horizontal: 0 = izquierda, 1 = centro, 2 = derecha
//  Valores de alineacion vertical:   0 = arriba,    1 = centro, 2 = abajo
// ─────────────────────────────────────────────────────────────────────────────
struct StyleData {
    std::string selectedFont  = "Predeterminada";
    float       textColor[4]  = { 1.0f, 1.0f, 1.0f, 1.0f };
    float       textSize      = 80.0f;
    float       refTextSize   = 28.0f;   // tamano de la linea de referencia biblica
    float       verseTextSize = 60.0f;   // tamano del cuerpo del versiculo
    bool        autoScale     = true;
    float       margins[4]    = { 80.0f, 60.0f, 80.0f, 60.0f }; // L T R B en px a 1920x1080
    int         textAlignment = 1;   // alineacion base (preview)
    int         vAlignment    = 1;

    // Alineacion especifica por modulo
    int         songTextAlignment  = 1;
    int         songVAlignment     = 1;
    int         bibleTextAlignment = 1;
    int         bibleVAlignment    = 1;
};

// ─────────────────────────────────────────────────────────────────────────────
//  CanvaStyleEditor
// ─────────────────────────────────────────────────────────────────────────────
class CanvaStyleEditor {
public:
    using OnSaveCallback         = std::function<void(const std::string&, const StyleData&)>;
    using OnFontImportedCallback = std::function<void(const std::string&)>;

    explicit CanvaStyleEditor(std::vector<std::string>* fontList,
                               OnFontImportedCallback    onFontImported = nullptr);
    ~CanvaStyleEditor();

    void OpenNew(const StyleData& defaults = {});
    void OpenEdit(const std::string& existingName, const StyleData& existingData);

    // Devuelve true el frame en que el usuario presiona Guardar
    bool Render(OnSaveCallback onSave);

    bool IsOpen() const { return m_IsOpen; }

    // Helpers de widgets estaticos (usados por los tabs)
    static bool PrimaryButton(const char* label, ImVec2 size = {});
    static bool GhostButton  (const char* label, ImVec2 size = {});
    static void Badge        (const char* label, ImVec4 color);
    static void SectionLabel (const char* label);
    // Cambiar la firma de SegmentedButtons:
static void SegmentedButtons(const char* prefix,
                              const char** labels, int count, int* current,
                              float totalWidth, float height,
                              const ImVec4& activeColor);

private:
    void RenderHeader    (ImDrawList* dl, ImVec2 winPos, ImVec2 winSize);
    void RenderActiveTab (float colWidth, float contentH, float tabH, ImDrawList* dl);
    void RenderPreview   (ImVec2 pos, ImVec2 sz, ImDrawList* dl);
    void RenderFooter    (ImVec2 winPos, ImVec2 winSize,
                          OnSaveCallback& onSave, bool& savedThisFrame);

    std::unique_ptr<TabTypography> m_TabTypography;
    std::unique_ptr<TabAlignment>  m_TabAlignment;
    std::unique_ptr<TabMargins>    m_TabMargins;

    std::vector<std::string>* m_FontList = nullptr;

    StyleData   m_Data;
    char        m_Name[128]         = {};
    bool        m_IsOpen            = false;
    bool        m_IsEditingExisting = false;
    int         m_ActiveTab         = 0;
    bool        m_LongPreview       = false;
};

} // namespace ProyecThor::UI