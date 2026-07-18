#include "LayersStyleTab.h"
#include "LayersTheme.h"
#include "../../backend/core/PresentationCore.h"
#include <imgui.h>
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <cstdlib>
#include <pwd.h>
#include <unistd.h>
#endif
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace fs = std::filesystem;
namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  Rutas
// ─────────────────────────────────────────────────────────────────────────────
static fs::path GetAppDataDir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH] = {};
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf);
    fs::path dir = fs::path(buf) / "ProyecThor";
#else
    // En Linux/macOS seguimos la convencion XDG: usamos $XDG_DATA_HOME si
    // esta definida, o $HOME/.local/share en su defecto. Si tampoco existe
    // HOME, se consulta /etc/passwd como ultimo recurso.
    fs::path base;
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg) {
        base = xdg;
    } else if (const char* home = std::getenv("HOME"); home && *home) {
        base = fs::path(home) / ".local" / "share";
    } else if (struct passwd* pw = getpwuid(getuid())) {
        base = fs::path(pw->pw_dir) / ".local" / "share";
    } else {
        base = fs::current_path();
    }
    fs::path dir = base / "ProyecThor";
#endif
    std::error_code ec;
    fs::create_directories(dir / "themes", ec);
    return dir;
}
static fs::path ThemesDir() { return GetAppDataDir() / "themes"; }
static fs::path FontsDir()  { return GetAppDataDir() / "assets" / "fonts"; }

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor
// ─────────────────────────────────────────────────────────────────────────────
LayersStyleTab::LayersStyleTab() {
    m_CurrentStyle.vAlignment    = 1;
    m_CurrentStyle.textAlignment = 1;
    m_CurrentStyle.textSize      = 80.0f;
    m_CurrentStyle.autoScale     = true;
    m_CurrentStyle.selectedFont  = "Predeterminada";
    for (int i=0;i<4;i++) m_CurrentStyle.textColor[i] = 1.0f;
    for (int i=0;i<4;i++) m_CurrentStyle.margins[i]   = 60.0f;

    LoadThemeList();
    LoadFontsList();

    auto onFontImported = [this](const std::string& fontPath) {
        Core::PresentationCore::Get().LoadSingleFontIntoImGui(fontPath);
        ImGui::GetIO().Fonts->TexID = nullptr;
        LoadFontsList();
    };
    m_StyleEditor = std::make_unique<CanvaStyleEditor>(&m_AvailableFonts, onFontImported);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Loaders
// ─────────────────────────────────────────────────────────────────────────────
void LayersStyleTab::LoadThemeList() {
    m_AvailableThemes.clear();
    try {
        fs::path d = ThemesDir();
        if (fs::exists(d))
            for (const auto& e : fs::directory_iterator(d))
                if (e.path().extension()==".theme")
                    m_AvailableThemes.push_back(e.path().stem().string());
    } catch (...) {}
}

void LayersStyleTab::LoadFontsList() {
    m_AvailableFonts.clear();
    m_AvailableFonts.push_back("Predeterminada");
    try {
        fs::path d = FontsDir();
        fs::create_directories(d);
        if (fs::exists(d))
            for (const auto& e : fs::directory_iterator(d)) {
                std::string ext = e.path().extension().string();
                std::transform(ext.begin(),ext.end(),ext.begin(),::tolower);
                if (ext==".ttf"||ext==".otf"||ext==".ttc")
                    m_AvailableFonts.push_back(e.path().stem().string());
            }
    } catch (...) {}
}
void LayersStyleTab::ReloadFonts() { LoadFontsList(); }

// ─────────────────────────────────────────────────────────────────────────────
//  Serialización de temas
// ─────────────────────────────────────────────────────────────────────────────
bool LayersStyleTab::SaveTheme(const std::string& name, const StyleData& data) {
    if (name.empty()) return false;
    std::error_code ec;
    fs::path dir = ThemesDir();
    fs::create_directories(dir, ec);
    if (ec) return false;

    std::ofstream f(dir / (name + ".theme"));
    if (!f.is_open()) return false;

    f << "textColor="   << data.textColor[0] << "," << data.textColor[1] << ","
                        << data.textColor[2] << "," << data.textColor[3] << "\n";
    f << "textSize="    << data.textSize      << "\n";
    f << "textAlign="   << data.textAlignment << "\n";
    f << "vAlign="      << data.vAlignment    << "\n";
    f << "margins="     << data.margins[0] << "," << data.margins[1] << ","
                        << data.margins[2] << "," << data.margins[3] << "\n";
    f << "autoScale="   << (data.autoScale ? 1 : 0) << "\n";
    f << "font="        << data.selectedFont  << "\n";
    f << "refTextSize=" << data.refTextSize   << "\n";
    f << "verseTextSize=" << data.verseTextSize << "\n";
    f << "songTextAlign="  << data.songTextAlignment << "\n";
    f << "songVAlign="     << data.songVAlignment    << "\n";
    f << "bibleTextAlign=" << data.bibleTextAlignment << "\n";
    f << "bibleVAlign="    << data.bibleVAlignment   << "\n";
    return true;
}

bool LayersStyleTab::LoadThemeData(const std::string& name, StyleData& out) {
    std::ifstream f(ThemesDir() / (name + ".theme"));
    if (!f.is_open()) return false;

    out = StyleData{};

    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string k, v;
        if (!std::getline(ss, k, '=') || !std::getline(ss, v)) continue;
        v.erase(std::remove(v.begin(), v.end(), '\r'), v.end());
        v.erase(std::remove(v.begin(), v.end(), '\n'), v.end());

        if      (k == "textSize")       out.textSize          = std::stof(v);
        else if (k == "textAlign")      out.textAlignment     = std::stoi(v);
        else if (k == "vAlign")         out.vAlignment        = std::stoi(v);
        else if (k == "autoScale")      out.autoScale         = (std::stoi(v) != 0);
        else if (k == "font")           out.selectedFont      = v;
        else if (k == "refTextSize")    out.refTextSize       = std::stof(v);
        else if (k == "verseTextSize")  out.verseTextSize     = std::stof(v);
        else if (k == "songTextAlign")  out.songTextAlignment = std::stoi(v);
        else if (k == "songVAlign")     out.songVAlignment    = std::stoi(v);
        else if (k == "bibleTextAlign") out.bibleTextAlignment = std::stoi(v);
        else if (k == "bibleVAlign")    out.bibleVAlignment   = std::stoi(v);
        else if (k == "textColor")
            sscanf(v.c_str(), "%f,%f,%f,%f",
                &out.textColor[0], &out.textColor[1],
                &out.textColor[2], &out.textColor[3]);
        else if (k == "margins")
            sscanf(v.c_str(), "%f,%f,%f,%f",
                &out.margins[0], &out.margins[1],
                &out.margins[2], &out.margins[3]);
    }
    return true;
}

