#include "LibraryService.hpp"
#include "persistence/repositories/ArtistRepository.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <cstdio>
#include <fstream>

namespace {

std::mutex gCallbackMutex;

struct ScanningStateGuard {
    std::atomic<bool>& flag;
    explicit ScanningStateGuard(std::atomic<bool>& f) noexcept : flag(f) {}
    ~ScanningStateGuard() noexcept { flag.store(false, std::memory_order_release); }
};

std::string makeAlbumCacheKey(int64_t artistId, const std::string& albumName) {
    return std::to_string(artistId) + "|" + albumName;
}

std::string toLowerAscii(std::string_view sv) {
    std::string result(sv);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

bool containsId(const std::vector<int64_t>& ids, int64_t target) {
    return std::find(ids.begin(), ids.end(), target) != ids.end();
}

std::optional<std::filesystem::path> findCachedImage(
    const std::filesystem::path& baseDir,
    const std::string& category,
    int id
) {
    if (baseDir.empty() || id <= 0) {
        return std::nullopt;
    }

    const std::filesystem::path folder = baseDir / category;
    const std::string stem = std::to_string(id);

    for (const char* ext : {".jpg", ".jpeg", ".png", ".webp"}) {
        std::filesystem::path candidate = folder / (stem + ext);
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec) && std::filesystem::is_regular_file(candidate, ec)) {
            return candidate;
        }
    }
    return std::nullopt;
}

} // namespace

uint64_t hashImageBytes(const std::vector<unsigned char>& data) {
    uint64_t hash = 14695981039346656037ULL;
    for (unsigned char byte : data) {
        hash ^= static_cast<uint64_t>(byte);
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::string extensionForMime(std::string_view mimeType) {
    const std::string lower = toLowerAscii(mimeType);
    if (lower.find("png") != std::string::npos) return ".png";
    if (lower.find("webp") != std::string::npos) return ".webp";
    if (lower.find("gif") != std::string::npos) return ".gif";
    if (lower.find("bmp") != std::string::npos) return ".bmp";
    return ".jpg";
}

std::string resolveTrackCoverPath(
    const std::filesystem::path& audioFilePath,
    const std::filesystem::path& cacheDir,
    const TagReader& tagReader
) {
    // 1. Intentar extraer carátula embebida del archivo de audio
    const auto covers = tagReader.extractCoverArt(audioFilePath);
    const RawCoverArt* selected = nullptr;

    for (const auto& art : covers) {
        if (art.data.empty()) continue;
        if (!selected || art.type == CoverArtType::FrontCover) {
            selected = &art;
            if (art.type == CoverArtType::FrontCover) {
                break;
            }
        }
    }

    if (selected && !cacheDir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(cacheDir, ec);

        char hexBuf[24];
        std::snprintf(
            hexBuf,
            sizeof(hexBuf),
            "%016llx",
            static_cast<unsigned long long>(hashImageBytes(selected->data))
        );

        const std::filesystem::path outPath =
            cacheDir / (std::string(hexBuf) + extensionForMime(selected->mimeType));

        if (!std::filesystem::exists(outPath, ec)) {
            std::ofstream outFile(outPath, std::ios::binary);
            if (outFile) {
                outFile.write(
                    reinterpret_cast<const char*>(selected->data.data()),
                    static_cast<std::streamsize>(selected->data.size())
                );
            }
        }

        if (std::filesystem::exists(outPath, ec)) {
            return outPath.string();
        }
    }

    // 2. Respaldo: buscar carátula externa en la misma carpeta del archivo
    const std::filesystem::path parentDir = audioFilePath.parent_path();
    for (const char* name : {"cover.jpg", "cover.png", "folder.jpg", "folder.png", "front.jpg", "front.png"}) {
        const std::filesystem::path candidate = parentDir / name;
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec) && std::filesystem::is_regular_file(candidate, ec)) {
            return candidate.string();
        }
    }

    return {};
}

LibraryService::LibraryService(
    TrackRepository& trackRepo,
    AlbumRepository& albumRepo,
    ArtistRepository& artistRepo,
    FileScanner& scanner,
    TagReader& tagReader,
    ConfigManager& configManager
)
    : trackRepo_(trackRepo)
    , albumRepo_(albumRepo)
    , artistRepo_(artistRepo)
    , scanner_(scanner)
    , tagReader_(tagReader)
    , configManager_(configManager) {}

