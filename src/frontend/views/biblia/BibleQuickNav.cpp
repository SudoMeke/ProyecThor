#include "BibleQuickNav.h"
#include "BibleTextUtils.h"
#include "BibleBookData.h"

#include <cctype>
#include <cmath>

namespace ProyecThor::UI {

void BibleQuickNav::Open() {
    m_Open = true;
    m_Buffer.clear();
    m_Resolution = QuickNavResolution();
}

void BibleQuickNav::Close() {
    m_Open = false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Resolve: separa el buffer tecleado en tres partes:
//    1) la parte "libro": letras, y ademas un digito inicial 1/2/3 si el
//       libro empieza con numero (ej: "1co", "2s").
//    2) la parte "capitulo": digitos que vienen justo despues de la parte
//       libro (ej: en "gn5" es "5").
//    3) la parte "versiculo": digitos que vienen despues de ':' (ej: en
//       "gn5:1" es "1").
//  Con la parte libro se buscan candidatos por prefijo (BibleBooks) y se
//  toma como mejor candidato el de menor numero canonico. Capitulo y
//  versiculo solo se resuelven si el libro ya quedo resuelto.
// ─────────────────────────────────────────────────────────────────────────────

void BibleQuickNav::Resolve(const BibleData& bible) {
    m_Resolution = QuickNavResolution();
    if (m_Buffer.empty() || bible.books.empty()) return;

    size_t i = 0;
    std::string bookPart;

    if (i < m_Buffer.size() && (m_Buffer[i] == '1' || m_Buffer[i] == '2' || m_Buffer[i] == '3')) {
        bookPart += m_Buffer[i];
        i++;
    }
    while (i < m_Buffer.size()
           && (std::isalpha((unsigned char)m_Buffer[i]) || m_Buffer[i] == ' ')) {
        bookPart += m_Buffer[i];
        i++;
    }

    std::string chapterPart;
    while (i < m_Buffer.size() && std::isdigit((unsigned char)m_Buffer[i])) {
        chapterPart += m_Buffer[i];
        i++;
    }

    std::string versePart;
    if (i < m_Buffer.size() && m_Buffer[i] == ':') {
        i++;
        while (i < m_Buffer.size() && std::isdigit((unsigned char)m_Buffer[i])) {
            versePart += m_Buffer[i];
            i++;
        }
    }

    if (bookPart.empty()) return;

    std::string normPrefix = TextUtils::Normalize(bookPart);
    if (normPrefix.empty()) return;

    std::vector<int> candidates = BibleBooks::FindBookCandidates(normPrefix);
    m_Resolution.bookCandidateCount = (int)candidates.size();
    if (candidates.empty()) return;

    int bestCanonical = candidates.front();
    for (int bi = 0; bi < (int)bible.books.size(); bi++) {
        if (bible.books[bi].canonicalNumber == bestCanonical) {
            m_Resolution.bookIdx = bi;
            m_Resolution.hasBook = true;
            break;
        }
    }
    if (!m_Resolution.hasBook) return;

    const BookData& book = bible.books[m_Resolution.bookIdx];

    if (!chapterPart.empty()) {
        int chapNum = 0;
        try { chapNum = std::stoi(chapterPart); } catch (...) { chapNum = 0; }
        if (chapNum > 0) {
            for (int ci = 0; ci < (int)book.chapters.size(); ci++) {
                if (book.chapters[ci].number == chapNum) {
                    m_Resolution.hasChapter        = true;
                    m_Resolution.chapterNumber     = chapNum;
                    m_Resolution.chapterIdx        = ci;
                    m_Resolution.chapterVerseCount = (int)book.chapters[ci].verses.size();
                    break;
                }
            }
        }
    }

    if (m_Resolution.hasChapter && !versePart.empty()) {
        int verseNum = 0;
        try { verseNum = std::stoi(versePart); } catch (...) { verseNum = 0; }
        if (verseNum > 0) {
            const ChapterData& chap = book.chapters[m_Resolution.chapterIdx];
            for (int vi = 0; vi < (int)chap.verses.size(); vi++) {
                if (chap.verses[vi].number == verseNum) {
                    m_Resolution.hasVerse    = true;
                    m_Resolution.verseNumber = verseNum;
                    m_Resolution.verseIdx    = vi;
                    break;
                }
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Update
// ─────────────────────────────────────────────────────────────────────────────

bool BibleQuickNav::Update(const BibleData& bible) {
    if (!m_Open) return false;

    ImGuiIO& io = ImGui::GetIO();
    bool changed = false;

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        Close();
        return false;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Backspace, true) && !m_Buffer.empty()) {
        m_Buffer.pop_back();
        changed = true;
    }

    for (int i = 0; i < io.InputQueueCharacters.Size; i++) {
        ImWchar wc = io.InputQueueCharacters[i];
        if (wc >= 32 && wc < 128) {
            char c = (char)wc;
            if (std::isalnum((unsigned char)c) || c == ' ' || c == ':') {
                m_Buffer += c;
                changed = true;
            }
        }
    }

    if (changed) Resolve(bible);

    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
        if (m_Resolution.hasBook) {
            const BookData& book = bible.books[m_Resolution.bookIdx];

            if (!m_Resolution.hasChapter && !book.chapters.empty()) {
                m_Resolution.hasChapter        = true;
                m_Resolution.chapterIdx        = 0;
                m_Resolution.chapterNumber     = book.chapters.front().number;
                m_Resolution.chapterVerseCount = (int)book.chapters.front().verses.size();
            }

            if (m_Resolution.hasChapter && !m_Resolution.hasVerse) {
                const ChapterData& chap = book.chapters[m_Resolution.chapterIdx];
                if (!chap.verses.empty()) {
                    m_Resolution.hasVerse    = true;
                    m_Resolution.verseIdx    = 0;
                    m_Resolution.verseNumber = chap.verses.front().number;
                }
            }

            Close();
            return true;
        }
    }

    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Render
// ─────────────────────────────────────────────────────────────────────────────

void BibleQuickNav::Render(const BibleData& bible) {
    if (!m_Open) return;

    ImGuiViewport* vp = ImGui::GetMainViewport();

    // Fondo semitransparente para enfocar la tarjeta central
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.55f));
    ImGui::Begin("##QuickNavDim", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoInputs);
    ImGui::End();
    ImGui::PopStyleColor();

    ImVec2 cardSize = ImVec2(420.0f, 260.0f);
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(cardSize, ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07f, 0.08f, 0.11f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.25f, 0.35f, 0.55f, 0.85f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(20.0f, 18.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);

    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoNav      | ImGuiWindowFlags_NoScrollbar;

    ImGui::Begin("##QuickNavCard", nullptr, kFlags);

    // "Esc para cancelar" arriba a la derecha
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.48f, 0.55f, 1.0f));
    const char* escHint = "Esc para cancelar";
    ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(escHint).x);
    ImGui::TextUnformatted(escHint);
    ImGui::PopStyleColor();

