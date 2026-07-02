#pragma once
#include <string>
#include <vector>
#include <imgui.h>

namespace ProyecThor::UI {

struct VerseData {
    int         number = 0;
    std::string text;
    bool        edited = false;
};

struct ChapterData {
    int                    number = 0;
    std::vector<VerseData> verses;
};

struct BookData {
    std::string              name;
    std::vector<ChapterData> chapters;
    int                      canonicalNumber = 0;
};

struct BibleData {
    std::string           name;
    std::vector<BookData> books;
};

enum class BibleSection {
    Pentateuch, HistoricalOT, Wisdom, MajorProphets, MinorProphets,
    Gospels, Acts, PaulineEpistles, GeneralEpistles, Apocalypse
};

struct HistoryEntry {
    std::string ref;
    std::string fullText;
    int         bookIdx  = -1;
    int         chapIdx  = -1;
    int         verseIdx = -1;
};

class BibleView {
public:
    BibleView()  = default;
    ~BibleView() = default;

    void Render();

private:
    void         LoadXMLBible(const std::string& path);
    void         SaveVerseToXML(int bookIdx, int chapIdx, int verseIdx);
    void         ProjectVerse(int bookIdx, int chapIdx, int verseIdx);
    void         ReprojectInCurrentBible();
    bool         ParseSmartQuery(const std::string& raw,
                                  int& outBook, int& outChap, int& outVerse);
    void         NavigateVerse(int delta);
    BibleSection GetBookSection(int canonicalNumber) const;
    void         GetSectionColor(BibleSection section,
                                  float& r, float& g, float& b) const;

    void RenderTopBar();
    void RenderHistoryPopup();
    void RenderBookGrid();
    void RenderChapterGrid();
    void RenderVerseList();
    void RenderEditModal();

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

    // Busqueda inteligente
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

    // Edicion de versiculo
    bool        m_ShowEditModal = false;
    int         m_EditBookIdx   = -1;
    int         m_EditChapIdx   = -1;
    int         m_EditVerseIdx  = -1;
    char        m_EditBuffer[4096] = {};
    std::string m_EditStatus;

    // Scroll
    int m_ScrollToVerse = -1;
};

} // namespace ProyecThor::UI