void LayersStyleTab::ApplyTheme(const std::string& name) {
    StyleData d;
    if (!LoadThemeData(name, d)) return;
    m_CurrentStyle  = d;
    m_SelectedTheme = name;
    ApplyCurrentStyleToCore();
}

void LayersStyleTab::DeleteTheme(const std::string& name) {
    std::error_code ec;
    fs::remove(ThemesDir()/(name+".theme"),ec);
    LoadThemeList();
    if (m_SelectedTheme==name) m_SelectedTheme.clear();
}

// FIXED: also call SetProjecting(true) so the projector re-renders with the
// updated style immediately (previously style changes were applied to the
// core state but the projector was left in its old state).
void LayersStyleTab::ApplyCurrentStyleToCore() {
    auto& core = Core::PresentationCore::Get();

    core.UpdateTextStyle(
        m_CurrentStyle.textSize,
        m_CurrentStyle.textColor,
        m_CurrentStyle.textAlignment,
        m_CurrentStyle.vAlignment,
        m_CurrentStyle.margins,
        m_CurrentStyle.autoScale,
        m_CurrentStyle.selectedFont);

    core.UpdateBibleStyle(
        m_CurrentStyle.refTextSize,
        m_CurrentStyle.verseTextSize,
        m_CurrentStyle.bibleTextAlignment,
        m_CurrentStyle.bibleVAlignment);

    core.UpdateSongStyle(
        m_CurrentStyle.songTextAlignment,
        m_CurrentStyle.songVAlignment);

    // FIXED: Notify projector that something changed so it re-draws.
    // Only do this if we're already projecting — don't start projection
    // just because the user tweaked a style setting.
    auto state = core.GetState();
    if (state.isProjecting) {
        core.SetProjecting(true);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Toolbar superior — compacta, solo iconos (estilo ProPresenter/Holyrics)
// ─────────────────────────────────────────────────────────────────────────────
void LayersStyleTab::RenderTopBar() {
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, LP::TextSub);
    ImGui::TextUnformatted("Estilos");
    ImGui::PopStyleColor();

    const float btnSz = 26.0f;
    const float zoomW = 76.0f;
    const float gap   = 4.0f;
    const float rowW  = zoomW + gap + btnSz*4 + gap*4;
    const float avail = ImGui::GetWindowContentRegionMax().x;
    ImGui::SameLine(std::max(ImGui::GetCursorPosX(), avail - rowW));

    if (m_GridMode) {
        LPZoomSlider("##stzoom", &m_ThumbZoom, 0.65f, 1.8f, zoomW);
        ImGui::SameLine(0, gap);
    } else {
        ImGui::Dummy(ImVec2(zoomW, btnSz));
        ImGui::SameLine(0, gap);
    }

    ImGui::PushID("styleview");
    if (LPCornerIconBtn("##sgridm", +[](ImDrawList* dl, ImVec2 c, float r, ImU32 col){
            float cs = r*0.42f, g = r*0.18f;
            for (int rI=0; rI<2; rI++) for (int cI=0; cI<2; cI++) {
                ImVec2 o = { c.x - cs - g*0.5f + cI*(cs+g), c.y - cs - g*0.5f + rI*(cs+g) };
                dl->AddRectFilled(o, {o.x+cs, o.y+cs}, col, 1.5f);
            }
        }, "Vista en cuadricula", {btnSz,btnSz}, m_GridMode))
        m_GridMode = true;
    ImGui::SameLine(0, gap);
    if (LPCornerIconBtn("##slistm", +[](ImDrawList* dl, ImVec2 c, float r, ImU32 col){
            for (int i=0;i<3;i++) {
                float y = c.y - r*0.5f + i*r*0.5f;
                dl->AddRectFilled({c.x-r*0.7f, y}, {c.x+r*0.7f, y+r*0.22f}, col, 1.0f);
            }
        }, "Vista en lista", {btnSz,btnSz}, !m_GridMode))
        m_GridMode = false;
    ImGui::PopID();

    ImGui::SameLine(0, gap*2);
    if (LPCornerIconBtn("##reloadfonts", LPDrawRefresh, "Recargar fuentes", {btnSz,btnSz}))
        LoadFontsList();
    ImGui::SameLine(0, gap);
    if (LPCornerIconBtn("##newstyle", LPDrawPlus, "Nuevo estilo", {btnSz,btnSz}, true))
        m_StyleEditor->OpenNew(m_CurrentStyle);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tarjeta de tema (grid)
// ─────────────────────────────────────────────────────────────────────────────
void LayersStyleTab::RenderThemeCard(const std::string& name, float W, float H,
                                     int idx, int col, int cols) {
    ImGui::PushID(idx);
    bool isSel = (m_SelectedTheme==name);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool hovRaw = ImGui::IsMouseHoveringRect(pos,{pos.x+W,pos.y+H});
    float t = LPHoverLerp(ImGui::GetID("##hov"), hovRaw);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float inset = 2.0f * t;
    ImVec2 p0 = {pos.x - inset, pos.y - inset};
    ImVec2 p1 = {pos.x + W + inset, pos.y + H + inset};

    ImU32 bg = isSel ? LPU32({0.18f,0.20f,0.36f,1.0f})
                     : LPU32(ImVec4(LP::Surface1.x+(LP::Surface2.x-LP::Surface1.x)*t,
                                    LP::Surface1.y+(LP::Surface2.y-LP::Surface1.y)*t,
                                    LP::Surface1.z+(LP::Surface2.z-LP::Surface1.z)*t, 1.0f));
    dl->AddRectFilled(p0,p1,bg,10.0f);

    ImVec4 borderC = isSel ? LP::Accent
                     : ImVec4(LP::Border.x+(LP::Accent.x-LP::Border.x)*0.4f*t,
                              LP::Border.y+(LP::Accent.y-LP::Border.y)*0.4f*t,
                              LP::Border.z+(LP::Accent.z-LP::Border.z)*0.4f*t,
                              LP::Border.w+(0.4f-LP::Border.w)*t);
    dl->AddRect(p0,p1,LPU32(borderC),10.0f,0,isSel?2.0f:(1.0f+0.4f*t));

    ImVec2 pp = {p0.x+12.0f, p0.y+12.0f};
    dl->AddText(ImGui::GetFont(),14.0f,pp,LPU32({1,1,1,0.95f}),"Aa Bb");
    dl->AddText(ImGui::GetFont(),10.0f,{pp.x,pp.y+18.0f},LPU32({0.65f,0.65f,0.65f,0.75f}),"123 — Gz");

    if (isSel) LPBadge(dl,{p1.x-52.0f,p0.y+7.0f},"ACTIVO",LP::Accent,{1,1,1,1});

    std::string dn=name.length()>17?name.substr(0,14)+"...":name;
    dl->AddRectFilled({p0.x,p1.y-24.0f},{p1.x,p1.y},
        LPU32({0,0,0,0.52f}),10.0f,ImDrawFlags_RoundCornersBottom);
    ImVec2 ns=ImGui::CalcTextSize(dn.c_str());
    dl->AddText({p0.x+(W-ns.x)*0.5f,p1.y-20.0f},
        isSel?LPU32(LP::Accent):LPU32(LP::Text),dn.c_str());

    ImGui::InvisibleButton("##tcard",{W,H});
    if (ImGui::IsItemClicked()) ApplyTheme(name);

    if (ImGui::BeginPopupContextItem("ThCtx")) {
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Accent);
        ImGui::Text("%s", name.c_str()); ImGui::PopStyleColor();
        ImGui::Separator();
        if (ImGui::Selectable("  Editar")) {
            StyleData ed; if (LoadThemeData(name,ed)) m_StyleEditor->OpenEdit(name,ed);
        }
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Red);
        if (ImGui::Selectable("  Eliminar")) DeleteTheme(name);
        ImGui::PopStyleColor();
        ImGui::EndPopup();
    }

    if (col<cols-1) ImGui::SameLine();
    ImGui::PopID();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Fila de tema (lista)
// ─────────────────────────────────────────────────────────────────────────────
void LayersStyleTab::RenderThemeRow(const std::string& name, float W, float rowH, int idx) {
    ImGui::PushID(idx);
    bool isSel = (m_SelectedTheme == name);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool hovRaw = ImGui::IsMouseHoveringRect(pos, {pos.x + W, pos.y + rowH});
    float t = LPHoverLerp(ImGui::GetID("##hov"), hovRaw);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImU32 bg = LPU32(ImVec4(LP::Surface1.x+(LP::Surface2.x-LP::Surface1.x)*t,
                             LP::Surface1.y+(LP::Surface2.y-LP::Surface1.y)*t,
                             LP::Surface1.z+(LP::Surface2.z-LP::Surface1.z)*t, 1.0f));
    if (isSel) bg = LPU32(ImVec4(0.15f, 0.16f, 0.20f, 1.0f));
    dl->AddRectFilled(pos, {pos.x + W, pos.y + rowH}, bg, 3.0f);

    if (isSel) {
        dl->AddRectFilled(pos, {pos.x + 4.0f, pos.y + rowH}, LPU32(LP::Accent), 3.0f, ImDrawFlags_RoundCornersLeft);
    } else if (t > 0.01f) {
        dl->AddRectFilled(pos, {pos.x + 4.0f, pos.y + rowH}, LPU32({LP::Accent.x, LP::Accent.y, LP::Accent.z, 0.3f*t}), 3.0f, ImDrawFlags_RoundCornersLeft);
    }

    float px = pos.x + 16.0f, py = pos.y + 8.0f;
    dl->AddText(ImGui::GetFont(), 14.0f, {px, py}, LPU32({1,1,1,0.9f}), "Aa");
    dl->AddText(ImGui::GetFont(), 10.0f, {px, py + 18.0f}, LPU32({0.6f,0.6f,0.6f,0.7f}), "123");

    std::string dn = name.length() > 28 ? name.substr(0, 25) + "..." : name;
    float txtX = px + 44.0f, txtY = pos.y + (rowH - ImGui::GetTextLineHeight()) * 0.5f;
    dl->AddText({txtX, txtY}, isSel ? LPU32(ImVec4(1,1,1,1)) : LPU32(LP::Text), dn.c_str());

    ImGui::InvisibleButton("##trow", {W, rowH});
    if (ImGui::IsItemClicked()) ApplyTheme(name);

    if (ImGui::BeginPopupContextItem("ThCtxL")) {
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Accent);
        ImGui::Text("%s", name.c_str()); ImGui::PopStyleColor();
        ImGui::Separator();
        if (ImGui::Selectable("  Editar")) {
            StyleData ed; if (LoadThemeData(name,ed)) m_StyleEditor->OpenEdit(name,ed);
        }
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Red);
        if (ImGui::Selectable("  Eliminar")) DeleteTheme(name);
        ImGui::PopStyleColor();
        ImGui::EndPopup();
    }

    ImGui::Dummy({0, 4.0f});
    ImGui::PopID();
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderThemeGrid (Ajustado para evitar márgenes negativos)
// ─────────────────────────────────────────────────────────────────────────────
void LayersStyleTab::RenderThemeGrid() {
    RenderTopBar();
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    LPSeparatorLine();
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    if (m_AvailableThemes.empty()) {
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w  = ImGui::GetContentRegionAvail().x;
        ImGui::GetWindowDrawList()->AddRectFilled(p, {p.x + w, p.y + 60}, LPU32(LP::Surface1), 12.0f);
        ImGui::Dummy({0, 18});
        ImGui::PushStyleColor(ImGuiCol_Text, LP::TextMuted);
        const char* h = "Crea tu primer estilo con el boton + de arriba";
        float tw = ImGui::CalcTextSize(h).x;
        ImGui::SetCursorPosX(std::max(0.0f, (w - tw) * 0.5f));
        ImGui::Text("%s", h);
        ImGui::PopStyleColor();
        ImGui::Dummy({0, 8});
        return;
    }

    if (m_GridMode) {
        const float cW = 145.0f * m_ThumbZoom, cH = 90.0f * m_ThumbZoom, gap2 = 10.0f;
        float pW = ImGui::GetContentRegionAvail().x;
        
        int cols = std::max(1, (int)((pW + gap2) / (cW + gap2)));
        
        // Centrado dinámico de la cuadrícula (Grid)
        float totalGridWidth = (cols * cW) + ((cols - 1) * gap2);
        float offsetX = std::max(0.0f, (pW - totalGridWidth) * 0.5f);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {gap2, gap2});
        for (int i = 0; i < (int)m_AvailableThemes.size(); i++) {
            if (i % cols == 0) {
                ImGui::SetCursorPosX(offsetX); // Empuja el inicio de cada fila al centro
            }
            RenderThemeCard(m_AvailableThemes[i], cW, cH, i, i % cols, cols);
        }
        ImGui::PopStyleVar();
    } else {
        float pW = ImGui::GetContentRegionAvail().x;
        for (int i = 0; i < (int)m_AvailableThemes.size(); i++)
            RenderThemeRow(m_AvailableThemes[i], pW, 48.0f, i);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ajustes Rapidos (Adaptado para panel lateral, sin CollapsingHeader)
// ─────────────────────────────────────────────────────────────────────────────
void LayersStyleTab::RenderQuickAdjust() {
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    
    // Titulo limpio en lugar de un header colapsable que roba espacio
    ImGui::PushStyleColor(ImGuiCol_Text, LP::TextSub);
    ImGui::Text(" AJUSTES RÁPIDOS");
    ImGui::PopStyleColor();
    LPSeparatorLine();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    bool changed = false;

    if (ImGui::BeginTable("##QuickAdjustTable", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 85.0f);
        ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthStretch);

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, LP::Surface1);

        // --- FUENTE ---
        ImGui::TableNextRow(); ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(LP::TextMuted, "Fuente");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##qf", m_CurrentStyle.selectedFont.c_str())) {
            for (const auto& f : m_AvailableFonts) {
                bool sel = (m_CurrentStyle.selectedFont == f);
                if (ImGui::Selectable(f.c_str(), sel)) {
                    m_CurrentStyle.selectedFont = f;
                    changed = true;
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // --- COLOR ---
        ImGui::TableNextRow(); ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(LP::TextMuted, "Color Base");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        changed |= ImGui::ColorEdit4("##qc", m_CurrentStyle.textColor,
            ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs |
            ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_AlphaPreviewHalf);

        // --- TAMAÑO ---
        ImGui::TableNextRow(); ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(LP::TextMuted, "Tamanio");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        changed |= ImGui::DragFloat("##qs", &m_CurrentStyle.textSize, 1.0f, 10.0f, 500.0f, "%.1f px");

        // --- ALINEACIÓN HORIZONTAL ---
        ImGui::TableNextRow(); ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(LP::TextMuted, "Alineacion H");
        ImGui::TableNextColumn();

        const char* hA[] = {"Izq", "Cen", "Der"};
        float btnW = ImGui::GetContentRegionAvail().x / 3.0f;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        for (int a = 0; a < 3; a++) {
            if (a > 0) ImGui::SameLine();
            bool act = (m_CurrentStyle.textAlignment == a);
            ImGui::PushStyleColor(ImGuiCol_Button,        act ? ImVec4(0.3f,0.3f,0.3f,1.0f) : LP::Surface0);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, act ? ImVec4(0.35f,0.35f,0.35f,1.0f) : LP::Surface2);
            ImGui::PushStyleColor(ImGuiCol_Text,          act ? ImVec4(1,1,1,1) : LP::TextSub);
            float rounding = (a == 0 || a == 2) ? 3.0f : 0.0f;
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rounding);
            std::string btnId = std::string(hA[a]) + "##qa" + std::to_string(a);
            if (ImGui::Button(btnId.c_str(), ImVec2(btnW, 26))) {
                m_CurrentStyle.textAlignment = a;
                changed = true;
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
        }
        ImGui::PopStyleVar();

        // --- ALINEACIÓN VERTICAL ---
        ImGui::TableNextRow(); ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(LP::TextMuted, "Alineacion V");
        ImGui::TableNextColumn();

        const char* vA[] = {"Arr", "Cen", "Aba"};
        btnW = ImGui::GetContentRegionAvail().x / 3.0f;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        for (int a = 0; a < 3; a++) {
            if (a > 0) ImGui::SameLine();
            bool act = (m_CurrentStyle.vAlignment == a);
            ImGui::PushStyleColor(ImGuiCol_Button,        act ? ImVec4(0.3f,0.3f,0.3f,1.0f) : LP::Surface0);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, act ? ImVec4(0.35f,0.35f,0.35f,1.0f) : LP::Surface2);
            ImGui::PushStyleColor(ImGuiCol_Text,          act ? ImVec4(1,1,1,1) : LP::TextSub);
            float rounding = (a == 0 || a == 2) ? 3.0f : 0.0f;
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rounding);
            std::string btnId = std::string(vA[a]) + "##qv" + std::to_string(a);
            if (ImGui::Button(btnId.c_str(), ImVec2(btnW, 26))) {
                m_CurrentStyle.vAlignment = a;
                changed = true;
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
        }
        ImGui::PopStyleVar();

        ImGui::PopStyleColor(); // FrameBg
        ImGui::PopStyleVar();   // FrameRounding
        ImGui::EndTable();
    }

    if (changed) ApplyCurrentStyleToCore();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Modal del editor de estilos (Se mantiene igual)
