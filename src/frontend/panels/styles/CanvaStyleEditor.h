#pragma once
#include <imgui.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "backend/core/PresentationCore.h"

namespace ProyecThor::Settings { struct ThemeSettings; }

namespace ProyecThor::UI {

class TabTypography;
class TabAlignment;
class TabMargins;
class TabEffects;

struct CanvaPalette {
    static inline ImVec4 Accent       = ImVec4(0.39f, 0.44f, 0.97f, 1.0f);
    static inline ImVec4 AccentHov    = ImVec4(0.49f, 0.54f, 1.00f, 1.0f);
    static inline ImVec4 AccentActive = ImVec4(0.30f, 0.35f, 0.90f, 1.0f);
    static inline ImVec4 Green        = ImVec4(0.10f, 0.79f, 0.55f, 1.0f);
    static inline ImVec4 Red          = ImVec4(0.93f, 0.26f, 0.36f, 1.0f);
    static inline ImVec4 Surface0     = ImVec4(0.09f, 0.09f, 0.11f, 1.0f);
    static inline ImVec4 Surface1     = ImVec4(0.12f, 0.13f, 0.16f, 1.0f);
    static inline ImVec4 Surface2     = ImVec4(0.16f, 0.17f, 0.22f, 1.0f);
    static inline ImVec4 Border       = ImVec4(0.22f, 0.23f, 0.30f, 1.0f);
    static inline ImVec4 Text         = ImVec4(0.92f, 0.92f, 0.94f, 1.0f);
    static inline ImVec4 TextMuted    = ImVec4(0.50f, 0.52f, 0.60f, 1.0f);
    static inline ImVec4 Gold         = ImVec4(0.95f, 0.72f, 0.20f, 1.0f);
    static inline ImVec4 Pink         = ImVec4(0.93f, 0.40f, 0.70f, 1.0f);
    static ImU32 ToU32(const ImVec4& c);

    static void Sync(const ProyecThor::Settings::ThemeSettings& theme);
};

// StyleData — parametros de estilo de una presentacion. Alineacion:
// 0 = izquierda/arriba, 1 = centro, 2 = derecha/abajo.
struct StyleData {
    std::string selectedFont  = "Predeterminada";
    float       textColor[4]  = { 1.0f, 1.0f, 1.0f, 1.0f };
    float       textSize      = 80.0f;
    float       refTextSize   = 28.0f;   // tamano de la linea de referencia biblica
    float       verseTextSize = 60.0f;   // tamano del cuerpo del versiculo
    bool        autoScale     = true;
    float       margins[4]    = { 80.0f, 60.0f, 80.0f, 60.0f }; // L T R B en px a 1920x1080
    int         textAlignment = 1;
    int         vAlignment    = 1;

    int         songTextAlignment  = 1;
    int         songVAlignment     = 1;
    int         bibleTextAlignment = 1;
    int         bibleVAlignment    = 1;

    ProyecThor::Core::TextEffectsData effects;
};

class CanvaStyleEditor {
public:
    using OnSaveCallback         = std::function<void(const std::string&, const StyleData&)>;
    using OnFontImportedCallback = std::function<void(const std::string&)>;

    explicit CanvaStyleEditor(std::vector<std::string>* fontList,
                               OnFontImportedCallback    onFontImported = nullptr);
    ~CanvaStyleEditor();

    void OpenNew(const StyleData& defaults = {});
    void OpenEdit(const std::string& existingName, const StyleData& existingData);

    // Devuelve true el frame en que el usuario presiona Guardar.
    // embedded=true: dibuja el contenido dentro de la ventana ya activa
    // en vez de abrir una ventana flotante propia.
    bool Render(OnSaveCallback onSave, bool embedded = false);

    bool IsOpen() const { return m_IsOpen; }

    static bool PrimaryButton(const char* label, ImVec2 size = {});
    static bool GhostButton  (const char* label, ImVec2 size = {});
    static void Badge        (const char* label, ImVec4 color);
    static void SectionLabel (const char* label);
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
    std::unique_ptr<TabEffects>    m_TabEffects;

    std::vector<std::string>* m_FontList = nullptr;

    StyleData   m_Data;
    char        m_Name[128]         = {};
    bool        m_IsOpen            = false;
    bool        m_IsEditingExisting = false;
    int         m_ActiveTab         = 0;
    bool        m_LongPreview       = false;
};

} // namespace ProyecThor::UI