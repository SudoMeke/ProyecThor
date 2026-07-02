#pragma once
// AudioAlbumArt.h — Extraccion de portadas embebidas en archivos de audio
// Usa las DLL de TagLib que ya vienen con VLC en MSYS2/UCRT64.
// La textura se carga en el contexto GL del hilo principal.

#include <string>
#include <vector>
#include <cstdint>
#include <GL/glew.h>

namespace ProyecThor::Audio {

// Resultado de la extraccion de portada
struct AlbumArt {
    std::vector<uint8_t> pixels;   // datos RGBA decodificados (stb_image)
    int                  width  = 0;
    int                  height = 0;
    GLuint               texID  = 0;  // 0 = sin textura GL creada aun

    bool HasData()    const { return !pixels.empty(); }
    bool HasTexture() const { return texID != 0; }
};

// Extrae la portada embebida del archivo de audio dado su path UTF-8.
// Soporta MP3 (ID3v2 APIC), FLAC (PICTURE block), M4A/AAC (covr atom),
// OGG Vorbis (METADATA_BLOCK_PICTURE).
// Devuelve AlbumArt con pixels llenos si encontro datos, vacio si no.
// No crea textura GL — llamar UploadAlbumArtToGL despues.
AlbumArt ExtractAlbumArt(const std::string& utf8FilePath);

// Sube los pixels de un AlbumArt a la GPU y guarda el ID en art.texID.
// Debe llamarse desde el hilo principal con contexto GL activo.
// Si art.texID != 0, primero destruye la textura anterior.
void UploadAlbumArtToGL(AlbumArt& art);

// Destruye la textura GL si existe.
void FreeAlbumArtTexture(AlbumArt& art);

} // namespace ProyecThor::Audio