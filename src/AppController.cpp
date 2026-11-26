#include "AppController.hpp"

#include "domain/models/Artist.hpp"
#include "domain/models/Playlist.hpp"
#include "domain/models/Track.hpp"
#include "presentation/LibraryModels.hpp"

#include <QMetaObject>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <qobject.h>
#include <qstringview.h>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

std::filesystem::path resolveConfigPath() {
#ifdef _WIN32
    char* raw = nullptr;
    size_t len = 0;
    if (_dupenv_s(&raw, &len, "APPDATA") == 0 && raw != nullptr) {
        std::filesystem::path base{raw};
        std::free(raw);
        return base / "Nightingale" / "config.ini";
    }
    return std::filesystem::path{"."} / "config.ini";
#else
    if (const char* xdgConfig = std::getenv("XDG_CONFIG_HOME"); xdgConfig && *xdgConfig != '\0') {
        return std::filesystem::path{xdgConfig} / "Nightingale" / "config.ini";
    }
    if (const char* home = std::getenv("HOME"); home && *home != '\0') {
        return std::filesystem::path{home} / ".config" / "Nightingale" / "config.ini";
    }
    return std::filesystem::path{"."} / "config.ini";
#endif
}

QString formatDurationMs(uint32_t durationMs) {
    const uint32_t totalSecs = durationMs / 1000u;
    const uint32_t mins = totalSecs / 60u;
    const uint32_t secs = totalSecs % 60u;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%u:%02u", mins, secs);
    return QString::fromLatin1(buf);
}

QString formatArtistList(
    const std::vector<int64_t>& artistIds,
    const std::unordered_map<int64_t, std::string>& artistNames
) {
    std::string joined;
    for (int64_t id : artistIds) {
        auto it = artistNames.find(id);
        const std::string& name = (it != artistNames.end()) ? it->second : "Unknown Artist.";
        if (!joined.empty()) {
            joined += ", ";
        }
        joined += name;
    }
    return QString::fromStdString(joined.empty() ? "Unknown Artist." : joined);
}

TrackItem toTrackItem(
    const Track& track,
    const std::unordered_map<int64_t, std::string>& artistNames
) {
    TrackItem item;
    item.id = track.id;
    item.title = QString::fromStdString(track.name);
    item.artist = formatArtistList(track.artists, artistNames);
    item.durationMs = static_cast<int>(track.durationMs);
    item.durationText = formatDurationMs(track.durationMs);
    item.isFavorite = track.isFavorite;

    if (!track.coverPath.empty()) {
        std::error_code ec;
        if (std::filesystem::exists(track.coverPath, ec)) {
            item.coverUrl = QUrl::fromLocalFile(QString::fromStdString(track.coverPath)).toString();
        }
    }

    return item;
}

AlbumItem toAlbumItem(
    const Album& album,
    const std::unordered_map<int64_t, std::string>& artistNames
) {
    AlbumItem item;
    item.id = album.id;
    item.name = QString::fromStdString(album.name);
    item.artists = formatArtistList(album.artists, artistNames); 

    item.year = album.year.value_or(0);
    item.trackCount = static_cast<int>(album.tracks.size());

    if (!album.coverPath.empty()) {
        std::error_code ec;
        if (std::filesystem::exists(album.coverPath, ec)) {
            item.coverUrl = QUrl::fromLocalFile(QString::fromStdString(album.coverPath)).toString();
        }
    }

    return item;
}

ArtistItem toArtistItem(
    const Artist& artist
) {
    ArtistItem item;
    item.id = artist.id;
    item.name = QString::fromStdString(artist.name);
    item.trackCount = static_cast<int>(artist.tracks.size());
    item.biography = QString::fromStdString(artist.bio);
    if (!artist.coverPath.empty()) {
        std::error_code ec;
        if (std::filesystem::exists(artist.coverPath, ec)) {
            item.coverUrl = QUrl::fromLocalFile(QString::fromStdString(artist.coverPath)).toString();
        }
    }

    return item;
}

} // namespace

