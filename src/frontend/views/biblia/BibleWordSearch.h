#pragma once

#include <string>
#include <vector>
#include <imgui.h>
#include "BibleTypes.h"

namespace ProyecThor::UI {

// Un versiculo que matcheo la busqueda, con indices listos para usar en
// BibleData (bookIdx/chapIdx/verseIdx), igual que QuickNavResolution.
struct WordSearchHit {
    int bookIdx  = -1;
    int chapIdx  = -1;
    int verseIdx = -1;
};

// Buscador de versiculos por PALABRAS DEL TEXTO -- a diferencia de
// BibleQuickNav (que resuelve una cita exacta "Gn 1:1"), este recorre el
// contenido de cada versiculo. Pensado para cuando el usuario recuerda
// palabras sueltas de un versiculo pero no la referencia.
//
// Coincidencia: el versiculo debe contener TODAS las palabras tecleadas
// (en cualquier orden, insensible a mayus/minus y tildes). Solo busca
// dentro de la traduccion actualmente cargada -- es la unica que esta
// completa en memoria (ver BibleView::LoadXMLBible); las demas viven en
// disco hasta que el usuario las abre.
class BibleWordSearch {
public:
    void Open();
    void Close();
    bool IsOpen() const { return m_Open; }

    // Recalcula resultados si el texto tecleado cambio desde el ultimo
    // frame. Llamar todos los frames mientras IsOpen().
    void Update(const BibleData& bible);

    // Dibuja el popup (campo de texto + lista de resultados con snippet),
    // anclado debajo de (anchorPos, anchorSize) -- mismo patron que
    // BibleView::RenderHistoryPopup/RenderFavoritesPopup. Devuelve true el
    // frame en que el usuario elige un resultado (click en la lista); en
    // ese caso ver GetResolution().
    bool Render(const BibleData& bible, ImVec2 anchorPos, ImVec2 anchorSize);

    const WordSearchHit& GetResolution() const { return m_Resolution; }

private:
    void RunSearch(const BibleData& bible);

    bool        m_Open = false;
    bool        m_NeedsFocus = false;
    char        m_Buffer[128] = "";
    std::string m_LastQuery; // ultimo buffer sobre el que ya se corrio la busqueda

    std::vector<WordSearchHit> m_Hits;
    static constexpr int kMaxHits = 200; // tope razonable para no listar miles de coincidencias

    WordSearchHit m_Resolution;
};

} // namespace ProyecThor::UI
