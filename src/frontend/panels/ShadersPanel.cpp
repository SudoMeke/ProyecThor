#include "ShadersPanel.h"
#include "frontend/ui/DesignSystem.h"
#include "backend/settings/SettingsManager.h"
#include "backend/core/PresentationCore.h"
#include <imgui.h>

namespace ProyecThor::UI {

void ShadersPanel::RenderContent() {
    auto& settingsMgr = Settings::SettingsManager::Get();
    auto& p           = settingsMgr.GetSettings().projection;
    auto& core        = Core::PresentationCore::Get();
    bool  changed     = false;

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.68f, 0.78f, 1.0f));
    ImGui::TextWrapped("Efectos de post-proceso sobre la salida en vivo (Audiencia). Los cambios se aplican al instante.");
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    const ImVec2 toggleSize(140.0f, 0.0f);
    constexpr float sliderW = 180.0f;

    // Cada bloque usa su propio PushID: GlassButton dibuja el label tal
    // cual (no le aplica la convencion "##" de ImGui::Button), asi que las
    // 4 secciones comparten el mismo texto visible ("Activado"/
    // "Desactivado") sin colisionar en ID solo porque cada una vive en su
    // propio scope de ID.

    // ── FSR (upscale + sharpen) ──────────────────────────────────────────
    ImGui::PushID("fsr");
    DS::GlassSectionHeader("FSR (upscale + sharpen)");
    if (DS::GlassButton(p.fsrEnabled ? "Activado" : "Desactivado", toggleSize,
                        p.fsrEnabled ? DS::SuccessColor : DS::AccentColor)) {
        p.fsrEnabled = !p.fsrEnabled;
        core.SetFSREnabled(p.fsrEnabled);
        changed = true;
    }
    if (p.fsrEnabled) {
        ImGui::SameLine(0.0f, 14.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Nitidez");
        ImGui::SameLine();
        if (DS::ModernSlider("##sharpness", &p.fsrSharpness, 0.0f, 2.0f, sliderW)) {
            core.SetFSRSharpness(p.fsrSharpness);
            changed = true;
        }
    }
    ImGui::PopID();
    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    DS::GlassSeparator();
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    // ── Modo CRT ──────────────────────────────────────────────────────────
    ImGui::PushID("crt");
    DS::GlassSectionHeader("Modo CRT");
    if (DS::GlassButton(p.crtEnabled ? "Activado" : "Desactivado", toggleSize,
                        p.crtEnabled ? DS::SuccessColor : DS::AccentColor)) {
        p.crtEnabled = !p.crtEnabled;
        core.SetCRTEnabled(p.crtEnabled);
        changed = true;
    }
    if (p.crtEnabled) {
        ImGui::SameLine(0.0f, 14.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Intensidad de scanlines");
        ImGui::SameLine();
        if (DS::ModernSlider("##scanline", &p.crtScanlineIntensity, 0.0f, 1.0f, sliderW)) {
            core.SetCRTScanlineIntensity(p.crtScanlineIntensity);
            changed = true;
        }
    }
    ImGui::PopID();
    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    DS::GlassSeparator();
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    // ── Grano ─────────────────────────────────────────────────────────────
    ImGui::PushID("grain");
    DS::GlassSectionHeader("Grano");
    if (DS::GlassButton(p.grainEnabled ? "Activado" : "Desactivado", toggleSize,
                        p.grainEnabled ? DS::SuccessColor : DS::AccentColor)) {
        p.grainEnabled = !p.grainEnabled;
        core.SetGrainEnabled(p.grainEnabled);
        changed = true;
    }
    if (p.grainEnabled) {
        ImGui::SameLine(0.0f, 14.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Intensidad");
        ImGui::SameLine();
        if (DS::ModernSlider("##intensity", &p.grainIntensity, 0.0f, 1.0f, sliderW)) {
            core.SetGrainIntensity(p.grainIntensity);
            changed = true;
        }
    }
    ImGui::PopID();
    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    DS::GlassSeparator();
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    // ── FXAA (antialiasing) ──────────────────────────────────────────────
    ImGui::PushID("fxaa");
    DS::GlassSectionHeader("FXAA (antialiasing)");
    if (DS::GlassButton(p.fxaaEnabled ? "Activado" : "Desactivado", toggleSize,
                        p.fxaaEnabled ? DS::SuccessColor : DS::AccentColor)) {
        p.fxaaEnabled = !p.fxaaEnabled;
        core.SetFXAAEnabled(p.fxaaEnabled);
        changed = true;
    }
    ImGui::PopID();

    if (changed) settingsMgr.Save();
}

} // namespace ProyecThor::UI
