#include "BibleSearch.h"
#include "BibleTextUtils.h"
#include "BibleBookData.h"

#include <algorithm>
#include <cctype>

namespace ProyecThor::UI::Search {

bool ParseSmartQuery(const BibleData& bible, const std::string& rawQuery,
                      int& outBook, int& outChap, int& outVerse) {
    outBook = outChap = outVerse = -1;
    if (bible.books.empty() || rawQuery.empty()) return false;

    std::string q = TextUtils::StripAccents(TextUtils::ToLowerUTF8(rawQuery));
    int chapNum = -1, verseNum = -1;

    size_t colonPos = q.rfind(':');
    if (colonPos != std::string::npos) {
        std::string afterColon = q.substr(colonPos + 1);
        afterColon.erase(std::remove_if(afterColon.begin(), afterColon.end(),
            [](char c){ return !std::isdigit((unsigned char)c); }), afterColon.end());
        if (!afterColon.empty()) try { verseNum = std::stoi(afterColon); } catch (...) {}

        std::string beforeColon = q.substr(0, colonPos);
        size_t lastSp = beforeColon.rfind(' ');
        std::string chapStr = (lastSp != std::string::npos) ? beforeColon.substr(lastSp + 1) : "";
        chapStr.erase(std::remove_if(chapStr.begin(), chapStr.end(),
            [](char c){ return !std::isdigit((unsigned char)c); }), chapStr.end());
        if (!chapStr.empty()) try { chapNum = std::stoi(chapStr); } catch (...) {}

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
                try { chapNum = std::stoi(tail); } catch (...) {}
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

        // Primero se intenta una abreviatura exacta ("gn", "1co", etc).
        resolvedBookNum = BibleBooks::ResolveAbbrev(nosp);

        // Si no hay coincidencia exacta, se usa el buscador por prefijo
        // (mismo motor que usa el overlay de busqueda rapida) y se toma el
        // mejor candidato, es decir, el de menor numero canonico.
        if (resolvedBookNum <= 0) {
            std::vector<int> candidates = BibleBooks::FindBookCandidates(nosp);
            if (!candidates.empty()) resolvedBookNum = candidates.front();
        }
    }
    if (resolvedBookNum <= 0) return false;

    for (int bi = 0; bi < (int)bible.books.size(); bi++) {
        if (bible.books[bi].canonicalNumber != resolvedBookNum) continue;
        outBook = bi;
        const BookData& book = bible.books[bi];
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

} // namespace ProyecThor::UI::Search