#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <qobject.h>
#include <qtmetamacros.h>
#include <vector>

#include "audio/Playback/PlaybackController.hpp"
#include "persistence/ConfigManager.hpp"
#include "persistence/DatabaseManager.hpp"
#include "persistence/repositories/AlbumRepository.hpp"
#include "persistence/repositories/ArtistRepository.hpp"
#include "persistence/repositories/PlaylistRepository.hpp"
#include "persistence/repositories/TrackRepository.hpp"
#include "presentation/LibraryModels.hpp"
#include "scanner/FileScanner.hpp"
#include "scanner/TagReader.hpp"
#include "services/LibraryService.hpp"
#include "services/PlaybackService.hpp"
#include "services/PlaylistService.hpp"

class AppController : public QObject {
    Q_OBJECT

    Q_PROPERTY(TrackListModel* tracksModel READ tracksModel CONSTANT)
    Q_PROPERTY(PlaylistListModel* playlistsModel READ playlistsModel CONSTANT)
    Q_PROPERTY(TrackListModel* queueModel READ queueModel CONSTANT)
    Q_PROPERTY(AlbumListModel* albumsModel READ albumsModel CONSTANT)
    Q_PROPERTY(ArtistListModel* artistsModel READ artistsModel CONSTANT)

    Q_PROPERTY(TrackListModel* recentTracksModel READ recentTracksModel CONSTANT)
    Q_PROPERTY(TrackListModel* topTracksModel READ topTracksModel CONSTANT)
    Q_PROPERTY(TrackListModel* recentlyAddedModel READ recentlyAddedModel CONSTANT)
    Q_PROPERTY(ArtistListModel* topArtistsModel READ topArtistsModel CONSTANT)
    Q_PROPERTY(TrackListModel* favoriteTracksModel READ favoriteTracksModel CONSTANT)

    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)
    Q_PROPERTY(QString currentTrackTitle READ currentTrackTitle NOTIFY currentTrackChanged)
    Q_PROPERTY(QString currentTrackArtist READ currentTrackArtist NOTIFY currentTrackChanged)
    Q_PROPERTY(QString currentTrackCover READ currentTrackCover NOTIFY currentTrackChanged)
    Q_PROPERTY(int positionMs READ positionMs NOTIFY positionMsChanged)
    Q_PROPERTY(int durationMs READ durationMs NOTIFY durationMsChanged)
    Q_PROPERTY(float volume READ volume NOTIFY volumeChanged)
    Q_PROPERTY(bool shuffleEnabled READ shuffleEnabled NOTIFY shuffleEnabledChanged)
    Q_PROPERTY(int repeatMode READ repeatMode NOTIFY repeatModeChanged)

    Q_PROPERTY(bool isScanning READ isScanning NOTIFY isScanningChanged)
    Q_PROPERTY(float scanProgress READ scanProgress NOTIFY scanProgressChanged)

    Q_PROPERTY(TrackListModel* subViewTracksModel READ subViewTracksModel CONSTANT)
    Q_PROPERTY(QString subViewTitle READ subViewTitle NOTIFY subViewChanged)
    Q_PROPERTY(QString subViewSubtitle READ subViewSubtitle NOTIFY subViewChanged)
    Q_PROPERTY(QString subViewCover READ subViewCover NOTIFY subViewChanged)
    Q_PROPERTY(int subViewTrackCount READ subViewTrackCount NOTIFY subViewChanged)

