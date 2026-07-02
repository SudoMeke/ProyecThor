#include "BibleView.h"
#include "backend/core/PresentationCore.h"
#include "backend/core/AppPaths.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cmath>
#include "frontend/ui/bin/StyleGeneralApp.h"

namespace fs = std::filesystem;

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers internos
// ─────────────────────────────────────────────────────────────────────────────

static std::string ToLowerUTF8(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s)
        out += static_cast<char>(std::tolower(c));
    return out;
}

static std::string StripAccents(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) { out += static_cast<char>(c); i++; }
        else if (c == 0xC3 && i + 1 < s.size()) {
            unsigned char n = static_cast<unsigned char>(s[i + 1]);
            char rep = '?';
            if      (n >= 0xA0 && n <= 0xA5) rep = 'a';
            else if (n >= 0xA8 && n <= 0xAB) rep = 'e';
            else if (n >= 0xAC && n <= 0xAF) rep = 'i';
            else if (n >= 0xB2 && n <= 0xB6) rep = 'o';
            else if (n >= 0xB9 && n <= 0xBC) rep = 'u';
            else if (n == 0xB1)              rep = 'n';
            else if (n >= 0x80 && n <= 0x85) rep = 'a';
            else if (n >= 0x88 && n <= 0x8B) rep = 'e';
            else if (n >= 0x8C && n <= 0x8F) rep = 'i';
            else if (n >= 0x92 && n <= 0x96) rep = 'o';
            else if (n >= 0x99 && n <= 0x9C) rep = 'u';
            else if (n == 0x91)              rep = 'n';
            else rep = static_cast<char>(n);
            out += rep; i += 2;
        } else { i++; }
    }
    return out;
}

static std::string Normalize(const std::string& s) {
    std::string r = StripAccents(ToLowerUTF8(s));
    r.erase(std::remove(r.begin(), r.end(), ' '), r.end());
    return r;
}

