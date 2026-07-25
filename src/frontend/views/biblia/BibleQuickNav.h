#pragma once

#include <string>
#include <vector>
#include <imgui.h>
#include "BibleTypes.h"

namespace ProyecThor::UI {

// Resultado de la resolucion del buscador rapido. Se va completando a
// medida que el usuario confirma cada paso con Enter.
struct QuickNavResolution {
    bool hasBook            = false;
    int  bookCandidateCount = 0;   // cuantos libros calzaban con el prefijo tecleado
    int  bookIdx            = -1;  // indice dentro de bible.books

    bool hasChapter         = false;
    int  chapterNumber      = -1;
    int  chapterIdx         = -1;
    int  chapterVerseCount  = 0;   // cantidad de versiculos del capitulo resuelto

    bool hasVerse           = false;
    int  verseNumber        = -1;
    int  verseIdx           = -1;
};

// Los 3 pasos del asistente. Se avanza con Enter y se retrocede con
// Backspace (con el campo vacio) o con Escape.
enum class QuickNavStep {
    Book,
    Chapter,
    Verse
};

// Overlay tipo asistente para saltar a un libro/capitulo/versiculo,
// PASO A PASO:
//
//   1) Escribe el libro (nombre o abreviatura: "gn", "1co", "salmos") y
//      presiona Enter. Se confirma el mejor candidato (el de numero
//      canonico mas bajo entre los que calzan con lo tecleado).
//   2) Escribe el numero de capitulo y presiona Enter (Enter con el campo
//      vacio confirma el primer capitulo disponible).
//   3) Escribe el numero de versiculo y presiona Enter (Enter con el campo
//      vacio confirma el primer versiculo disponible). Este ultimo Enter
//      es el que confirma la seleccion completa.
//
// En cualquier paso, Backspace con el campo ya vacio, o Escape, retroceden
// un paso (y Escape en el primer paso cierra el buscador).
class BibleQuickNav {
public:
    // Abre el overlay en el paso "Libro" y reinicia todo el estado.
    void Open();
    // Cierra el overlay sin confirmar ninguna seleccion.
    void Close();
    bool IsOpen() const { return m_Open; }
// Rectángulo de la tarjeta (actualizado en Render(), leído en Update()
// para detectar clics fuera de la tarjeta).
ImVec2 m_CardMin = {};
ImVec2 m_CardMax = {};
int    m_OpenedFrame = -1;   // frame en que se llamo Open(), para no auto-cerrarse el mismo frame

    // Debe llamarse todos los frames (no hace nada si esta cerrado). Captura
    // el teclado del paso actual y avanza/retrocede segun corresponda.
    // Devuelve true SOLO en el frame en que se confirma el ultimo paso
    // (Versiculo); en ese caso revisar GetResolution() para conocer la
    // seleccion completa.
    bool Update(const BibleData& bible);

    // Dibuja la tarjeta central con el paso actual. Llamar despues de Update().
    void Render(const BibleData& bible);

    const QuickNavResolution& GetResolution() const { return m_Resolution; }

private:
    void RefreshBookCandidates(const BibleData& bible);
    void ConfirmBookStep(const BibleData& bible);
    void ConfirmChapterStep(const BibleData& bible);
    bool ConfirmVerseStep(const BibleData& bible);
    void GoBackStep(const BibleData& bible);

    bool         m_Open = false;
    QuickNavStep m_Step = QuickNavStep::Book;
double m_OpenSince    = -1.0; // ImGui::GetTime() cuando se llamo Open()
    double m_ClosingUntil = -1.0;
    std::string  m_BookBuffer;
    std::string  m_ChapterBuffer;
    std::string  m_VerseBuffer;

    std::vector<int> m_BookCandidates; // numeros canonicos que calzan con m_BookBuffer

    std::string  m_StatusMessage; // mensaje breve de error (ej: "Capitulo invalido")

    QuickNavResolution m_Resolution;
};

} // namespace ProyecThor::UI