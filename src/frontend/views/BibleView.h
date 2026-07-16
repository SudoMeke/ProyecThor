#pragma once

#include <string>
#include <vector>
#include <imgui.h>
#include "biblia/BibleTypes.h"
#include "biblia/BibleQuickNav.h"

namespace ProyecThor::UI {

class BibleView {
public:
    BibleView()  = default;
    ~BibleView() = default;

    void Render();

private:
    void LoadXMLBible(const std::string& path);
    void SaveVerseToXML(int bookIdx, int chapIdx, int verseIdx);
    void ProjectVerse(int bookIdx, int chapIdx, int verseIdx);
    void ReprojectInCurrentBible();
    void NavigateVerse(int delta);
    // Texto del proximo versiculo (mismo rollover capitulo/libro que
    // NavigateVerse), sin mutar estado. Vacio si no hay siguiente. Solo para
    // el Stage Display, nunca se muestra al publico.
    std::string PeekNextVerseText(int bookIdx, int chapIdx, int verseIdx) const;

    void RenderTopBar();
    void RenderHistoryPopup();
    void RenderBookGrid();
    void RenderChapterGrid();
    void RenderVerseList();
    void RenderEditModal();
enum class JumpKind { None, Chapter, Verse };

// Salto rapido de capitulo/versiculo (tap de Ctrl / Alt)
void UpdateModifierTaps();
void OpenJump(JumpKind kind);
void CloseJump();
void UpdateJumpOverlay();
void RenderJumpOverlay();
void ConfirmJump();

JumpKind    m_JumpMode = JumpKind::None;
std::string m_JumpBuffer;
std::string m_JumpStatus;
ImVec2      m_JumpCardMin = {};
ImVec2      m_JumpCardMax = {};

double m_CtrlDownSince  = -1.0;
bool   m_CtrlComboFired = false;
double m_AltDownSince   = -1.0;
bool   m_AltComboFired  = false;
    // Aplica la seleccion confirmada del buscador rapido (biblia/BibleQuickNav)
    void HandleQuickNavConfirm();

    // Datos de la Biblia activa
    BibleData   m_CurrentBible;
    bool        m_BibleLoaded     = false;
    std::string m_LoadedBiblePath;
    std::string m_LastSelectedFile;

    // Seleccion
    int m_SelectedBook    = 0;
    int m_SelectedChapter = 0;
    int m_SelectedVerse   = 0;

    // Proyeccion activa
    int m_ProjectedBookNum  = -1;
    int m_ProjectedChapNum  = -1;
    int m_ProjectedVerseNum = -1;
    int m_ProjectedBookIdx  = -1;
    int m_ProjectedChapIdx  = -1;
    int m_ProjectedVerseIdx = -1;

    // Busqueda inteligente (barra de texto en la parte superior)
    char m_LiveSearch[128]  = "";
    int  m_FilteredBook     = -1;
    int  m_FilteredChapter  = -1;
    bool m_SearchFocused    = false;
    bool m_NeedsFocusSearch = false;

    // Historial
    std::vector<HistoryEntry> m_History;
    bool   m_ShowHistory    = false;
    ImVec2 m_HistoryBtnPos  = {};
    ImVec2 m_HistoryBtnSize = {};

    // Buscador rapido tipo "quick nav" (overlay, letra por letra, con
    // previsualizacion en vivo de Libro / Capitulo / Versiculo)
    BibleQuickNav m_QuickNav;
    ImVec2        m_QuickNavBtnPos  = {};
    ImVec2        m_QuickNavBtnSize = {};

    // Edicion de versiculo
    bool        m_ShowEditModal    = false;
    int         m_EditBookIdx      = -1;
    int         m_EditChapIdx      = -1;
    int         m_EditVerseIdx     = -1;
    char        m_EditBuffer[4096] = {};
    std::string m_EditStatus;

    // Scroll
    int m_ScrollToVerse = -1;
};

} // namespace ProyecThor::UI