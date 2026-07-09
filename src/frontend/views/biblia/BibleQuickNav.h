#pragma once

#include <string>
#include <imgui.h>
#include "BibleTypes.h"

namespace ProyecThor::UI {

// Resultado de la resolucion en vivo del texto que el usuario va tecleando
// en el buscador rapido.
struct QuickNavResolution {
    bool hasBook            = false;
    int  bookCandidateCount = 0;   // cuantos libros calzan con el prefijo tecleado
    int  bookIdx            = -1;  // indice dentro de bible.books del MEJOR candidato

    bool hasChapter         = false;
    int  chapterNumber      = -1;
    int  chapterIdx         = -1;
    int  chapterVerseCount  = 0;   // cantidad de versiculos del capitulo resuelto

    bool hasVerse           = false;
    int  verseNumber        = -1;
    int  verseIdx           = -1;
};

// Overlay tipo "buscador rapido" (estilo command palette) para saltar a un
// libro/capitulo/versiculo escribiendo texto libre, por ejemplo:
//
//   "g"        -> mejor candidato: Genesis. Como tambien existe Galatas,
//                 se muestra el total de coincidencias para invitar a
//                 seguir escribiendo si no era el libro deseado.
//   "gn5"      -> Genesis capitulo 5
//   "gn5:1"    -> Genesis 5:1
//   "1"        -> ambiguo entre 1 Samuel, 1 Reyes, 1 Cronicas, 1 Corintios,
//                 1 Tesalonicenses, 1 Timoteo, 1 Pedro y 1 Juan: no se
//                 confirma solo, hay que seguir escribiendo.
//   "1co"      -> 1 Corintios (ya no ambiguo)
//   "1co13:4"  -> 1 Corintios 13:4
//
// El buffer tecleado siempre se muestra en pantalla, y Enter confirma
// siempre el MEJOR candidato actual (el de menor numero canonico), aunque
// todavia existan otras coincidencias posibles.
class BibleQuickNav {
public:
    // Abre el overlay y reinicia el buffer de texto tecleado.
    void Open();
    // Cierra el overlay sin confirmar ninguna seleccion.
    void Close();
    bool IsOpen() const { return m_Open; }

    // Debe llamarse todos los frames (no hace nada si esta cerrado). Captura
    // el teclado (letras, numeros, espacio, ':', backspace, enter, escape) y
    // recalcula la resolucion en vivo. Devuelve true SOLO en el frame en que
    // el usuario confirma con Enter; en ese caso revisar GetResolution()
    // para saber que libro/capitulo/versiculo fue confirmado.
    bool Update(const BibleData& bible);

    // Dibuja la tarjeta central con el buffer tecleado y la previsualizacion
    // de Libro / Capitulo / Versiculo. Llamar despues de Update().
    void Render(const BibleData& bible);

    const QuickNavResolution& GetResolution() const { return m_Resolution; }
    const std::string&        GetBuffer()     const { return m_Buffer; }

private:
    void Resolve(const BibleData& bible);

    bool               m_Open = false;
    std::string        m_Buffer;
    QuickNavResolution m_Resolution;
};

} // namespace ProyecThor::UI