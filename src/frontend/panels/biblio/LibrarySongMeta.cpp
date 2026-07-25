#include "LibrarySongMeta.h"
#include "LibraryHelpers.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace ProyecThor::Library {

static std::string SongMetaPath(const std::string& filename)
{
    return GetAssetsPath() + "/songs_meta/" + filename + ".json";
}

static void EnsureSongMetaDir()
{
    try {
        fs::create_directories(U8Path(GetAssetsPath() + "/songs_meta"));
    } catch (...) {
        // Best-effort — si falla, Set/GetSongMeta simplemente no persisten.
    }
}

SongMeta GetSongMeta(const std::string& filename)
{
    SongMeta meta; // linesPerSlide=0 (centinela legacy) por default

    std::ifstream f(U8Path(SongMetaPath(filename)));
    if (!f.is_open()) return meta;

    json j;
    try {
        f >> j;
    } catch (...) {
        return meta; // JSON corrupto/incompleto: se ignora, se comporta como si no existiera
    }

    meta.version      = j.value("version", 1);
    meta.title        = j.value("title", "");
    meta.artistAuthor = j.value("artistAuthor", "");
    meta.note         = j.value("note", "");
    meta.copyright    = j.value("copyright", "");
    meta.extra        = j.value("extra", "");
    meta.linesPerSlide = j.value("linesPerSlide", 0);

    return meta;
}

void SetSongMeta(const std::string& filename, const SongMeta& meta)
{
    EnsureSongMetaDir();

    json j;
    j["version"]       = meta.version;
    j["title"]         = meta.title;
    j["artistAuthor"]  = meta.artistAuthor;
    j["note"]          = meta.note;
    j["copyright"]     = meta.copyright;
    j["extra"]         = meta.extra;
    j["linesPerSlide"] = meta.linesPerSlide;

    std::ofstream f(U8Path(SongMetaPath(filename)));
    if (!f.is_open()) return;
    f << j.dump(2);
}

} // namespace ProyecThor::Library