AppController::AppController(QObject* parent)
    : QObject(parent)
    , configManager_(resolveConfigPath())
    , dbManager_()
    , trackRepo_(dbManager_)
    , albumRepo_(dbManager_)
    , artistRepo_(dbManager_)
    , playlistRepo_(dbManager_)
    , fileScanner_()
    , tagReader_()
    , playbackController_()
    , libraryService_(trackRepo_, albumRepo_, artistRepo_, fileScanner_, tagReader_, configManager_)
    , playlistService_(playlistRepo_)
    , playbackService_(playbackController_, configManager_)
    , tracksModel_(this)
    , playlistsModel_(this)
    , queueModel_(this) 
    , recentTracksModel_(this)
    , topTracksModel_(this)
    , recentlyAddedModel_(this)
    , topArtistsModel_(this)
    , favoriteTracksModel_(this) {}

AppController::~AppController() {
    libraryService_.setOnScanProgress(nullptr);
    libraryService_.setOnLibraryUpdated(nullptr);
    playbackService_.setOnStateChanged(nullptr);
    playbackService_.setOnTrackChanged(nullptr);
    playbackService_.setOnPositionChanged(nullptr);
    playbackService_.setOnQueueChanged(nullptr);
}

bool AppController::initialize() {
    if (!configManager_.load()) {
        std::cerr << "[AppController] No se pudo cargar config.ini; usando valores por defecto.\n";
    }

    const AppConfig cfg = configManager_.getConfig();

    std::error_code ec;
    if (cfg.databasePath.has_parent_path()) {
        std::filesystem::create_directories(cfg.databasePath.parent_path(), ec);
    }
    if (!cfg.coverCacheDir.empty()) {
        std::filesystem::create_directories(cfg.coverCacheDir, ec);
    }

    if (!dbManager_.open(cfg.databasePath.string())) {
        std::cerr << "[AppController] Error al abrir SQLite en: " << cfg.databasePath << "\n";
        return false;
    }

    if (!artistRepo_.initSchema() ||
        !albumRepo_.initSchema() ||
        !trackRepo_.initSchema() ||
        !playlistRepo_.initSchema()) {
        std::cerr << "[AppController] Error al inicializar el esquema de la base de datos.\n";
        return false;
    }

    if (!playbackController_.init()) {
        std::cerr << "[AppController] Error al inicializar el dispositivo de audio.\n";
        return false;
    }

    playbackService_.syncFromConfig();

    volume_ = playbackService_.getVolume();
    shuffleEnabled_ = playbackService_.isShuffleEnabled();
    repeatMode_ = static_cast<int>(playbackService_.getRepeatMode());

    bindServiceCallbacks();
    refreshLibraryModels();
    refreshHomeModels();
    refreshPlaylistModels();
    refreshAlbumModels();
    refreshArtistModels();

    if (cfg.scanOnStartup) {
        libraryService_.startScanAsync();
    }

    return true;
}

