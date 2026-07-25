#pragma once
#include <string>
#include <vector>

namespace ProyecThor::Library {

// =============================================================================
//  Playlists — listas ordenadas de canciones, persistidas en
//  assets/playlists/<nombre>.playlist (una linea = un archivo de cancion).
//  Pensado para armar el set list del dia, como las Playlists de ProPresenter.
// =============================================================================

struct Playlist
{
    std::string              name;
    std::vector<std::string> songs; // nombres de archivo, en orden
};

std::vector<std::string> ListPlaylists();
Playlist                 LoadPlaylist(const std::string& name);
void                     SavePlaylist(const Playlist& playlist);

bool CreatePlaylist(const std::string& name);
void DeletePlaylist(const std::string& name);
bool RenamePlaylist(const std::string& oldName, const std::string& newName);

void AddSongToPlaylist(const std::string& playlistName, const std::string& songFilename);
void RemoveSongFromPlaylist(const std::string& playlistName, int index);
void MovePlaylistSong(const std::string& playlistName, int index, int delta); // delta: -1 sube, +1 baja

} // namespace ProyecThor::Library