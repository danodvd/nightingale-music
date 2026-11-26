#include "PlaylistService.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

PlaylistService::PlaylistService(PlaylistRepository& playlistRepo) : playlistRepo_(playlistRepo) {}

bool PlaylistService::createPlaylist(const std::string& name, 
    const std::string& description, 
    const std::string& coverPath, 
    size_t durationSecs, 
    std::optional<std::vector<int64_t>> trackIds) {
    Playlist playlist;
    playlist.name = name;
    playlist.description = description;
    playlist.coverPath = coverPath;
    playlist.durationSecs = durationSecs;
    if(trackIds.has_value() && !trackIds->empty()) {
        playlist.tracks.insert(
            playlist.tracks.end(),
            trackIds->begin(),
            trackIds->end()
        );
    }

    return playlistRepo_.insert(playlist);
}

bool PlaylistService::renamePlaylist(int64_t playlistId, const std::string& newName) {
    if(playlistId < 0) {
        throw std::out_of_range("Negative id.");
    }

    auto playlist = playlistRepo_.getById(playlistId);
    if (!playlist.has_value()) {
        return false;
        
    }
    Playlist temp = *playlist;
    temp.name = newName;

    return playlistRepo_.update(temp);
}

bool PlaylistService::deletePlaylist(int64_t playlistId) {
    if(playlistId < 0) {
        throw std::out_of_range("Negative id.");
    }

    return playlistRepo_.deleteById(playlistId);
}

bool PlaylistService::addTrackToPlaylist(int64_t playlistId, int64_t trackId) {
    if (playlistId <= 0 || trackId <= 0) {
        return false;
    }
    return playlistRepo_.addTrack(playlistId, trackId);
}

bool PlaylistService::removeTrackFromPlaylist(int64_t playlistId, int64_t trackId) {
    return playlistRepo_.removeTrack(playlistId, trackId);
}

bool PlaylistService::moveTrackInPlaylist(int64_t playlistId, size_t fromIndex, size_t toIndex) {
    return playlistRepo_.updateTrackPos(playlistId, fromIndex, toIndex);
}

bool PlaylistService::updatePlaylistCover(int playlistId, const std::string& coverPath) {
    if (playlistId <= 0) {
        return false;
    }
    auto playlist = playlistRepo_.getById(playlistId);
    if (!playlist.has_value()) {
        return false;
    }
    Playlist temp = *playlist;
    temp.coverPath = coverPath;
    return playlistRepo_.update(temp);
}

std::vector<Playlist> PlaylistService::getAllPlaylists() const {
    return playlistRepo_.getAll();
}
std::optional<Playlist> PlaylistService::getPlaylistWithTracks(int64_t playlistId) const {
    return playlistRepo_.getById(playlistId);
}