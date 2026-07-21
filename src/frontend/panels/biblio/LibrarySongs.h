#pragma once
#include "LibraryContext.h"
#include <string>
#include <vector>

namespace ProyecThor::Library {

void RenderSideList(LibraryContext& ctx);
void RenderSongEditor(LibraryContext& ctx);

void CreateNewSong(LibraryContext& ctx);
void SaveSong(LibraryContext& ctx,
              const std::string& title,
              const std::string& content,
              const std::string& author);

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