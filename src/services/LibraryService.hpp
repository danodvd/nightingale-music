#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "../persistence/ConfigManager.hpp"
#include "../persistence/repositories/TrackRepository.hpp"
#include "../persistence/repositories/ArtistRepository.hpp"
#include "../persistence/repositories/AlbumRepository.hpp"
#include "../scanner/FileScanner.hpp"
#include "../scanner/TagReader.hpp"
#include "domain/models/Artist.hpp"

struct LibraryScanProgress {
    size_t processedFiles = 0;
    size_t totalFilesFound = 0;
    std::string currentFileName;
    bool isFinished = false;
};

class LibraryService {
public:
    using ProgressCallback = std::function<void(const LibraryScanProgress&)>;
    using LibraryUpdatedCallback = std::function<void()>;

    LibraryService(
        TrackRepository& trackRepo,
        AlbumRepository& albumRepo,
        ArtistRepository& artistRepo,
        FileScanner& scanner,
        TagReader& tagReader,
        ConfigManager& configManager
    );
    ~LibraryService();

    void startScanAsync();
    void cancelScan();
    [[nodiscard]] bool isScanning() const noexcept;

    [[nodiscard]] std::vector<Track> getAllTracks() const;
    [[nodiscard]] std::vector<Album> getAllAlbums() const;
    [[nodiscard]] std::vector<Artist> getAllArtists() const;
    [[nodiscard]] std::vector<Track> getTracksByAlbum(int albumId) const;
    [[nodiscard]] std::vector<Track> getTracksByArtist(int artistId) const;
    [[nodiscard]] std::vector<Track> searchTracks(const std::string& query) const;
    [[nodiscard]] std::vector<Track> getRecentlyPlayedTracks(size_t limit = 10) const;
    [[nodiscard]] std::vector<Track> getMostPlayedTracks(size_t limit = 10) const;
    [[nodiscard]] std::vector<Track> getFavoriteTracks() const;
    [[nodiscard]] std::vector<ArtistPlayStats> getTopArtists(size_t limit = 10) const;
    [[nodiscard]] std::vector<Track> getRecentlyAdded(size_t limit) const;

    [[nodiscard]] std::optional<std::filesystem::path> getAlbumCoverPath(int albumId) const;
    [[nodiscard]] std::optional<std::filesystem::path> getArtistCoverPath(int artistId) const;
    [[nodiscard]] std::optional<std::filesystem::path>  getPlaylistCoverPath(int playlistId) const;
    

    void setOnScanProgress(ProgressCallback callback);
    void setOnLibraryUpdated(LibraryUpdatedCallback callback);

    bool recordTrackPlay(int64_t trackId);
    bool toggleTrackFavorite(int64_t trackId);

private:
    TrackRepository& trackRepo_;
    AlbumRepository& albumRepo_;
    ArtistRepository& artistRepo_;
    FileScanner& scanner_;
    TagReader& tagReader_;
    ConfigManager& configManager_;

    std::atomic<bool> scanning_{false};
    std::atomic<bool> cancelRequested_{false};
    std::thread scanThread_;

    ProgressCallback onScanProgress_;
    LibraryUpdatedCallback onLibraryUpdated_;

    void runScanWorker(std::vector<std::filesystem::path> rootPaths);
};