LibraryService::~LibraryService() {
    cancelScan();
    if (scanThread_.joinable()) {
        scanThread_.join();
    }
}

void LibraryService::startScanAsync() {
    bool expected = false;
    if (!scanning_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }

    if (scanThread_.joinable()) {
        scanThread_.join();
    }

    cancelRequested_.store(false, std::memory_order_release);

    std::vector<std::filesystem::path> paths = configManager_.getLibraryPaths();

    scanThread_ = std::thread(&LibraryService::runScanWorker, this, std::move(paths));
}

void LibraryService::cancelScan() {
    cancelRequested_.store(true, std::memory_order_release);
}

bool LibraryService::isScanning() const noexcept {
    return scanning_.load(std::memory_order_acquire);
}

std::vector<Track> LibraryService::getAllTracks() const {
    return trackRepo_.getAll();
}

std::vector<Album> LibraryService::getAllAlbums() const {
    return albumRepo_.getAll();
}

std::vector<Artist> LibraryService::getAllArtists() const {
    return artistRepo_.getAll();
}

std::vector<Track> LibraryService::getTracksByAlbum(int albumId) const {
    std::vector<Track> result;
    const auto targetId = static_cast<int64_t>(albumId);

    for (const auto& track : trackRepo_.getAll()) {
        if (containsId(track.albums, targetId)) {
            result.push_back(track);
        }
    }
    return result;
}

std::vector<Track> LibraryService::getTracksByArtist(int artistId) const {
    std::vector<Track> result;
    const auto targetId = static_cast<int64_t>(artistId);

    for (const auto& track : trackRepo_.getAll()) {
        if (containsId(track.artists, targetId)) {
            result.push_back(track);
        }
    }
    return result;
}

std::vector<Track> LibraryService::searchTracks(const std::string& query) const {
    auto allTracks = trackRepo_.getAll();
    if (query.empty()) {
        return allTracks;
    }

    const std::string needle = toLowerAscii(query);
    std::vector<Track> result;
    for (const auto& track : allTracks) {
        if (toLowerAscii(track.name).find(needle) != std::string::npos) {
            result.push_back(track);
        }
    }
    return result;
}

std::optional<std::filesystem::path> LibraryService::getAlbumCoverPath(int albumId) const {
    if (auto album = albumRepo_.getById(albumId); album && !album->coverPath.empty()) {
        std::filesystem::path path(album->coverPath);
        std::error_code ec;
        if (std::filesystem::exists(path, ec) && std::filesystem::is_regular_file(path, ec)) {
            return path;
        }
    }

    const auto cfg = configManager_.getConfig();
    return findCachedImage(cfg.coverCacheDir, "albums", albumId);
}

std::optional<std::filesystem::path> LibraryService::getArtistCoverPath(int artistId) const {
    if (auto artist = artistRepo_.getById(artistId); artist && !artist->coverPath.empty()) {
        std::filesystem::path path(artist->coverPath);
        std::error_code ec;
        if (std::filesystem::exists(path, ec) && std::filesystem::is_regular_file(path, ec)) {
            return path;
        }
    }

    const auto cfg = configManager_.getConfig();
    return findCachedImage(cfg.coverCacheDir, "artists", artistId);
}

std::optional<std::filesystem::path> LibraryService::getPlaylistCoverPath(int playlistId) const {
    const auto cfg = configManager_.getConfig();
    return findCachedImage(cfg.coverCacheDir, "playlists", playlistId);
}

void LibraryService::setOnScanProgress(ProgressCallback callback) {
    std::lock_guard<std::mutex> lock(gCallbackMutex);
    onScanProgress_ = std::move(callback);
}

void LibraryService::setOnLibraryUpdated(LibraryUpdatedCallback callback) {
    std::lock_guard<std::mutex> lock(gCallbackMutex);
    onLibraryUpdated_ = std::move(callback);
}