public:
    explicit AppController(QObject* parent = nullptr);
    ~AppController() override;

    AppController(const AppController&) = delete;
    AppController& operator=(const AppController&) = delete;

    bool initialize();

    TrackListModel* tracksModel() { return &tracksModel_; }
    PlaylistListModel* playlistsModel() { return &playlistsModel_; }
    TrackListModel* queueModel() { return &queueModel_; }
    AlbumListModel* albumsModel() { return &albumsModel_; }
    ArtistListModel* artistsModel() { return &artistsModel_; }

    TrackListModel* recentTracksModel() { return &recentTracksModel_; }
    TrackListModel* topTracksModel() { return &topTracksModel_; }
    TrackListModel* recentlyAddedModel() { return &recentlyAddedModel_; }
    ArtistListModel* topArtistsModel() { return &topArtistsModel_; }
    TrackListModel* favoriteTracksModel() { return &favoriteTracksModel_; }

    TrackListModel* subViewTracksModel() { return &subViewTracksModel_;}
    QString subViewTitle() { return subViewTitle_; }
    QString subViewSubtitle() { return subViewSubtitle_; }
    QString subViewCover() { return subViewCover_;}
    int subViewTrackCount() const noexcept { return static_cast<int>(subViewTracks_.size()); }

    bool isPlaying() const noexcept { return isPlaying_; }
    QString currentTrackTitle() const { return currentTrackTitle_; }
    QString currentTrackArtist() const { return currentTrackArtist_; }
    QString currentTrackCover() const { return currentTrackCover_; }
    int positionMs() const noexcept { return positionMs_; }
    int durationMs() const noexcept { return durationMs_; }
    float volume() const noexcept { return volume_; }
    bool shuffleEnabled() const noexcept { return shuffleEnabled_; }
    int repeatMode() const noexcept { return repeatMode_; }
    bool isScanning() const noexcept { return isScanning_; }
    float scanProgress() const noexcept { return scanProgress_; }

    Q_INVOKABLE void playTrackById(qint64 trackId);
    Q_INVOKABLE void togglePlayPause();
    Q_INVOKABLE void stopPlayback();
    Q_INVOKABLE void nextTrack();
    Q_INVOKABLE void previousTrack();
    Q_INVOKABLE void seekTo(int positionMs);
    Q_INVOKABLE void setVolume(float volume);
    Q_INVOKABLE void toggleShuffle();
    Q_INVOKABLE void cycleRepeatMode();

    Q_INVOKABLE void searchTracks(const QString& query);
    Q_INVOKABLE void triggerRescan();
    Q_INVOKABLE void addLibraryFolder(const QUrl& folderUrl);

    Q_INVOKABLE void createPlaylist(const QString& name);
    Q_INVOKABLE void deletePlaylist(qint64 playlistId);
    Q_INVOKABLE void addTrackToPlaylist(qint64 playlistId, qint64 trackId);
    Q_INVOKABLE void playPlaylist(qint64 playlistId);
    Q_INVOKABLE void renamePlaylist(qint64 playlistId, const QString& name);
    Q_INVOKABLE void setPlaylistCover(qint64 playlistId, const QUrl& imageUrl);
    Q_INVOKABLE void toggleFavorite(qint64 trackId);

    Q_INVOKABLE void openAlbumSubView(qint64 albumId);
    Q_INVOKABLE void openArtistSubView(qint64 artistId);
    Q_INVOKABLE void openPlaylistSubView(qint64 playlistId);


    Q_INVOKABLE void playSubViewAll(int startIndex = 0);

signals:
    void isPlayingChanged();
    void currentTrackChanged();
    void positionMsChanged();
    void durationMsChanged();
    void volumeChanged();
    void shuffleEnabledChanged();
    void repeatModeChanged();
    void isScanningChanged();
    void scanProgressChanged();
    void subViewChanged();

private:
    void bindServiceCallbacks();
    void refreshLibraryModels();
    void refreshHomeModels();
    void refreshPlaylistModels();
    void refreshAlbumModels();
    void refreshArtistModels();

    ConfigManager configManager_;
    DatabaseManager dbManager_;

    TrackRepository trackRepo_;
    AlbumRepository albumRepo_;
    ArtistRepository artistRepo_;
    PlaylistRepository playlistRepo_;

    FileScanner fileScanner_;
    TagReader tagReader_;
    PlaybackController playbackController_;

    LibraryService libraryService_;
    PlaylistService playlistService_;
    PlaybackService playbackService_;

    TrackListModel tracksModel_;
    PlaylistListModel playlistsModel_;
    TrackListModel queueModel_;
    AlbumListModel albumsModel_;
    ArtistListModel artistsModel_;

    TrackListModel recentTracksModel_;
    TrackListModel topTracksModel_;
    TrackListModel recentlyAddedModel_;
    ArtistListModel topArtistsModel_;
    TrackListModel favoriteTracksModel_;
    std::vector<Track> subViewTracks_;
    TrackListModel subViewTracksModel_;

    bool isPlaying_ = false;
    QString currentTrackTitle_ = QStringLiteral("Sin reproducción");
    QString currentTrackArtist_;
    QString currentTrackCover_;
    QString subViewTitle_;
    QString subViewCover_;
    QString subViewSubtitle_;
    int positionMs_ = 0;
    int durationMs_ = 0;
    float volume_ = 0.8f;
    bool shuffleEnabled_ = false;
    int repeatMode_ = 0;
    bool isScanning_ = false;
    float scanProgress_ = 0.0f;
};