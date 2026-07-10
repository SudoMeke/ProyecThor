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

void ApplySongSelection(LibraryContext& ctx, const std::string& filename);

} // namespace ProyecThor::Library