static ImU32 Col(float r, float g, float b, float a = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tabla de abreviaturas
// ─────────────────────────────────────────────────────────────────────────────

static const std::pair<const char*, int> kAbbrevTable[] = {
    {"genesis",1},{"gen",1},{"gn",1},
    {"exodo",2},{"exo",2},{"ex",2},
    {"levitico",3},{"lev",3},{"lv",3},
    {"numeros",4},{"num",4},{"nm",4},
    {"deuteronomio",5},{"deut",5},{"dt",5},
    {"josue",6},{"jos",6},
    {"jueces",7},{"jue",7},{"jc",7},
    {"rut",8},{"rt",8},
    {"1samuel",9},{"1sam",9},{"1s",9},
    {"2samuel",10},{"2sam",10},{"2s",10},
    {"1reyes",11},{"1re",11},{"1r",11},
    {"2reyes",12},{"2re",12},{"2r",12},
    {"1cronicas",13},{"1cro",13},{"1cr",13},
    {"2cronicas",14},{"2cro",14},{"2cr",14},
    {"esdras",15},{"esd",15},
    {"nehemias",16},{"neh",16},
    {"ester",17},{"est",17},
    {"job",18},
    {"salmos",19},{"sal",19},{"ps",19},{"sl",19},
    {"proverbios",20},{"prov",20},{"pr",20},
    {"eclesiastes",21},{"ecl",21},{"qo",21},
    {"cantares",22},{"cnt",22},{"ct",22},{"can",22},
    {"isaias",23},{"isa",23},{"is",23},
    {"jeremias",24},{"jer",24},{"jr",24},
    {"lamentaciones",25},{"lam",25},
    {"ezequiel",26},{"eze",26},{"ez",26},
    {"daniel",27},{"dan",27},{"dn",27},
    {"oseas",28},{"ose",28},{"os",28},
    {"joel",29},{"jl",29},
    {"amos",30},{"am",30},
    {"abdias",31},{"abd",31},{"ab",31},
    {"jonas",32},{"jon",32},
    {"miqueas",33},{"miq",33},{"mi",33},
    {"nahum",34},{"nah",34},
    {"habacuc",35},{"hab",35},
    {"sofonias",36},{"sof",36},
    {"hageo",37},{"hag",37},
    {"zacarias",38},{"zac",38},
    {"malaquias",39},{"mal",39},
    {"mateo",40},{"mat",40},{"mt",40},
    {"marcos",41},{"mar",41},{"mc",41},{"mr",41},
    {"lucas",42},{"luc",42},{"lc",42},
    {"juan",43},{"jn",43},{"jua",43},
    {"hechos",44},{"hch",44},{"hec",44},{"act",44},
    {"romanos",45},{"rom",45},{"ro",45},
    {"1corintios",46},{"1cor",46},{"1co",46},
    {"2corintios",47},{"2cor",47},{"2co",47},
    {"galatas",48},{"gal",48},{"ga",48},
    {"efesios",49},{"efe",49},{"ef",49},
    {"filipenses",50},{"fil",50},{"php",50},
    {"colosenses",51},{"col",51},
    {"1tesalonicenses",52},{"1tes",52},{"1ts",52},
    {"2tesalonicenses",53},{"2tes",53},{"2ts",53},
    {"1timoteo",54},{"1tim",54},{"1ti",54},
    {"2timoteo",55},{"2tim",55},{"2ti",55},
    {"tito",56},{"tit",56},
    {"filemon",57},{"flm",57},{"fm",57},
    {"hebreos",58},{"heb",58},
    {"santiago",59},{"sant",59},{"stg",59},{"sg",59},
    {"1pedro",60},{"1ped",60},{"1pe",60},
    {"2pedro",61},{"2ped",61},{"2pe",61},
    {"1juan",62},{"1jn",62},
    {"2juan",63},{"2jn",63},
    {"3juan",64},{"3jn",64},
    {"judas",65},{"jud",65},
    {"apocalipsis",66},{"apo",66},{"ap",66},{"rev",66},
    {nullptr,0}
};

static int ResolveAbbrev(const std::string& norm) {
    for (int k = 0; kAbbrevTable[k].first != nullptr; k++)
        if (norm == kAbbrevTable[k].first) return kAbbrevTable[k].second;
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Secciones y colores
// ─────────────────────────────────────────────────────────────────────────────

BibleSection BibleView::GetBookSection(int n) const {
    if (n >= 1  && n <= 5)  return BibleSection::Pentateuch;
    if (n >= 6  && n <= 17) return BibleSection::HistoricalOT;
    if (n >= 18 && n <= 22) return BibleSection::Wisdom;
    if (n >= 23 && n <= 27) return BibleSection::MajorProphets;
    if (n >= 28 && n <= 39) return BibleSection::MinorProphets;
    if (n >= 40 && n <= 43) return BibleSection::Gospels;
    if (n == 44)             return BibleSection::Acts;
    if (n >= 45 && n <= 57) return BibleSection::PaulineEpistles;
    if (n >= 58 && n <= 65) return BibleSection::GeneralEpistles;
    return BibleSection::Apocalypse;
}

void BibleView::GetSectionColor(BibleSection section, float& r, float& g, float& b) const {
    switch (section) {
        case BibleSection::Pentateuch:      r=0.95f; g=0.75f; b=0.35f; break;
        case BibleSection::HistoricalOT:    r=0.55f; g=0.80f; b=0.45f; break;
        case BibleSection::Wisdom:          r=0.95f; g=0.85f; b=0.30f; break;
        case BibleSection::MajorProphets:   r=0.75f; g=0.50f; b=0.95f; break;
        case BibleSection::MinorProphets:   r=0.50f; g=0.65f; b=0.95f; break;
        case BibleSection::Gospels:         r=0.30f; g=0.80f; b=0.85f; break;
        case BibleSection::Acts:            r=0.45f; g=0.88f; b=0.60f; break;
        case BibleSection::PaulineEpistles: r=0.95f; g=0.58f; b=0.35f; break;
        case BibleSection::GeneralEpistles: r=0.90f; g=0.50f; b=0.70f; break;
        case BibleSection::Apocalypse:      r=0.95f; g=0.35f; b=0.35f; break;
        default:                            r=0.70f; g=0.70f; b=0.70f; break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helper: build the projected text string
//  Format: "BookName Cap:Verse (BibleName)\nverse text"
// ─────────────────────────────────────────────────────────────────────────────

static std::string BuildProjectedText(const BookData& book,
                                       const ChapterData& chap,
                                       const VerseData& verse,
                                       const std::string& bibleName) {
    std::string ref = book.name
                    + " " + std::to_string(chap.number)
                    + ":" + std::to_string(verse.number)
                    + " (" + bibleName + ")";
    return ref + "\n" + verse.text;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Carga XML
// ─────────────────────────────────────────────────────────────────────────────

void BibleView::LoadXMLBible(const std::string& path) {
    m_CurrentBible    = BibleData();
    m_BibleLoaded     = false;
    m_LoadedBiblePath = path;

    const char* bookNames[] = {
        "Genesis","Exodo","Levitico","Numeros","Deuteronomio",
        "Josue","Jueces","Rut","1 Samuel","2 Samuel","1 Reyes","2 Reyes",
        "1 Cronicas","2 Cronicas","Esdras","Nehemias","Ester","Job","Salmos",
        "Proverbios","Eclesiastes","Cantares","Isaias","Jeremias","Lamentaciones",
        "Ezequiel","Daniel","Oseas","Joel","Amos","Abdias","Jonas","Miqueas",
        "Nahum","Habacuc","Sofonias","Hageo","Zacarias","Malaquias",
        "Mateo","Marcos","Lucas","Juan","Hechos","Romanos","1 Corintios",
        "2 Corintios","Galatas","Efesios","Filipenses","Colosenses","1 Tesalonicenses",
        "2 Tesalonicenses","1 Timoteo","2 Timoteo","Tito","Filemon","Hebreos",
        "Santiago","1 Pedro","2 Pedro","1 Juan","2 Juan","3 Juan","Judas","Apocalipsis"
    };

    std::ifstream file(path);
    if (!file.is_open()) return;

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string xml = buffer.str();

    size_t bookPos = 0;
    while ((bookPos = xml.find("<book", bookPos)) != std::string::npos) {
        BookData book;
        int bookNum = 0;
        size_t numStart = xml.find("number=\"", bookPos);
        if (numStart != std::string::npos) {
            numStart += 8;
            size_t numEnd = xml.find("\"", numStart);
            try { bookNum = std::stoi(xml.substr(numStart, numEnd - numStart)); } catch(...) {}
        }
        book.canonicalNumber = bookNum;
        book.name = (bookNum >= 1 && bookNum <= 66)
            ? bookNames[bookNum - 1]
            : "Libro " + std::to_string(bookNum);

        size_t nextBookPos = xml.find("<book", bookPos + 5);
        if (nextBookPos == std::string::npos) nextBookPos = xml.length();

        size_t chapPos = bookPos;
        while ((chapPos = xml.find("<chapter", chapPos)) != std::string::npos
               && chapPos < nextBookPos) {
            ChapterData chapter;
            size_t cnumStart = xml.find("number=\"", chapPos);
            if (cnumStart != std::string::npos && cnumStart < nextBookPos) {
                cnumStart += 8;
                size_t cnumEnd = xml.find("\"", cnumStart);
                try { chapter.number = std::stoi(xml.substr(cnumStart, cnumEnd - cnumStart)); } catch(...) {}
            }
            size_t nextChapPos = xml.find("<chapter", chapPos + 8);
            if (nextChapPos == std::string::npos) nextChapPos = nextBookPos;

            size_t versPos = chapPos;
            while ((versPos = xml.find("<verse", versPos)) != std::string::npos
                   && versPos < nextChapPos) {
                VerseData verse;
                size_t vnumStart = xml.find("number=\"", versPos);
                if (vnumStart != std::string::npos && vnumStart < nextChapPos) {
                    vnumStart += 8;
                    size_t vnumEnd = xml.find("\"", vnumStart);
                    try { verse.number = std::stoi(xml.substr(vnumStart, vnumEnd - vnumStart)); } catch(...) {}
                }
                size_t textStart = xml.find(">", versPos) + 1;
                size_t textEnd   = xml.find("</verse>", textStart);
                if (textStart != std::string::npos && textEnd != std::string::npos
                    && textStart < textEnd)
                    verse.text = xml.substr(textStart, textEnd - textStart);
                chapter.verses.push_back(verse);
                versPos = textEnd;
            }
            if (!chapter.verses.empty()) book.chapters.push_back(chapter);
            chapPos = nextChapPos;
        }
        if (!book.chapters.empty()) m_CurrentBible.books.push_back(book);
        bookPos = nextBookPos;
    }

    m_CurrentBible.name = fs::path(path).stem().string();
    m_BibleLoaded       = !m_CurrentBible.books.empty();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Guardado de versiculo editado en XML
// ─────────────────────────────────────────────────────────────────────────────

void BibleView::SaveVerseToXML(int bookIdx, int chapIdx, int verseIdx) {
    if (m_LoadedBiblePath.empty()) { m_EditStatus = "Error: ruta desconocida"; return; }
    if (bookIdx  < 0 || bookIdx  >= (int)m_CurrentBible.books.size())
        { m_EditStatus = "Error: libro invalido"; return; }
    auto& book = m_CurrentBible.books[bookIdx];
    if (chapIdx  < 0 || chapIdx  >= (int)book.chapters.size())
        { m_EditStatus = "Error: capitulo invalido"; return; }
    auto& chap = book.chapters[chapIdx];
    if (verseIdx < 0 || verseIdx >= (int)chap.verses.size())
        { m_EditStatus = "Error: versiculo invalido"; return; }

    // FIXED: Apply the edited buffer text to the in-memory model FIRST
    chap.verses[verseIdx].text   = std::string(m_EditBuffer);
    chap.verses[verseIdx].edited = true;

    // Rewrite the entire XML
    std::ofstream out(m_LoadedBiblePath, std::ios::trunc);
    if (!out.is_open()) { m_EditStatus = "Error: no se pudo abrir el archivo"; return; }

    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<bible>\n";
    for (auto& b : m_CurrentBible.books) {
        out << "  <book number=\"" << b.canonicalNumber << "\" name=\"" << b.name << "\">\n";
        for (auto& c : b.chapters) {
            out << "    <chapter number=\"" << c.number << "\">\n";
            for (auto& v : c.verses) {
                out << "      <verse number=\"" << v.number << "\">"
                    << v.text
                    << "</verse>\n";
            }
            out << "    </chapter>\n";
        }
        out << "  </book>\n";
    }
    out << "</bible>\n";
    out.close();

    m_EditStatus = "Guardado correctamente";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Proyeccion
// ─────────────────────────────────────────────────────────────────────────────

void BibleView::ProjectVerse(int bookIdx, int chapIdx, int verseIdx) {
    if (bookIdx < 0 || bookIdx >= (int)m_CurrentBible.books.size()) return;
    auto& book = m_CurrentBible.books[bookIdx];
    if (chapIdx < 0 || chapIdx >= (int)book.chapters.size()) return;
    auto& chap = book.chapters[chapIdx];
    if (verseIdx < 0 || verseIdx >= (int)chap.verses.size()) return;
    auto& verse = chap.verses[verseIdx];

    // FIXED: Build projected text via shared helper so it's always consistent
    std::string fullText = BuildProjectedText(book, chap, verse, m_CurrentBible.name);

    m_ProjectedBookNum  = book.canonicalNumber;
    m_ProjectedChapNum  = chap.number;
    m_ProjectedVerseNum = verse.number;
    m_ProjectedBookIdx  = bookIdx;
    m_ProjectedChapIdx  = chapIdx;
    m_ProjectedVerseIdx = verseIdx;
    m_ScrollToVerse     = verseIdx;

    // Add to history (no duplicate at tail)
    bool isDuplicate = !m_History.empty()
        && m_History.back().bookIdx  == bookIdx
        && m_History.back().chapIdx  == chapIdx
        && m_History.back().verseIdx == verseIdx;

    if (!isDuplicate) {
        HistoryEntry entry;
        entry.ref      = book.name + " " + std::to_string(chap.number)
                       + ":" + std::to_string(verse.number);
        entry.fullText = fullText;
        entry.bookIdx  = bookIdx;
        entry.chapIdx  = chapIdx;
        entry.verseIdx = verseIdx;
        m_History.push_back(entry);
        if (m_History.size() > 50)
            m_History.erase(m_History.begin());
    } else {
        // FIXED: Even if it's a duplicate position, update fullText in case
        // the verse was just edited and re-projected with new content.
        m_History.back().fullText = fullText;
    }

    auto& core = Core::PresentationCore::Get();
    core.SetLayer2_Text(fullText);
    core.SetProjecting(true);
}

void BibleView::ReprojectInCurrentBible() {
    if (!m_BibleLoaded || m_ProjectedBookNum < 0) return;
    for (int bi = 0; bi < (int)m_CurrentBible.books.size(); bi++) {
        if (m_CurrentBible.books[bi].canonicalNumber != m_ProjectedBookNum) continue;
        auto& book = m_CurrentBible.books[bi];
        for (int ci = 0; ci < (int)book.chapters.size(); ci++) {
            if (book.chapters[ci].number != m_ProjectedChapNum) continue;
            auto& chap = book.chapters[ci];
            for (int vi = 0; vi < (int)chap.verses.size(); vi++) {
                if (chap.verses[vi].number == m_ProjectedVerseNum)
                    { ProjectVerse(bi, ci, vi); return; }
            }
            if (!chap.verses.empty()) ProjectVerse(bi, ci, (int)chap.verses.size() - 1);
            return;
        }
        break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Busqueda inteligente
// ─────────────────────────────────────────────────────────────────────────────

bool BibleView::ParseSmartQuery(const std::string& rawQuery,
                                 int& outBook, int& outChap, int& outVerse) {
    outBook = outChap = outVerse = -1;
    if (!m_BibleLoaded || rawQuery.empty()) return false;

    std::string q = StripAccents(ToLowerUTF8(rawQuery));
    int chapNum = -1, verseNum = -1;

    size_t colonPos = q.rfind(':');
    if (colonPos != std::string::npos) {
        std::string afterColon = q.substr(colonPos + 1);
        afterColon.erase(std::remove_if(afterColon.begin(), afterColon.end(),
            [](char c){ return !std::isdigit((unsigned char)c); }), afterColon.end());
        if (!afterColon.empty()) try { verseNum = std::stoi(afterColon); } catch(...) {}

        std::string beforeColon = q.substr(0, colonPos);
        size_t lastSp = beforeColon.rfind(' ');
        std::string chapStr = (lastSp != std::string::npos) ? beforeColon.substr(lastSp + 1) : "";
        chapStr.erase(std::remove_if(chapStr.begin(), chapStr.end(),
            [](char c){ return !std::isdigit((unsigned char)c); }), chapStr.end());
        if (!chapStr.empty()) try { chapNum = std::stoi(chapStr); } catch(...) {}

        q = (lastSp != std::string::npos) ? beforeColon.substr(0, lastSp) : beforeColon;
        if (chapNum < 0)
            while (!q.empty() && std::isdigit((unsigned char)q.back())) q.pop_back();
    } else {
        size_t lastSp = q.rfind(' ');
        if (lastSp != std::string::npos) {
            std::string tail = q.substr(lastSp + 1);
            bool allDigits = !tail.empty();
            for (char c : tail) if (!std::isdigit((unsigned char)c)) { allDigits = false; break; }
            if (allDigits) {
                try { chapNum = std::stoi(tail); } catch(...) {}
                q = q.substr(0, lastSp);
            }
        }
    }

    while (!q.empty() && q.front() == ' ') q.erase(q.begin());
    while (!q.empty() && q.back()  == ' ') q.pop_back();

    int resolvedBookNum = -1;
    {
        std::string nosp = q;
        nosp.erase(std::remove(nosp.begin(), nosp.end(), ' '), nosp.end());
        resolvedBookNum = ResolveAbbrev(nosp);
    }
    if (resolvedBookNum <= 0) {
        int bestLen = -1;
        for (auto& b : m_CurrentBible.books) {
            std::string bNorm = Normalize(b.name);
            if (bNorm.find(q) != std::string::npos || q.find(bNorm) != std::string::npos) {
                if ((int)bNorm.size() > bestLen) {
                    bestLen = (int)bNorm.size();
                    resolvedBookNum = b.canonicalNumber;
                }
            }
        }
    }
    if (resolvedBookNum <= 0) return false;

    for (int bi = 0; bi < (int)m_CurrentBible.books.size(); bi++) {
        if (m_CurrentBible.books[bi].canonicalNumber != resolvedBookNum) continue;
        outBook = bi;
        auto& book = m_CurrentBible.books[bi];
        if (chapNum > 0) {
            for (int ci = 0; ci < (int)book.chapters.size(); ci++) {
                if (book.chapters[ci].number == chapNum) {
                    outChap = ci;
                    if (verseNum > 0) {
                        for (int vi = 0; vi < (int)book.chapters[ci].verses.size(); vi++) {
                            if (book.chapters[ci].verses[vi].number == verseNum)
                                { outVerse = vi; break; }
                        }
                        if (outVerse < 0) outVerse = 0;
                    }
                    break;
                }
            }
            if (outChap < 0) outChap = 0;
        } else {
            outChap = 0;
        }
        break;
    }
    return outBook >= 0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Navegacion
// ─────────────────────────────────────────────────────────────────────────────

void BibleView::NavigateVerse(int delta) {
    if (!m_BibleLoaded) return;
    if (m_SelectedBook < 0 || m_SelectedBook >= (int)m_CurrentBible.books.size()) return;
    auto& book     = m_CurrentBible.books[m_SelectedBook];
    int verseCount = (int)book.chapters[m_SelectedChapter].verses.size();
    int newVerse   = m_SelectedVerse + delta;
    if (newVerse >= verseCount) {
        if (m_SelectedChapter + 1 < (int)book.chapters.size()) {
            m_SelectedChapter++;
            m_SelectedVerse = 0;
        }
    } else if (newVerse < 0) {
        if (m_SelectedChapter - 1 >= 0) {
            m_SelectedChapter--;
            m_SelectedVerse = (int)book.chapters[m_SelectedChapter].verses.size() - 1;
        }
    } else {
        m_SelectedVerse = newVerse;
    }
    ProjectVerse(m_SelectedBook, m_SelectedChapter, m_SelectedVerse);
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderTopBar
// ─────────────────────────────────────────────────────────────────────────────

void BibleView::RenderTopBar() {
    const float barH    = 50.0f;
    const float frameH  = ImGui::GetFrameHeight();
    const float centerY = (barH - frameH) * 0.5f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.06f, 0.08f, 1.0f));
    ImGui::BeginChild("##BibleTopBar", ImVec2(0.0f, barH), false,
                      ImGuiWindowFlags_NoScrollbar);

    ImGui::SetCursorPos(ImVec2(10.0f, (barH - ImGui::GetTextLineHeight()) * 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.75f, 1.0f, 1.0f));
    ImGui::TextUnformatted(m_CurrentBible.name.empty() ? "Sin Biblia" : m_CurrentBible.name.c_str());
    ImGui::PopStyleColor();

    ImGui::SameLine(0.0f, 12.0f);
    ImGui::SetCursorPosY((barH - ImGui::GetTextLineHeight()) * 0.5f);
    ImGui::TextDisabled("|");
    ImGui::SameLine(0.0f, 10.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(10.0f, 6.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.10f, 0.12f, 0.16f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.14f, 0.16f, 0.22f, 1.0f));
    ImGui::SetNextItemWidth(260.0f);
    ImGui::SetCursorPosY(centerY);

    if (m_NeedsFocusSearch) { ImGui::SetKeyboardFocusHere(); m_NeedsFocusSearch = false; }

    bool searchChanged = ImGui::InputTextWithHint("##liveSearch",
        "Gn 1:1  Jn 3:16  salmo 23...", m_LiveSearch, sizeof(m_LiveSearch));
    m_SearchFocused = ImGui::IsItemActive();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    if (searchChanged) {
        int bk, ch, vs;
        if (ParseSmartQuery(std::string(m_LiveSearch), bk, ch, vs)) {
            m_FilteredBook    = bk;
            m_FilteredChapter = (ch >= 0) ? ch : 0;
            if (bk >= 0) {
                m_SelectedBook    = bk;
                m_SelectedChapter = (ch >= 0) ? ch : 0;
                if (vs >= 0) m_SelectedVerse = vs;
            }
        } else {
            m_FilteredBook = m_FilteredChapter = -1;
        }
    }

    if (m_SearchFocused && ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        if (m_SelectedBook >= 0) {
            ProjectVerse(m_SelectedBook, m_SelectedChapter,
                         m_SelectedVerse >= 0 ? m_SelectedVerse : 0);
            m_LiveSearch[0]   = '\0';
            m_FilteredBook    = -1;
            m_FilteredChapter = -1;
        }
    }

    // Chip del versiculo proyectado
    ImGui::SameLine(0.0f, 10.0f);
    ImGui::SetCursorPosY(centerY);
    if (m_ProjectedBookIdx >= 0
        && m_ProjectedBookIdx < (int)m_CurrentBible.books.size()) {
        auto& pb = m_CurrentBible.books[m_ProjectedBookIdx];
        if (m_ProjectedChapIdx < (int)pb.chapters.size()) {
            auto& pc = pb.chapters[m_ProjectedChapIdx];
            if (m_ProjectedVerseIdx < (int)pc.verses.size()) {
                float r, g, b;
                GetSectionColor(GetBookSection(pb.canonicalNumber), r, g, b);
                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(r*0.12f, g*0.12f, b*0.12f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(r*0.22f, g*0.22f, b*0.22f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(r, g, b, 1.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
                // Le damos unos espacios en blanco al inicio ("   ") para hacerle hueco al icono
                std::string projLabel = "   " + pb.name + " "
                    + std::to_string(pc.number) + ":"
                    + std::to_string(pc.verses[m_ProjectedVerseIdx].number);
                
                bool btnClicked = ImGui::Button(projLabel.c_str(), ImVec2(0.0f, 28.0f));
                
                // Dibujamos el icono manualMENTE por encima del botón que acabamos de crear
                ImVec2 btnMin = ImGui::GetItemRectMin();
                auto it = StyleGeneralApp::Icons.find("izquierda");
                if (it != StyleGeneralApp::Icons.end() && it->second.textureID) {
                    float iconSize = ImGui::GetFontSize() * 0.9f;
                    ImVec2 iconPos = ImVec2(btnMin.x + 8.0f, btnMin.y + (28.0f - iconSize) * 0.5f);
                    
                    ImGui::GetWindowDrawList()->AddImage(
                        it->second.textureID,
                        iconPos,
                        ImVec2(iconPos.x + iconSize, iconPos.y + iconSize),
                        ImVec2(0, 0), ImVec2(1, 1),
                        Col(r, g, b, 1.0f) // Teñido con el color del botón
                    );
                }

                if (btnClicked) {
                    m_SelectedBook    = m_ProjectedBookIdx;
                    m_SelectedChapter = m_ProjectedChapIdx;
                    m_SelectedVerse   = m_ProjectedVerseIdx;
                    m_ScrollToVerse   = m_ProjectedVerseIdx;
                }
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(3);
            }
        }
    }

    // Boton historial
    ImGui::SameLine(0.0f, 10.0f);
    ImGui::SetCursorPosY(centerY);

    bool hasHistory = !m_History.empty();
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,
        m_ShowHistory
            ? ImVec4(0.20f, 0.30f, 0.45f, 1.0f)
            : ImVec4(0.12f, 0.14f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.26f, 0.38f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,
        hasHistory ? ImVec4(0.55f, 0.85f, 1.0f, 1.0f) : ImVec4(0.40f, 0.42f, 0.48f, 1.0f));

    std::string histLabel = "  Historial (" + std::to_string(m_History.size()) + ")  ";
    if (ImGui::Button(histLabel.c_str(), ImVec2(0.0f, 28.0f)) && hasHistory)
        m_ShowHistory = !m_ShowHistory;

    m_HistoryBtnPos  = ImGui::GetItemRectMin();
    m_HistoryBtnSize = ImGui::GetItemRectSize();

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    ImGui::EndChild();
    ImGui::PopStyleColor(); // ChildBg

    if (m_ShowHistory)
        RenderHistoryPopup();
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderHistoryPopup
//  FIXED: Pop style vars/colors BEFORE ImGui::End(), not after.
// ─────────────────────────────────────────────────────────────────────────────

void BibleView::RenderHistoryPopup() {
    if (!m_ShowHistory || m_History.empty()) return;

    ImVec2 winPos = ImVec2(m_HistoryBtnPos.x,
                           m_HistoryBtnPos.y + m_HistoryBtnSize.y + 4.0f);

    ImGui::SetNextWindowPos(winPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(320.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(260.0f, 60.0f), ImVec2(400.0f, 420.0f));
    ImGui::SetNextWindowBgAlpha(0.97f);

    // Push style BEFORE Begin
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07f, 0.09f, 0.12f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.20f, 0.30f, 0.45f, 0.70f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(6.0f, 6.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,    ImVec2(6.0f, 3.0f));

    constexpr ImGuiWindowFlags kHistFlags =
        ImGuiWindowFlags_NoTitleBar        |
        ImGuiWindowFlags_NoResize          |
        ImGuiWindowFlags_NoMove            |
        ImGuiWindowFlags_NoSavedSettings   |
        ImGuiWindowFlags_NoFocusOnAppearing|
        ImGuiWindowFlags_NoNav;

    bool windowOpen = true;
    bool earlyExit  = false;

    if (ImGui::Begin("##BibleHistoryWin", &windowOpen, kHistFlags)) {

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.65f, 0.90f, 1.0f));
        ImGui::TextUnformatted("  Historial de versiculos");
        ImGui::PopStyleColor();

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 50.0f);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.25f, 0.10f, 0.10f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.40f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.90f, 0.40f, 0.40f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        if (ImGui::SmallButton("Limpiar")) {
            m_History.clear();
            m_ShowHistory = false;
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        ImGui::Separator();

        for (int i = (int)m_History.size() - 1; i >= 0; i--) {
            auto& entry = m_History[i];
            ImGui::PushID(i);

            bool isActive = (entry.bookIdx  == m_ProjectedBookIdx
                          && entry.chapIdx  == m_ProjectedChapIdx
                          && entry.verseIdx == m_ProjectedVerseIdx);

            ImGui::PushStyleColor(ImGuiCol_Header,
                ImVec4(0.15f, 0.25f, 0.40f, isActive ? 1.0f : 0.0f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                ImVec4(0.18f, 0.28f, 0.45f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,
                isActive
                    ? ImVec4(0.55f, 0.85f, 1.0f, 1.0f)
                    : ImVec4(0.85f, 0.87f, 0.90f, 1.0f));

            bool selected = ImGui::Selectable(entry.ref.c_str(), isActive,
                                              ImGuiSelectableFlags_None, ImVec2(0.0f, 0.0f));
            ImGui::PopStyleColor(3);
            ImGui::PopID();

            if (selected) {
                m_SelectedBook      = entry.bookIdx;
                m_SelectedChapter   = entry.chapIdx;
                m_SelectedVerse     = entry.verseIdx;
                m_ScrollToVerse     = entry.verseIdx;
                m_ProjectedBookIdx  = entry.bookIdx;
                m_ProjectedChapIdx  = entry.chapIdx;
                m_ProjectedVerseIdx = entry.verseIdx;
                Core::PresentationCore::Get().SetLayer2_Text(entry.fullText);
                Core::PresentationCore::Get().SetProjecting(true);
                m_ShowHistory = false;
                earlyExit = true;
                break; // FIXED: break loop, then End() normally below
            }
        }

        // Close if click outside
        if (!earlyExit
            && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
            && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_ShowHistory = false;
        }
    }

    // FIXED: Always call End() before PopStyleVar/Color
    ImGui::End();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);

    if (!windowOpen) m_ShowHistory = false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderBookGrid
// ─────────────────────────────────────────────────────────────────────────────

void BibleView::RenderBookGrid() {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.38f, 0.45f, 1.0f));
    ImGui::TextUnformatted("  LIBROS");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::BeginChild("##BookGrid",
        ImVec2(0.0f, ImGui::GetContentRegionAvail().y * 0.60f), false);

    const float cellW = 58.0f, cellH = 34.0f, spacing = 3.0f;
    int cols = std::max(1, (int)((ImGui::GetContentRegionAvail().x + spacing) / (cellW + spacing)));

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(spacing, spacing));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

    for (int i = 0; i < (int)m_CurrentBible.books.size(); i++) {
        auto& b = m_CurrentBible.books[i];
        float r, g, bv;
        GetSectionColor(GetBookSection(b.canonicalNumber), r, g, bv);

        bool selected = (m_SelectedBook == i);
        bool filtered = (m_FilteredBook < 0) || (i == m_FilteredBook);
        float alpha   = filtered ? 1.0f : 0.18f;

        ImGui::PushStyleColor(ImGuiCol_Button,
            selected ? ImVec4(r*0.38f, g*0.38f, bv*0.38f, 1.0f)
                     : ImVec4(r*0.08f, g*0.08f, bv*0.08f, alpha));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            ImVec4(r*0.25f, g*0.25f, bv*0.25f, alpha));
        ImGui::PushStyleColor(ImGuiCol_Text,
            selected ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
                     : ImVec4(r, g, bv, alpha));

        ImGui::PushID(i);
        if (ImGui::Button(b.name.substr(0, 4).c_str(), ImVec2(cellW, cellH))) {
            m_SelectedBook    = i;
            m_SelectedChapter = 0;
            m_SelectedVerse   = 0;
            m_LiveSearch[0]   = '\0';
            m_FilteredBook    = -1;
            m_FilteredChapter = -1;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", b.name.c_str());
        ImGui::PopID();
        ImGui::PopStyleColor(3);

        if ((i + 1) % cols != 0) ImGui::SameLine();
    }

    ImGui::PopStyleVar(2);
    ImGui::EndChild();
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderChapterGrid
// ─────────────────────────────────────────────────────────────────────────────

void BibleView::RenderChapterGrid() {
    if (m_SelectedBook < 0 || m_SelectedBook >= (int)m_CurrentBible.books.size()) return;
    auto& book = m_CurrentBible.books[m_SelectedBook];

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.38f, 0.45f, 1.0f));
    ImGui::Text("  CAPITULOS  —  %s", book.name.c_str());
    ImGui::PopStyleColor();

    ImGui::BeginChild("##ChapterGrid", ImVec2(0.0f, 0.0f), false);

    const float cellW = 36.0f, cellH = 28.0f, spacing = 3.0f;
    int cols = std::max(1, (int)((ImGui::GetContentRegionAvail().x + spacing) / (cellW + spacing)));

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(spacing, spacing));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

    float r, g, bv;
    GetSectionColor(GetBookSection(book.canonicalNumber), r, g, bv);

    for (int i = 0; i < (int)book.chapters.size(); i++) {
        bool selected = (m_SelectedChapter == i);
        ImGui::PushStyleColor(ImGuiCol_Button,
            selected ? ImVec4(r*0.35f, g*0.35f, bv*0.35f, 1.0f)
                     : ImVec4(0.10f, 0.11f, 0.14f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(r*0.20f, g*0.20f, bv*0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,
            selected ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ImVec4(0.65f, 0.68f, 0.74f, 1.0f));

        ImGui::PushID(1000 + i);
        if (ImGui::Button(std::to_string(book.chapters[i].number).c_str(), ImVec2(cellW, cellH))) {
            m_SelectedChapter = i;
            m_SelectedVerse   = 0;
        }
        ImGui::PopID();
        ImGui::PopStyleColor(3);

        if ((i + 1) % cols != 0) ImGui::SameLine();
    }

    ImGui::PopStyleVar(2);
    ImGui::EndChild();
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderVerseList
// ─────────────────────────────────────────────────────────────────────────────

void BibleView::RenderVerseList() {
    if (m_SelectedBook    < 0 || m_SelectedBook    >= (int)m_CurrentBible.books.size()) return;
    if (m_SelectedChapter < 0 || m_SelectedChapter >= (int)m_CurrentBible.books[m_SelectedBook].chapters.size()) return;

    auto& book = m_CurrentBible.books[m_SelectedBook];
    auto& chap = book.chapters[m_SelectedChapter];

    float r, g, bv;
    GetSectionColor(GetBookSection(book.canonicalNumber), r, g, bv);

    const float marginH     = 28.0f; // Aumentado de 14 a 28 para dar espacio al icono
    const float numColW     = 32.0f;
    const float gap         = 8.0f;
    const float rowPadV     = 7.0f;
    const float availW      = ImGui::GetContentRegionAvail().x;
    const float verseTextW  = availW - marginH * 2.0f - numColW - gap - 28.0f;
    const float textAbsX    = ImGui::GetCursorScreenPos().x + marginH + numColW + gap;

    ImGui::SetCursorPosX(marginH);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(r, g, bv, 1.0f));
    ImGui::Text("%s  %d", book.name.c_str(), chap.number);
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (int i = 0; i < (int)chap.verses.size(); i++) {
        auto& verse = chap.verses[i];
        ImGui::PushID(i);

        ImVec2 textSz = ImGui::CalcTextSize(verse.text.c_str(), nullptr, false, verseTextW);
        float  rowH   = textSz.y + rowPadV * 2.0f;
        ImVec2 rowMin = ImGui::GetCursorScreenPos();
        ImVec2 rowMax = ImVec2(rowMin.x + availW, rowMin.y + rowH);

        if (m_ScrollToVerse == i) { ImGui::SetScrollHereY(0.3f); m_ScrollToVerse = -1; }

        ImGui::InvisibleButton("##vRow", ImVec2(availW, rowH));
        bool clicked   = ImGui::IsItemClicked();
        bool dblClick  = ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemHovered();
        bool hovered   = ImGui::IsItemHovered();

        bool isProjected = (i == m_ProjectedVerseIdx
                         && m_SelectedBook    == m_ProjectedBookIdx
                         && m_SelectedChapter == m_ProjectedChapIdx);
        bool isSelected  = (i == m_SelectedVerse);

        if (isProjected && isSelected)
            dl->AddRectFilled(rowMin, rowMax, ImGui::ColorConvertFloat4ToU32(ImVec4(r*0.35f, g*0.35f, bv*0.35f, 0.65f)), 4.0f);
        else if (isProjected)
            dl->AddRectFilled(rowMin, rowMax, ImGui::ColorConvertFloat4ToU32(ImVec4(r*0.20f, g*0.20f, bv*0.20f, 0.50f)), 4.0f);
        else if (isSelected)
            dl->AddRectFilled(rowMin, rowMax, Col(0.22f, 0.32f, 0.48f, 0.40f), 4.0f);
        else if (hovered)
            dl->AddRectFilled(rowMin, rowMax, Col(1.0f, 1.0f, 1.0f, 0.04f), 4.0f);

        if (isProjected) {
            auto it = StyleGeneralApp::Icons.find("izquierda");
            if (it != StyleGeneralApp::Icons.end() && it->second.textureID) {
                // Ajustamos el tamaño relativo a la fuente actual
                float iconSize = ImGui::GetFontSize() * 0.85f; 
                ImVec2 iconPos = ImVec2(rowMin.x + 4.0f, rowMin.y + rowPadV + 2.0f);
                
                // AddImage permite pasar un color al final que "tiñe" la textura
                dl->AddImage(
                    it->second.textureID,
                    iconPos,
                    ImVec2(iconPos.x + iconSize, iconPos.y + iconSize),
                    ImVec2(0, 0), ImVec2(1, 1),
                    Col(r, g, bv, 0.95f) // Usamos el color de la sección bíblica!
                );
            }
        }

        if (verse.edited)
            dl->AddCircleFilled(ImVec2(rowMin.x + marginH - 4.0f, rowMin.y + rowPadV + 4.0f),
                3.0f, Col(0.95f, 0.72f, 0.20f, 0.90f));

        dl->AddText(ImVec2(rowMin.x + marginH, rowMin.y + rowPadV),
            Col(r, g, bv, isProjected ? 1.0f : 0.45f),
            std::to_string(verse.number).c_str());

        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
            ImVec2(textAbsX, rowMin.y + rowPadV),
            Col(0.90f, 0.90f, 0.92f, (isSelected || isProjected) ? 1.0f : 0.86f),
            verse.text.c_str(), nullptr, verseTextW);

        if (hovered || isSelected) {
            float btnX = rowMax.x - 26.0f;
            float btnY = rowMin.y + (rowH - 20.0f) * 0.5f;
            ImGui::SetCursorScreenPos(ImVec2(btnX, btnY));
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.15f, 0.20f, 0.30f, 0.90f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.35f, 0.55f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.60f, 0.80f, 1.00f, 1.00f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(3.0f, 2.0f));
            
            // Creamos el botón vacío sin el emoji del lápiz
            bool btnEditClicked = ImGui::Button("  ##edit", ImVec2(20.0f, 20.0f));
            
            // Dibujamos el icono "editar" manualmente encima
            ImVec2 btnMin = ImGui::GetItemRectMin();
            auto itEdit = StyleGeneralApp::Icons.find("editar");
            if (itEdit != StyleGeneralApp::Icons.end() && itEdit->second.textureID) {
                float iconSize = 14.0f; // Tamaño del icono dentro del botón
                ImVec2 iconPos = ImVec2(btnMin.x + (20.0f - iconSize) * 0.5f, btnMin.y + (20.0f - iconSize) * 0.5f);
                
                ImGui::GetWindowDrawList()->AddImage(
                    itEdit->second.textureID,
                    iconPos, ImVec2(iconPos.x + iconSize, iconPos.y + iconSize),
                    ImVec2(0, 0), ImVec2(1, 1),
                    Col(0.70f, 0.85f, 1.0f, 1.0f) // Le damos un tinte celeste suave
                );
            } else {
                // Si olvidaste poner la imagen, dibuja una 'E' para que no quede invisible ni salga el '?'
                ImGui::GetWindowDrawList()->AddText(
                    ImVec2(btnMin.x + 6.0f, btnMin.y + 2.0f),
                    Col(1.0f, 1.0f, 1.0f, 1.0f), "E"
                );
            }

            // Lógica original de cuando se hace clic
            if (btnEditClicked) {
                m_EditBookIdx  = m_SelectedBook;
                m_EditChapIdx  = m_SelectedChapter;
                m_EditVerseIdx = i;
                size_t len = std::min(verse.text.size(), sizeof(m_EditBuffer) - 1);
                memcpy(m_EditBuffer, verse.text.c_str(), len);
                m_EditBuffer[len] = '\0';
                m_EditStatus.clear();
                m_ShowEditModal = true;
            }
            
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(3);
        }

        dl->AddLine(ImVec2(rowMin.x + marginH, rowMax.y),
                    ImVec2(rowMax.x - marginH,  rowMax.y),
                    Col(1.0f, 1.0f, 1.0f, 0.04f));

        if (clicked)  { m_SelectedVerse = i; ProjectVerse(m_SelectedBook, m_SelectedChapter, i); }
        if (dblClick)   ProjectVerse(m_SelectedBook, m_SelectedChapter, i);

        ImGui::SetCursorScreenPos(ImVec2(rowMin.x, rowMax.y));
        ImGui::PopID();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderEditModal
//  FIXED: after saving, re-project the verse so the projector shows the new text.
//         Status message is cleared when the modal is closed (Cancelar / X).
// ─────────────────────────────────────────────────────────────────────────────

void BibleView::RenderEditModal() {
    if (!m_ShowEditModal) return;

    ImGui::SetNextWindowSize(ImVec2(520.0f, 280.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.07f, 0.10f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.22f, 0.32f, 0.50f, 0.80f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(16.0f, 14.0f));

    bool open = true;
    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoCollapse      |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar;

    if (ImGui::Begin("Editar versiculo##editModal", &open, kFlags)) {

        if (m_EditBookIdx  >= 0 && m_EditBookIdx  < (int)m_CurrentBible.books.size()
         && m_EditChapIdx  >= 0 && m_EditChapIdx  < (int)m_CurrentBible.books[m_EditBookIdx].chapters.size()
         && m_EditVerseIdx >= 0 && m_EditVerseIdx < (int)m_CurrentBible.books[m_EditBookIdx].chapters[m_EditChapIdx].verses.size()) {

            auto& b = m_CurrentBible.books[m_EditBookIdx];
            auto& c = b.chapters[m_EditChapIdx];
            auto& v = c.verses[m_EditVerseIdx];

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.70f, 1.0f, 1.0f));
            ImGui::Text("%s %d:%d", b.name.c_str(), c.number, v.number);
            ImGui::PopStyleColor();
        }

        ImGui::Separator();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.09f, 0.11f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.11f, 0.13f, 0.18f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextMultiline("##editVerse", m_EditBuffer, sizeof(m_EditBuffer),
                                  ImVec2(-1.0f, 120.0f));
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);

        ImGui::Spacing();

        if (!m_EditStatus.empty()) {
            bool ok = (m_EditStatus.find("Error") == std::string::npos);
            ImGui::PushStyleColor(ImGuiCol_Text,
                ok ? ImVec4(0.20f, 0.85f, 0.45f, 1.0f)
                   : ImVec4(0.90f, 0.35f, 0.35f, 1.0f));
            ImGui::TextUnformatted(m_EditStatus.c_str());
            ImGui::PopStyleColor();
            ImGui::SameLine();
        }

        float btnW = 110.0f;
        ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - btnW * 2.0f - 8.0f);

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f, 0.15f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.22f, 0.30f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        if (ImGui::Button("Cancelar", ImVec2(btnW, 32.0f))) {
            m_ShowEditModal = false;
            m_EditStatus.clear();
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine(0.0f, 8.0f);

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f, 0.30f, 0.55f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.40f, 0.70f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        if (ImGui::Button("Guardar", ImVec2(btnW, 32.0f))) {
            SaveVerseToXML(m_EditBookIdx, m_EditChapIdx, m_EditVerseIdx);
            // FIXED: Always re-project after saving so the projector reflects
            // the new text immediately, whether or not this was the active verse.
            if (m_EditBookIdx  == m_ProjectedBookIdx
             && m_EditChapIdx  == m_ProjectedChapIdx
             && m_EditVerseIdx == m_ProjectedVerseIdx) {
                ProjectVerse(m_ProjectedBookIdx, m_ProjectedChapIdx, m_ProjectedVerseIdx);
            }
        }
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
    }
    ImGui::End();

    // FIXED: Pop window styles AFTER End()
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    if (!open) {
        m_ShowEditModal = false;
        m_EditStatus.clear();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Render principal
// ─────────────────────────────────────────────────────────────────────────────

void BibleView::Render() {
    auto selection = Core::PresentationCore::Get().PeekSelection();

    if (selection.title != m_LastSelectedFile) {
        int oldBookNum = 1, oldChapNum = 1, oldVerseNum = 1;
        if (!m_CurrentBible.books.empty() && m_SelectedBook >= 0
            && m_SelectedBook < (int)m_CurrentBible.books.size()) {
            oldBookNum = m_CurrentBible.books[m_SelectedBook].canonicalNumber;
            if (m_SelectedChapter < (int)m_CurrentBible.books[m_SelectedBook].chapters.size()) {
                oldChapNum = m_CurrentBible.books[m_SelectedBook].chapters[m_SelectedChapter].number;
                auto& vers = m_CurrentBible.books[m_SelectedBook].chapters[m_SelectedChapter].verses;
                if (m_SelectedVerse < (int)vers.size())
                    oldVerseNum = vers[m_SelectedVerse].number;
            }
        }

        m_LastSelectedFile = selection.title;
        if (!selection.title.empty())
            LoadXMLBible(BiblesPath() + selection.title);

        m_SelectedBook = 0; m_SelectedChapter = 0; m_SelectedVerse = 0;
        if (!m_CurrentBible.books.empty()) {
            for (int i = 0; i < (int)m_CurrentBible.books.size(); i++) {
                if (m_CurrentBible.books[i].canonicalNumber != oldBookNum) continue;
                m_SelectedBook = i;
                for (int j = 0; j < (int)m_CurrentBible.books[i].chapters.size(); j++) {
                    if (m_CurrentBible.books[i].chapters[j].number != oldChapNum) continue;
                    m_SelectedChapter = j;
                    for (int k = 0; k < (int)m_CurrentBible.books[i].chapters[j].verses.size(); k++) {
                        if (m_CurrentBible.books[i].chapters[j].verses[k].number == oldVerseNum)
                            { m_SelectedVerse = k; break; }
                    }
                    break;
                }
                break;
            }
            if (!m_CurrentBible.books.empty()) {
                auto& selBook = m_CurrentBible.books[m_SelectedBook];
                if (m_SelectedChapter >= (int)selBook.chapters.size()) m_SelectedChapter = 0;
                if (!selBook.chapters.empty()
                    && m_SelectedVerse >= (int)selBook.chapters[m_SelectedChapter].verses.size())
                    m_SelectedVerse = 0;
            }
        }
        ReprojectInCurrentBible();
    }

    if (!m_SearchFocused && m_BibleLoaded) {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow,  false)) NavigateVerse(-1);
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false)) NavigateVerse(+1);
    }

    RenderTopBar();

    if (ImGui::BeginTable("##BibleLayout", 2, ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Nav",    ImGuiTableColumnFlags_WidthFixed, 210.0f);
        ImGui::TableSetupColumn("Verses", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.06f, 0.08f, 1.0f));
        ImGui::BeginChild("##NavChild", ImVec2(0.0f, 0.0f), false);
        if (m_BibleLoaded) {
            RenderBookGrid();
            RenderChapterGrid();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.38f, 0.45f, 1.0f));
            ImGui::TextWrapped("Selecciona una Biblia en la biblioteca para comenzar.");
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::TableSetColumnIndex(1);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.04f, 0.05f, 0.07f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 6.0f));
        ImGui::BeginChild("##VersesChild", ImVec2(0.0f, 0.0f), false,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar);
        if (m_BibleLoaded)
            RenderVerseList();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        ImGui::EndTable();
    }

    RenderEditModal();
}

} // namespace ProyecThor::UI