void AppController::bindServiceCallbacks() {
    libraryService_.setOnScanProgress([this](const LibraryScanProgress& progress) {
        QMetaObject::invokeMethod(this, [this, progress]() {
            const bool scanningNow = !progress.isFinished;
            if (isScanning_ != scanningNow) {
                isScanning_ = scanningNow;
                emit isScanningChanged();
            }

            float ratio = 0.0f;
            if (progress.totalFilesFound > 0) {
                ratio = static_cast<float>(progress.processedFiles) /
                        static_cast<float>(progress.totalFilesFound);
            } else if (progress.isFinished) {
                ratio = 1.0f;
            }

            scanProgress_ = ratio;
            emit scanProgressChanged();
        }, Qt::QueuedConnection);
    });

    libraryService_.setOnLibraryUpdated([this]() {
        QMetaObject::invokeMethod(this, [this]() {
            refreshLibraryModels();
            refreshHomeModels();
            refreshAlbumModels();
            refreshArtistModels();
        }, Qt::QueuedConnection);
    });

    playbackService_.setOnStateChanged([this](PlaybackState state) {
        QMetaObject::invokeMethod(this, [this, state]() {
            const bool playingNow = (state == PlaybackState::Playing);
            if (isPlaying_ != playingNow) {
                isPlaying_ = playingNow;
                emit isPlayingChanged();
            }
        }, Qt::QueuedConnection);
    });

    playbackService_.setOnTrackChanged([this](const std::optional<Track>& trackOpt) {
        QMetaObject::invokeMethod(this, [this, trackOpt]() {
            if (!trackOpt) {
                currentTrackTitle_ = QStringLiteral("Nothing playing");
                currentTrackArtist_.clear();
                currentTrackCover_.clear();
                positionMs_ = 0;
                durationMs_ = 0;
                emit currentTrackChanged();
                emit positionMsChanged();
                emit durationMsChanged();
                return;
            }

            if (trackOpt->id > 0) {
                libraryService_.recordTrackPlay(trackOpt->id);
                refreshHomeModels();
            }

            std::unordered_map<int64_t, std::string> artistNames;
            for (const auto& artist : libraryService_.getAllArtists()) {
                artistNames.emplace(artist.id, artist.name);
            }

            const TrackItem item = toTrackItem(*trackOpt, artistNames);
            currentTrackTitle_ = item.title;
            currentTrackArtist_ = item.artist;
            currentTrackCover_ = item.coverUrl;
            durationMs_ = item.durationMs;

            emit currentTrackChanged();
            emit durationMsChanged();
        }, Qt::QueuedConnection);
    });

    playbackService_.setOnPositionChanged([this](uint32_t positionMs, uint32_t durationMs) {
        QMetaObject::invokeMethod(this, [this, positionMs, durationMs]() {
            positionMs_ = static_cast<int>(positionMs);
            emit positionMsChanged();

            if (durationMs > 0 && durationMs_ != static_cast<int>(durationMs)) {
                durationMs_ = static_cast<int>(durationMs);
                emit durationMsChanged();
            }
        }, Qt::QueuedConnection);
    });

    playbackService_.setOnQueueChanged([this](const std::vector<Track>& queue, size_t /*currentIndex*/) {
        QMetaObject::invokeMethod(this, [this, queue]() {
            std::unordered_map<int64_t, std::string> artistNames;
            for (const auto& artist : libraryService_.getAllArtists()) {
                artistNames.emplace(artist.id, artist.name);
            }

            std::vector<TrackItem> items;
            items.reserve(queue.size());
            for (const auto& t : queue) {
                items.push_back(toTrackItem(t, artistNames));
            }
            queueModel_.setItems(std::move(items));
        }, Qt::QueuedConnection);
    });
}

void AppController::playTrackById(qint64 trackId) {
    const auto allTracks = libraryService_.getAllTracks();
    for (size_t i = 0; i < allTracks.size(); ++i) {
        if (allTracks[i].id == trackId) {
            playbackService_.setQueue(allTracks, i);
            return;
        }
    }
}

void AppController::togglePlayPause() {
    playbackService_.togglePlayPause();
}

void AppController::stopPlayback() {
    playbackService_.stop();
}

void AppController::nextTrack() {
    playbackService_.next();
}

void AppController::previousTrack() {
    playbackService_.previous();
}

void AppController::seekTo(int positionMs) {
    if (positionMs >= 0) {
        playbackService_.seek(static_cast<uint32_t>(positionMs));
    }
}

void AppController::setVolume(float volume) {
    playbackService_.setVolume(volume);
    volume_ = playbackService_.getVolume();
    emit volumeChanged();
}

void AppController::toggleShuffle() {
    const bool nextState = !playbackService_.isShuffleEnabled();
    playbackService_.setShuffle(nextState);
    shuffleEnabled_ = nextState;
    emit shuffleEnabledChanged();
}

void AppController::cycleRepeatMode() {
    RepeatMode current = playbackService_.getRepeatMode();
    RepeatMode nextMode = RepeatMode::None;
    if (current == RepeatMode::None) {
        nextMode = RepeatMode::All;
    } else if (current == RepeatMode::All) {
        nextMode = RepeatMode::One;
    }
    playbackService_.setRepeatMode(nextMode);
    repeatMode_ = static_cast<int>(nextMode);
    emit repeatModeChanged();
}

void AppController::searchTracks(const QString& query) {
    const auto filtered = libraryService_.searchTracks(query.toStdString());

    std::unordered_map<int64_t, std::string> artistNames;
    for (const auto& artist : libraryService_.getAllArtists()) {
        artistNames.emplace(artist.id, artist.name);
    }

    std::vector<TrackItem> items;
    items.reserve(filtered.size());
    for (const auto& t : filtered) {
        items.push_back(toTrackItem(t, artistNames));
    }
    tracksModel_.setItems(std::move(items));
}