    // Buffer tecleado por el usuario, siempre visible
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.85f, 1.0f, 1.0f));
    std::string shown = m_Buffer.empty() ? "" : m_Buffer;
    ImGui::SetWindowFontScale(1.15f);
    if (!shown.empty())
        ImGui::TextUnformatted(shown.c_str());
    else
        ImGui::TextDisabled("Escribe: gn5:1, 1co13, salmos 23...");
    if (!shown.empty()) ImGui::SameLine(0.0f, 2.0f);
    if (std::fmod(ImGui::GetTime(), 1.0) < 0.5)
        ImGui::TextUnformatted("|");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    auto DrawField = [](const char* label, const std::string& value, bool resolved) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.43f, 0.50f, 1.0f));
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text,
            resolved ? ImVec4(0.92f, 0.94f, 0.98f, 1.0f) : ImVec4(0.45f, 0.48f, 0.55f, 1.0f));
        ImGui::SetWindowFontScale(1.4f);
        ImGui::TextUnformatted(value.c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
        ImGui::Spacing();
    };

    if (m_Resolution.hasBook) {
        std::string bookLabel = bible.books[m_Resolution.bookIdx].name;
        if (m_Resolution.bookCandidateCount > 1)
            bookLabel += "  (+" + std::to_string(m_Resolution.bookCandidateCount - 1)
                       + " mas, sigue escribiendo para afinar)";
        DrawField("Libro", bookLabel, true);
    } else {
        DrawField("Libro", "...", false);
    }

    DrawField("Capitulo",
        m_Resolution.hasChapter ? std::to_string(m_Resolution.chapterNumber) : "...",
        m_Resolution.hasChapter);

    DrawField("Versiculo",
        m_Resolution.hasVerse ? std::to_string(m_Resolution.verseNumber) : "...",
        m_Resolution.hasVerse);

    if (m_Resolution.hasChapter) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.43f, 0.50f, 1.0f));
        ImGui::Text("Versiculos: %d", m_Resolution.chapterVerseCount);
        ImGui::PopStyleColor();
    }

    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

} // namespace ProyecThor::UI