// ─────────────────────────────────────────────────────────────────────────────
void LayersStyleTab::RenderStyleEditorModal() {
    if (!m_StyleEditor) return;
    m_StyleEditor->Render([this](const std::string& name, const StyleData& data) {
        if (SaveTheme(name, data)) {
            LoadThemeList();
            m_CurrentStyle  = data;
            m_SelectedTheme = name;
            ApplyCurrentStyleToCore();
        }
    });
}

// ─────────────────────────────────────────────────────────────────────────────
//  Render principal del tab (AHORA CON LAYOUT DE 2 COLUMNAS)
// ─────────────────────────────────────────────────────────────────────────────
void LayersStyleTab::Render() {
    // ── GALERÍA DE TEMAS (arriba) ──────────────────────────────────────────
    float availH    = ImGui::GetContentRegionAvail().y;
    float topHeight = availH * 0.55f;   // 55% para la galería

    ImGui::BeginChild("##ThemesListChild", ImVec2(0, topHeight), false);
    RenderThemeGrid();
    ImGui::EndChild();

    // Separador visual entre secciones
    LPSeparatorLine();

    // ── AJUSTES RÁPIDOS (abajo) ────────────────────────────────────────────
    ImGui::BeginChild("##QuickAdjustChild", ImVec2(0, 0), false);
    RenderQuickAdjust();
    ImGui::EndChild();

    // Modal del editor (siempre al final, fuera de children)
    RenderStyleEditorModal();
}

} // namespace ProyecThor::UI