void LibraryService::runScanWorker(std::vector<std::filesystem::path> rootPaths) {
    ScanningStateGuard guard(scanning_);

    auto emitProgress = [this](const LibraryScanProgress& progress) {
        ProgressCallback cb;
        {
            std::lock_guard<std::mutex> lock(gCallbackMutex);
            cb = onScanProgress_;
        }
        if (cb) {
            cb(progress);
        }
    };

    auto emitLibraryUpdated = [this]() {
        LibraryUpdatedCallback cb;
        {
            std::lock_guard<std::mutex> lock(gCallbackMutex);
            cb = onLibraryUpdated_;
        }
        if (cb) {
            cb();
        }
    };

    std::vector<std::filesystem::path> discoveredFiles =
        scanner_.scanDirectories(rootPaths, cancelRequested_);

    if (cancelRequested_.load(std::memory_order_acquire)) {
        return;
    }
    
    std::unordered_map<std::string, Track> existingTracksByPath;
    for (const auto& t : trackRepo_.getAll()) {
        existingTracksByPath.emplace(t.filePath, t);
    }

    std::unordered_map<int64_t, std::unordered_set<int64_t>> artistTrackIds;
    std::unordered_map<std::string, int64_t> artistIdCache;
    for (const auto& a : artistRepo_.getAll()) {
        artistIdCache.emplace(a.name, a.id);
        artistTrackIds[a.id];
    }

    std::unordered_map<int64_t, std::unordered_set<int64_t>> albumTrackIds;
    std::unordered_map<int64_t, std::unordered_set<int64_t>> albumArtistIds;
    std::unordered_map<std::string, int64_t> albumIdCache;
    for (const auto& al : albumRepo_.getAll()) {
        albumTrackIds[al.id];
        albumArtistIds[al.id];
        if (al.artists.empty()) {
            albumIdCache.emplace(makeAlbumCacheKey(0, al.name), al.id);
        } else {
            for (int64_t aid : al.artists) {
                albumIdCache.emplace(makeAlbumCacheKey(aid, al.name), al.id);
            }
        }
    }

    std::unordered_set<std::string> validDiskPaths;
    validDiskPaths.reserve(discoveredFiles.size());

    const size_t totalFiles = discoveredFiles.size();
    const AppConfig cfg = configManager_.getConfig();
    for (size_t i = 0; i < totalFiles; ++i) {
        if (cancelRequested_.load(std::memory_order_acquire)) {
            return;
        }

        const auto& filePath = discoveredFiles[i];
        const std::string normalizedPath = filePath.generic_string();
        validDiskPaths.insert(normalizedPath);

        emitProgress(LibraryScanProgress{
            i + 1,
            totalFiles,
            filePath.filename().string(),
            false
        });

        if (auto existingIt = existingTracksByPath.find(normalizedPath);
            existingIt != existingTracksByPath.end()) {
            const Track& existing = existingIt->second;

            for (int64_t artistId : existing.artists) {
                if (artistId > 0) {
                    artistTrackIds[artistId].insert(existing.id);
                }
            }
            for (int64_t albumId : existing.albums) {
                if (albumId > 0) {
                    albumTrackIds[albumId].insert(existing.id);
                    for (int64_t artistId : existing.artists) {
                        if (artistId > 0) {
                            albumArtistIds[albumId].insert(artistId);
                        }
                    }
                }
            }
            continue;
        }

        auto metadataOpt = tagReader_.readMetadata(filePath);
        if (!metadataOpt) {
            continue;
        }

        const auto& meta = *metadataOpt;
        const std::vector<std::string> artistName;
        const std::string albumName = meta.albumTitle.empty() ? "Unknown Album." : meta.albumTitle;
        const std::string trackName = meta.title.empty() ? "Unknown Track." : meta.title;

        std::vector<std::string> artistNames;
        for (const auto& name : meta.artists) {
            if (!name.empty()) {
                artistNames.push_back(name);
            }
        }
        if (artistNames.empty()) {
            artistNames.emplace_back("Unknown Artist.");
        }

        std::vector<int64_t> trackArtistIds;
        trackArtistIds.reserve(artistNames.size());

        for (const auto& artistName : artistNames) {
            int64_t artistId = 0;
            if (auto it = artistIdCache.find(artistName); it != artistIdCache.end()) {
                artistId = it->second;
            } else {
                Artist newArtist;
                newArtist.name = artistName;
                if (artistRepo_.insert(newArtist)) {
                    artistId = newArtist.id;
                    artistIdCache.emplace(artistName, artistId);
                }
            }

            if (artistId > 0 && !containsId(trackArtistIds, artistId)) {
                trackArtistIds.push_back(artistId);
            }
        }

        const int64_t primaryArtistId = trackArtistIds.empty() ? 0 : trackArtistIds.front();
        const std::string albumKey = makeAlbumCacheKey(primaryArtistId, albumName);

        int64_t albumId = 0;
        if (auto it = albumIdCache.find(albumKey); it != albumIdCache.end()) {
            albumId = it->second;
        } else {
            Album newAlbum;
            newAlbum.name = albumName;
            newAlbum.year = meta.year;
            newAlbum.artists = trackArtistIds;
            if (albumRepo_.insert(newAlbum)) {
                albumId = newAlbum.id;
                albumIdCache.emplace(albumKey, albumId);
            }
        }

        const std::string resolvedCover =
            resolveTrackCoverPath(filePath, cfg.coverCacheDir, tagReader_);
        
        if (!resolvedCover.empty() && albumId > 0 && albumName != "Unknown Album.") {
            if (auto albumOpt = albumRepo_.getById(albumId); albumOpt && albumOpt->coverPath.empty()) {
                albumOpt->coverPath = resolvedCover;
                albumRepo_.update(*albumOpt);
            }
        }
        if (!resolvedCover.empty()) {
            for (int64_t aid : trackArtistIds) {
                if (auto artistOpt = artistRepo_.getById(aid); artistOpt && artistOpt->coverPath.empty()) {
                    artistOpt->coverPath = resolvedCover;
                    artistRepo_.update(*artistOpt);
                }
            }
        }
        Track track;
        track.name = trackName;
        track.filePath = normalizedPath;
        track.coverPath = resolvedCover;
        track.artists = trackArtistIds;
        if (albumId > 0) {
            track.albums.push_back(albumId);
        }
        track.durationMs = static_cast<uint32_t>(meta.durationMs);
        track.trackNumber = meta.trackNumber;
        if (trackRepo_.insert(track) && track.id > 0) {
            existingTracksByPath.emplace(normalizedPath, track);
            for (int64_t aid : trackArtistIds) {
                artistTrackIds[aid].insert(track.id);
                if (albumId > 0) {
                    albumArtistIds[albumId].insert(aid);
                }
            }
            if (albumId > 0) {
                albumTrackIds[albumId].insert(track.id);
            }
        }
    }

    if (!cancelRequested_.load(std::memory_order_acquire)) {
        for (const auto& [path, dbTrack] : existingTracksByPath) {
            if (validDiskPaths.find(path) == validDiskPaths.end()) {
                trackRepo_.deleteById(dbTrack.id);
            }
        }

        for (const auto& [artistId, trackSet] : artistTrackIds) {
            std::vector<int64_t> trackVec(trackSet.begin(), trackSet.end());
            artistRepo_.syncRelations(artistId, trackVec);
        }

        for (const auto& [albumId, trackSet] : albumTrackIds) {
            const auto& artistSet = albumArtistIds[albumId];
            std::vector<int64_t> artistVec(artistSet.begin(), artistSet.end());
            std::vector<int64_t> trackVec(trackSet.begin(), trackSet.end());
            albumRepo_.syncRelations(albumId, artistVec, trackVec);
        }

        emitProgress(LibraryScanProgress{
            totalFiles,
            totalFiles,
            "",
            true
        });
        emitLibraryUpdated();
    }
}

std::vector<Track> LibraryService::getRecentlyPlayedTracks(size_t limit) const {
    return trackRepo_.getRecentlyPlayed(limit);
}

std::vector<Track> LibraryService::getMostPlayedTracks(size_t limit) const {
   return trackRepo_.getMostPlayed(limit);
}
std::vector<Track> LibraryService::getFavoriteTracks() const {
    return trackRepo_.getFavorites();
}

std::vector<ArtistPlayStats> LibraryService::getTopArtists(size_t limit) const {
    return artistRepo_.getTopArtists(limit);
}

std::vector<Track> LibraryService::getRecentlyAdded(size_t limit) const {
    return trackRepo_.getRecentlyAdded(limit);
}

bool LibraryService::recordTrackPlay(int64_t trackId) {
    if (trackId <= 0) {
        return false;
    }
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    return trackRepo_.recordPlay(trackId, now);
}

bool LibraryService::toggleTrackFavorite(int64_t trackId) {
    if (trackId <= 0) {
        return false;
    }
    auto trackOpt = trackRepo_.getById(trackId);
    if (!trackOpt) {
        return false;
    }
    return trackRepo_.setFavorite(trackId, !trackOpt->isFavorite);
}