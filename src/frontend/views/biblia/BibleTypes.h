#pragma once

#include <string>
#include <vector>

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

    // Atributos de la etiqueta <bible ...> del XML original — capturados al
    // cargar y re-escritos al guardar (ver XmlIO::SaveBible) para no perder
    // metadata en cada edicion de versiculo.
    std::string translation;
    std::string info;
    std::string link;
};

enum class BibleSection {
    Pentateuch, HistoricalOT, Wisdom, MajorProphets, MinorProphets,
    Gospels, Acts, PaulineEpistles, GeneralEpistles, Apocalypse
};

struct HistoryEntry {
    std::string ref;      // solo la referencia, ej. "Genesis 1:1"
    std::string fullText; // ref + "\n" + body -- legacy, ver comentario en BibleView::ProjectVerse
    std::string body;     // solo el texto del versiculo, sin la referencia
    int         bookIdx  = -1;
    int         chapIdx  = -1;
    int         verseIdx = -1;
};

} // namespace ProyecThor::UI