void AppController::triggerRescan() {
    libraryService_.startScanAsync();
}

void AppController::addLibraryFolder(const QUrl& folderUrl) {
    if (!folderUrl.isLocalFile()) {
        return;
    }
    configManager_.addLibraryPath(std::filesystem::path{folderUrl.toLocalFile().toStdString()});
    libraryService_.startScanAsync();
}

void AppController::createPlaylist(const QString& name) {
    if (playlistService_.createPlaylist(name.toStdString())) {
        refreshPlaylistModels();
    }
}

void AppController::deletePlaylist(qint64 playlistId) {
    if (playlistService_.deletePlaylist(static_cast<int>(playlistId))) {
        refreshPlaylistModels();
    }
}

void AppController::addTrackToPlaylist(qint64 playlistId, qint64 trackId) {
    if (playlistId <= 0 || trackId <= 0) {
        return;
    }
    if (playlistService_.addTrackToPlaylist(static_cast<int>(playlistId), static_cast<int>(trackId))) {
        refreshPlaylistModels();
    }
}

void AppController::renamePlaylist(qint64 playlistId, const QString& newName) {
    if (playlistId <= 0 || newName.trimmed().isEmpty()) return;
    if (playlistService_.renamePlaylist(static_cast<int>(playlistId), newName.trimmed().toStdString())) {
        refreshPlaylistModels();
        openPlaylistSubView(playlistId);
    }
}

void AppController::setPlaylistCover(qint64 playlistId, const QUrl& imageUrl) {
    if (playlistId <= 0 || !imageUrl.isLocalFile()) {
        return;
    }

    const std::filesystem::path sourcePath{imageUrl.toLocalFile().toStdString()};
    std::error_code ec;
    if (!std::filesystem::exists(sourcePath, ec) || !std::filesystem::is_regular_file(sourcePath, ec)) {
        return;
    }

    const AppConfig cfg = configManager_.getConfig();
    std::filesystem::path finalCoverPath = sourcePath;

    if (!cfg.coverCacheDir.empty()) {
        const std::filesystem::path destDir = cfg.coverCacheDir / "playlists";
        std::filesystem::create_directories(destDir, ec);

        // Elimina portadas previas de esa playlist con distinta extensión
        const std::string stem = std::to_string(playlistId);
        for (const char* ext : {".jpg", ".jpeg", ".png", ".webp"}) {
            std::filesystem::remove(destDir / (stem + ext), ec);
        }

        const std::string ext = sourcePath.has_extension() ? sourcePath.extension().string() : ".jpg";
        const std::filesystem::path destPath = destDir / (stem + ext);

        if (std::filesystem::copy_file(sourcePath, destPath, std::filesystem::copy_options::overwrite_existing, ec)) {
            finalCoverPath = destPath;
        }
    }

    if (playlistService_.updatePlaylistCover(static_cast<int>(playlistId), finalCoverPath.string())) {
        refreshPlaylistModels();
        openPlaylistSubView(playlistId);
    }
}

void AppController::playPlaylist(qint64 playlistId) {
    auto playlistOpt = playlistService_.getPlaylistWithTracks(static_cast<int>(playlistId));
    if (!playlistOpt || playlistOpt->tracks.empty()) {
        return;
    }

    std::unordered_map<int64_t, Track> tracksById;
    for (const auto& t : libraryService_.getAllTracks()) {
        tracksById.emplace(t.id, t);
    }

    std::vector<Track> playlistTracks;
    playlistTracks.reserve(playlistOpt->tracks.size());
    for (int64_t tid : playlistOpt->tracks) {
        if (auto it = tracksById.find(tid); it != tracksById.end()) {
            playlistTracks.push_back(it->second);
        }
    }

    if (!playlistTracks.empty()) {
        playbackService_.setQueue(playlistTracks, 0);
    }
}

void AppController::refreshAlbumModels() {
    const auto albums = libraryService_.getAllAlbums();
    const auto artists = libraryService_.getAllArtists();

    std::unordered_map<int64_t, std::string> artistNames;
    artistNames.reserve(artists.size());
    for (const auto& artist : artists) {
        artistNames.emplace(artist.id, artist.name);
    }

    std::vector<AlbumItem> items;
    items.reserve(albums.size());
    for (const auto& album : albums) {
        items.push_back(toAlbumItem(album, artistNames));
    }

    albumsModel_.setItems(std::move(items));
}

