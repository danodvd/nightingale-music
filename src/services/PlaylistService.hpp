#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "../domain/models/Playlist.hpp"
#include "../persistence/repositories/PlaylistRepository.hpp"

class PlaylistService {
public:
    explicit PlaylistService(PlaylistRepository& playlistRepo);

    bool createPlaylist(const std::string& name, 
        const std::string& description = "", 
        const std::string& coverPath = "", 
        size_t durationSecs = 0,
        std::optional<std::vector<int64_t>> trackIds = std::nullopt);
    bool renamePlaylist(int64_t playlistId, const std::string& newName);
    bool deletePlaylist(int64_t playlistId);

    bool updatePlaylistCover(int playlistId, const std::string& coverPath);
    bool addTrackToPlaylist(int64_t playlistId, int64_t trackId);
    bool removeTrackFromPlaylist(int64_t playlistId, int64_t trackId);
    bool moveTrackInPlaylist(int64_t playlistId, size_t fromIndex, size_t toIndex);

    [[nodiscard]] std::vector<Playlist> getAllPlaylists() const;
    [[nodiscard]] std::optional<Playlist> getPlaylistWithTracks(int64_t playlistId) const;

private:
    PlaylistRepository& playlistRepo_;
};