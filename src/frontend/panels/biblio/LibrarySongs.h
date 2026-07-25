#pragma once
#include "LibraryContext.h"
#include <string>
#include <vector>

namespace ProyecThor::Library {

void RenderSideList(LibraryContext& ctx);

// Divide la letra de <filename> en diapositivas: primero por estrofa (linea
// en blanco = limite, igual que siempre), y dentro de cada estrofa agrupando
// las lineas fisicas de a GetSongMeta(filename).linesPerSlide (si esta
// configurado) — ver LibrarySongMeta.h. Sin config guardada, una estrofa
// completa sigue siendo una sola diapositiva (comportamiento legacy).
std::vector<std::string> LoadSongVerses(const std::string& filename);

// Nucleo del agrupado de LoadSongVerses, factorizado para que SongEditView
// pueda recalcular el mismo resultado sobre el buffer EN MEMORIA (mientras
// el usuario todavia esta escribiendo/pegando, antes de guardar en disco) y
// asi el panel de preview del editor sea fiel a lo que se va a guardar.
// linesPerSlide<=0 = centinela legacy (una estrofa completa = una diapositiva).
std::vector<std::string> GroupLyricsIntoSlides(const std::string& normalizedContent, int linesPerSlide);

void CreateNewSong(LibraryContext& ctx);

// Variante de CreateNewSong para el menu Archivo > Importar > "Importar
// cancion desde portapapeles": crea el archivo con <clipboardText> como
// letra inicial (en vez de vacio) y pide que el editor unificado se abra
// directo. No recibe LibraryContext (a diferencia de CreateNewSong) porque
// se llama desde la barra de menu, que no tiene una instancia a mano —
// hace el mismo select+RequestSongEditorOpen directo contra PresentationCore.
void CreateNewSongFromClipboard(const std::string& clipboardText);

std::string GetSongAuthor(const std::string& filename);
void SetSongAuthor(const std::string& filename, const std::string& author);

std::vector<std::string> GetSongTags(const std::string& filename);
void SetSongTags(const std::string& filename, const std::vector<std::string>& tags);

// ── Preset por cancion (estilo + fondo por defecto) ─────────────────────────
// Reemplaza el viejo "estilo por defecto" a nivel de categoria completa: cada
// cancion puede tener su propio estilo/fondo preferido, asignado desde la
// tarjeta de ajustes (la primera del grid de estrofas en SongView). Si la
// cancion no tiene nada guardado, no se aplica nada (el operador elige a
// mano desde el selector, como siempre).
std::string GetSongStyle(const std::string& filename);
void SetSongStyle(const std::string& filename, const std::string& styleName);

struct SongBackground { std::string path; bool isVideo = false; };
SongBackground GetSongBackground(const std::string& filename);
void SetSongBackground(const std::string& filename, const std::string& path, bool isVideo);
void ClearSongBackground(const std::string& filename);

// ── Color de etiqueta por estrofa ────────────────────────────────────────────
// Tag visual libre (no un "tipo" automatico como Verso/Coro de ProPresenter,
// que requeriria parsear estructura que este parser de texto plano no tiene):
// el operador le pone color a mano a cada tarjeta para agrupar visualmente.
// 0 = sin color asignado (se dibuja neutro).
unsigned int GetStanzaColor(const std::string& filename, int stanzaIndex);
void SetStanzaColor(const std::string& filename, int stanzaIndex, unsigned int colorU32);

void ApplySongSelection(LibraryContext& ctx, const std::string& filename);

} // namespace ProyecThor::Library