void AppController::refreshArtistModels() {
    const auto artists = libraryService_.getAllArtists();
    
    std::vector<ArtistItem> items;
    items.reserve(artists.size());
    for(const auto& artist : artists) {
        items.push_back(toArtistItem(artist));
    }

    artistsModel_.setItems(std::move(items));
}

void AppController::refreshLibraryModels() {
    const auto tracks = libraryService_.getAllTracks();
    const auto artists = libraryService_.getAllArtists();

    std::unordered_map<int64_t, std::string> artistNames;
    artistNames.reserve(artists.size());
    for (const auto& artist : artists) {
        artistNames.emplace(artist.id, artist.name);
    }

    std::vector<TrackItem> items;
    items.reserve(tracks.size());
    for (const auto& track : tracks) {
        items.push_back(toTrackItem(track, artistNames));
    }

    tracksModel_.setItems(std::move(items));
}

void AppController::refreshPlaylistModels() {
    const auto playlists = playlistService_.getAllPlaylists();
    const auto allTracks = libraryService_.getAllTracks();

    std::unordered_map<int64_t, uint32_t> trackDurationMsById;
    trackDurationMsById.reserve(allTracks.size());
    for (const auto& track : allTracks) {
        trackDurationMsById.emplace(track.id, track.durationMs);
    }

    std::vector<PlaylistItem> items;
    items.reserve(playlists.size());
    for (const auto& pl : playlists) {
        PlaylistItem item;
        item.id = pl.id;
        item.name = QString::fromStdString(pl.name);
        item.trackCount = static_cast<int>(pl.tracks.size());

        uint64_t totalMs = 0;
        for (int64_t trackId : pl.tracks) {
            if (auto it = trackDurationMsById.find(trackId); it != trackDurationMsById.end()) {
                totalMs += it->second;
            }
        }
        item.durationSecs = static_cast<int>(totalMs / 1000u);

        if (!pl.coverPath.empty()) {
            std::error_code ec;
            if (std::filesystem::exists(pl.coverPath, ec)) {
                item.coverUrl = QUrl::fromLocalFile(QString::fromStdString(pl.coverPath)).toString();
            }
        }
        items.push_back(std::move(item));
    }

    playlistsModel_.setItems(std::move(items));
}

void AppController::toggleFavorite(qint64 trackId) {
    if (libraryService_.toggleTrackFavorite(trackId)) {
        refreshLibraryModels();
        refreshHomeModels();
    }
}

void AppController::refreshHomeModels() {
    const auto artists = libraryService_.getAllArtists();
    std::unordered_map<int64_t, std::string> artistNames;
    artistNames.reserve(artists.size());
    for (const auto& artist : artists) {
        artistNames.emplace(artist.id, artist.name);
    }

    auto mapTracks = [&](const std::vector<Track>& source) {
        std::vector<TrackItem> out;
        out.reserve(source.size());
        for (const auto& track : source) {
            out.push_back(toTrackItem(track, artistNames));
        }
        return out;
    };

    recentTracksModel_.setItems(mapTracks(libraryService_.getRecentlyPlayedTracks(10)));
    topTracksModel_.setItems(mapTracks(libraryService_.getMostPlayedTracks(10)));
    recentlyAddedModel_.setItems(mapTracks(libraryService_.getRecentlyAdded(10)));
    favoriteTracksModel_.setItems(mapTracks(libraryService_.getFavoriteTracks()));

    const auto topArtists = libraryService_.getTopArtists(10);
    std::vector<ArtistItem> artistItems;
    artistItems.reserve(topArtists.size());
    for (const auto& entry : topArtists) {
        ArtistItem item;
        item.id = entry.artist.id;
        item.name = QString::fromStdString(entry.artist.name);
        item.playCount = static_cast<int>(entry.totalPlays);
        if (!entry.artist.coverPath.empty()) {
            std::error_code ec;
            if (std::filesystem::exists(entry.artist.coverPath, ec)) {
                item.coverUrl = QUrl::fromLocalFile(
                    QString::fromStdString(entry.artist.coverPath)
                ).toString();
            }
        }
        artistItems.push_back(std::move(item));
    }
    topArtistsModel_.setItems(std::move(artistItems));
}

