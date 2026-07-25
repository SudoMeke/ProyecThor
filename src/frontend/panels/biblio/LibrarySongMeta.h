#pragma once
#include <string>

namespace ProyecThor::Library {

// =============================================================================
//  LibrarySongMeta — sidecar NUEVO por cancion (assets/songs_meta/<archivo>.json),
//  aditivo: no reemplaza songs_authors.ini/songs_tags.ini/songs_style.ini/
//  songs_background.ini/songs_stanza_colors.ini, que siguen intactos y son la
//  fuente de verdad para lo que ya leian (busqueda, preset de estilo/fondo por
//  cancion, color cosmetico por estrofa). Este archivo solo guarda lo nuevo
//  del rework del editor: campos de metadatos ampliados (Nota/Extra/Derechos
//  de autor) y la config de "lineas por diapositiva". (El estilo/fondo por
//  linea individual que se penso en un primer momento se saco: ya existe un
//  preset de estilo/fondo por CANCION ENTERA — tarjeta "Ajustes" del grid de
//  estrofas en SongView — y tener los dos era redundante.)
// =============================================================================

struct SongMeta {
    int         version      = 1;
    std::string title;
    std::string artistAuthor; // se espeja tambien en songs_authors.ini via SetSongAuthor
    std::string note;
    std::string copyright;
    std::string extra;

    // 0 = sin configurar todavia (centinela "legacy": una estrofa completa
    // sigue siendo una sola diapositiva, igual que el comportamiento de
    // antes de este rework). 1/2/3 = cantidad de lineas fisicas por
    // diapositiva dentro de cada estrofa.
    int linesPerSlide = 0;
};

SongMeta GetSongMeta(const std::string& filename);
void     SetSongMeta(const std::string& filename, const SongMeta& meta);

} // namespace ProyecThor::Library