void AppController::openAlbumSubView(qint64 albumId) {
    auto albumOpt = albumRepo_.getById(albumId);
    if (!albumOpt) return;

    subViewTracks_.clear();

    subViewTracks_ = libraryService_.getTracksByAlbum(albumId);
    const auto artists = libraryService_.getAllArtists();

    std::unordered_map<int64_t, std::string> artistNames;
    artistNames.reserve(artists.size());
    for (const auto& artist : artists) {
        artistNames.emplace(artist.id, artist.name);
    }

    std::vector<TrackItem> items;
    items.reserve(subViewTracks_.size());
    for (const auto& track : subViewTracks_) {
        items.push_back(toTrackItem(track, artistNames));
    }

    subViewTracksModel_.setItems(std::move(items));
    subViewTitle_ = QString::fromStdString(albumOpt->name);
    subViewSubtitle_ = formatArtistList(albumOpt->artists, artistNames);

    subViewCover_.clear();
    if (!albumOpt->coverPath.empty()) {
        std::error_code ec;
        if (std::filesystem::exists(albumOpt->coverPath, ec)) {
            subViewCover_ = QUrl::fromLocalFile(
                QString::fromStdString(albumOpt->coverPath)
            ).toString();
        }
    }

    emit subViewChanged();
}

void AppController::openArtistSubView(qint64 artistId) {
    auto artistOpt = artistRepo_.getById(artistId);
    if (!artistOpt) return;

    subViewTracks_.clear();

    subViewTracks_ = libraryService_.getTracksByArtist(artistId);
    const auto artists = libraryService_.getAllArtists();

    std::unordered_map<int64_t, std::string> artistNames;
    artistNames.reserve(artists.size());
    for (const auto& artist : artists) {
        artistNames.emplace(artist.id, artist.name);
    }

    std::vector<TrackItem> items;
    items.reserve(subViewTracks_.size());
    for (const auto& track : subViewTracks_) {
        items.push_back(toTrackItem(track, artistNames));
    }

    subViewTracksModel_.setItems(std::move(items));
    subViewTitle_ = QString::fromStdString(artistOpt->name);
    subViewSubtitle_.clear();

    subViewCover_.clear();
    if (!artistOpt->coverPath.empty()) {
        std::error_code ec;
        if (std::filesystem::exists(artistOpt->coverPath, ec)) {
            subViewCover_ = QUrl::fromLocalFile(
                QString::fromStdString(artistOpt->coverPath)
            ).toString();
        }
    }

    emit subViewChanged();
}


void AppController::openPlaylistSubView(qint64 playlistId) {
    auto playlistOpt = playlistService_.getPlaylistWithTracks(playlistId);
    if (!playlistOpt) return;

    subViewTracks_.clear();

    for(const auto& trackId : playlistOpt->tracks) {
        if (auto trackOpt = trackRepo_.getById(trackId)) {
            subViewTracks_.push_back(std::move(*trackOpt));
        }
    }
    const auto artists = libraryService_.getAllArtists();

    std::unordered_map<int64_t, std::string> artistNames;
    artistNames.reserve(artists.size());
    for (const auto& artist : artists) {
        artistNames.emplace(artist.id, artist.name);
    }

    std::vector<TrackItem> items;
    items.reserve(subViewTracks_.size());
    for (const auto& track : subViewTracks_) {
        items.push_back(toTrackItem(track, artistNames));
    }

    subViewTracksModel_.setItems(std::move(items));
    subViewTitle_ = QString::fromStdString(playlistOpt->name);
    subViewSubtitle_.clear();

    subViewCover_.clear();
    if (!playlistOpt->coverPath.empty()) {
        std::error_code ec;
        if (std::filesystem::exists(playlistOpt->coverPath, ec)) {
            subViewCover_ = QUrl::fromLocalFile(
                QString::fromStdString(playlistOpt->coverPath)
            ).toString();
        }
    }

    emit subViewChanged();
}


void AppController::playSubViewAll(int startIndex) {
    if (subViewTracks_.empty()) return;
    const size_t safeIdx = (startIndex >= 0 && static_cast<size_t>(startIndex) < subViewTracks_.size())
        ? static_cast<size_t>(startIndex)
        : 0;
    playbackService_.setQueue(subViewTracks_, safeIdx);
}