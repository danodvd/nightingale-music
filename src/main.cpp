#include "main.h"
#include "core/databasemanager.hpp"
#include "core/audioscanner.hpp"
#include "core/audio/AudioManager.hpp"
#include "core/audio/QueueManager.hpp"
#include "portable-file-dialogs.h" // Add portable-file-dialogs header
#include "private/slint_image.h"
#include "private/slint_models.h"
#include "private/slint_string.h"
#include "private/slint_window.h"

#include <memory>
#include <string>

void refreshUiData(MainWindow& window, DatabaseManager& db, const std::shared_ptr<slint::VectorModel<TrackData>>& favoritesModel) {
    auto artistModel = std::make_shared<slint::VectorModel<MediaGroupData>>();
    auto albumModel = std::make_shared<slint::VectorModel<MediaGroupData>>();
    auto recentlyAdded = std::make_shared<slint::VectorModel<TrackData>>();
    auto trackModel = std::make_shared<slint::VectorModel<TrackData>>();
    auto playlistModel = std::make_shared<slint::VectorModel<Playlist>>();

    while(favoritesModel-> row_count() > 0) {
        favoritesModel->erase(0);
    }

    for (const auto& track : db.getRecentlyAdded(10)) {
        slint::Image cover;
        if (!track.cover_path.empty()) {
            cover = slint::Image::load_from_path(slint::SharedString(track.cover_path));
        }
        recentlyAdded->push_back({
            track.id,
            slint::SharedString(track.title),
            slint::SharedString(track.artist),
            cover,
            track.is_favorite
        });
    }

    for (const auto& artist : db.getGroupedMedia(FilterBy::Artist)) {
        slint::Image cover_img;
        if (!artist.cover_path.empty()) cover_img = slint::Image::load_from_path(slint::SharedString(artist.cover_path));
        artistModel->push_back({ slint::SharedString(artist.name), artist.track_count, cover_img });
    }

    for (const auto& album : db.getGroupedMedia(FilterBy::Album)) {
        slint::Image cover_img;
        if (!album.cover_path.empty()) cover_img = slint::Image::load_from_path(slint::SharedString(album.cover_path));
        albumModel->push_back({ slint::SharedString(album.name), album.track_count, cover_img });
    }

    for (const auto& track : db.getTracks()) {
        slint::Image cover;
        if (!track.cover_path.empty()) {
            cover = slint::Image::load_from_path(slint::SharedString(track.cover_path));
        }
        trackModel->push_back({
            track.id,
            slint::SharedString(track.title),
            slint::SharedString(track.artist),
            cover,
            track.is_favorite
        });
        if (track.is_favorite) {
            favoritesModel->push_back({
                track.id,
                slint::SharedString(track.title),
                slint::SharedString(track.artist),
                cover,
                track.is_favorite
            });
        }
    }

    for (const auto& p : db.getPlaylists()) {
        auto playlistTracksModel = std::make_shared<slint::VectorModel<TrackData>>();
        slint::Image cover;
        bool coverSet = false;

        // Populate the inner tracks array for this playlist
        for (const auto& track : db.getTracksByPlaylist(p.id)) {
            slint::Image trackCover;
            if (!track.cover_path.empty()) {
                trackCover = slint::Image::load_from_path(slint::SharedString(track.cover_path));
                
                // Use the first track's cover as the playlist's cover image
                if (!coverSet) {
                    cover = trackCover;
                    coverSet = true;
                }
            }
            playlistTracksModel->push_back({
                track.id,
                slint::SharedString(track.title),
                slint::SharedString(track.artist),
                trackCover,
                track.is_favorite
            });
        }

        Playlist slintPlaylist;
        slintPlaylist.name = slint::SharedString(p.title);
        slintPlaylist.cover = cover;
        slintPlaylist.tracks = playlistTracksModel;
        
        playlistModel->push_back(slintPlaylist);
    }

    window.set_playlists(playlistModel);
    window.set_recently_added(recentlyAdded);
    window.set_artist_list(artistModel);
    window.set_album_list(albumModel);
    window.set_track_list(trackModel);
    window.set_favorites(favoritesModel);
}

int main() {
    auto mainWindow = MainWindow::create();
    DatabaseManager db;
    AudioManager audioManager;
    QueueManager queueManager;
    AudioScanner scanner;
    slint::Timer playbackTimer;
    slint::Timer searchTimer;
    auto favoritesModel = std::make_shared<slint::VectorModel<TrackData>>();

    playbackTimer.start(
        slint::TimerMode::Repeated,
        std::chrono::milliseconds(100),
        [&audioManager, &mainWindow]() {
            if (!audioManager.isPlaying())
                return;

            mainWindow->set_currPos(
                audioManager.getProgress()
            );

        }
    );

    if (db.open("nightingale.db")) {
        refreshUiData(*mainWindow, db, favoritesModel);

        // Native Folder Dialog Import Callback
        mainWindow->on_importFiles([&db, &scanner, &mainWindow, favoritesModel]() {
            auto folder = pfd::select_folder("Select Music Folder", "").result();
            if (!folder.empty()) {
                scanner.scanDirectory(folder, db);
                refreshUiData(*mainWindow, db, favoritesModel); // Live update UI
            }
        });

        mainWindow->on_searchQuery([&db, &mainWindow, &searchTimer](slint::SharedString query) {
            std::string q = query.data();
            
            // If the search is empty, clear the UI immediately without hitting the DB
            if (q.empty()) {
                mainWindow->set_searchTracks(std::make_shared<slint::VectorModel<TrackData>>());
                mainWindow->set_searchAlbums(std::make_shared<slint::VectorModel<MediaGroupData>>());
                mainWindow->set_searchArtists(std::make_shared<slint::VectorModel<MediaGroupData>>());
                return;
            }

            // Start or restart the timer on every keystroke
            // The lambda will only execute if 250ms pass without another keystroke
            searchTimer.start(slint::TimerMode::SingleShot, std::chrono::milliseconds(250), [&db, &mainWindow, q]() {
                auto searchTracksModel = std::make_shared<slint::VectorModel<TrackData>>();
                auto searchAlbumsModel = std::make_shared<slint::VectorModel<MediaGroupData>>();
                auto searchArtistsModel = std::make_shared<slint::VectorModel<MediaGroupData>>();

                for (const auto& track : db.searchTracks(q)) {
                    slint::Image cover;
                    if(!track.cover_path.empty()) cover = slint::Image::load_from_path(slint::SharedString(track.cover_path));
                    searchTracksModel->push_back({track.id, slint::SharedString(track.title), slint::SharedString(track.artist), cover, track.is_favorite});
                }
                
                for(const auto& album : db.searchGroupedMedia(FilterBy::Album, q)) {
                    slint::Image cover;
                    if(!album.cover_path.empty()) cover = slint::Image::load_from_path(slint::SharedString(album.cover_path));
                    searchAlbumsModel->push_back({ slint::SharedString(album.name), album.track_count, cover});
                }
                
                for(const auto& artist : db.searchGroupedMedia(FilterBy::Artist, q)) {
                    slint::Image cover;
                    if(!artist.cover_path.empty()) cover = slint::Image::load_from_path(slint::SharedString(artist.cover_path));
                    searchArtistsModel->push_back({ slint::SharedString(artist.name), artist.track_count, cover});
                }

                mainWindow->set_searchTracks(searchTracksModel);
                mainWindow->set_searchAlbums(searchAlbumsModel);
                mainWindow->set_searchArtists(searchArtistsModel);
            });
        });

        mainWindow->on_toggleFavorites([&db, favoritesModel, mainWindow](int track_id) {
            db.toggleFavorite(track_id);
            auto trackOpt = db.getTrackById(track_id);
            if (!trackOpt) return;

            bool is_fav = trackOpt->is_favorite;

            // 1. Update the Favorites Model directly
            bool found_in_favs = false;
            for (int i = 0; i < favoritesModel->row_count(); ++i) {
                auto dataOpt = favoritesModel->row_data(i);
                if (dataOpt && dataOpt->id == track_id) {
                    found_in_favs = true;
                    if (!is_fav) {
                        // Remove the song if it was unfavorited
                        favoritesModel->erase(i);
                    } else {
                        // Update the state if it was favorited
                        dataOpt->is_favorite = true;
                        favoritesModel->set_row_data(i, *dataOpt);
                    }
                    break;
                }
            }

            // Add to favorites if it was favorited but not found in the model
            if (is_fav && !found_in_favs) {
                slint::Image cover;
                if(!trackOpt->cover_path.empty()) {
                    cover = slint::Image::load_from_path(slint::SharedString(trackOpt->cover_path));
                }
                favoritesModel->push_back({
                    track_id,
                    slint::SharedString(trackOpt->title),
                    slint::SharedString(trackOpt->artist),
                    cover,
                    is_fav
                });
            }

            // 2. Helper to find and update the star color in any generic model
            auto updateModel = [track_id, is_fav](std::shared_ptr<slint::Model<TrackData>> generic_model) {
                if (!generic_model) return;
                
                // Downcast to VectorModel so we can mutate the rows
                auto model = std::static_pointer_cast<slint::VectorModel<TrackData>>(generic_model);
                for (int i = 0; i < model->row_count(); ++i) {
                    auto dataOpt = model->row_data(i);
                    if (dataOpt && dataOpt->id == track_id) {
                        dataOpt->is_favorite = is_fav;
                        model->set_row_data(i, *dataOpt); // This triggers Slint to redraw the star!
                        break;
                    }
                }
            };

            // 3. Apply the helper to all other active UI lists
            updateModel(mainWindow->get_track_list());
            updateModel(mainWindow->get_recently_added());
            updateModel(mainWindow->get_searchTracks());
            updateModel(mainWindow->get_in_album_tracks());
            updateModel(mainWindow->get_in_artist_tracks());
        });

        mainWindow->on_playTrack([&audioManager, &queueManager, &db, &mainWindow](int track_id) {
            // For now, playing a track sets it as the sole item in the queue.
            // You can expand this later to load the whole album/view into the queue.
            queueManager.playNext(track_id);
            
            auto trackOpt = db.getTrackById(track_id);
            if(trackOpt) {
                if(audioManager.playTrack(trackOpt->path)) {
                    std::string displayTitle = trackOpt->title;
                    std::string displayArtist = trackOpt->artist;
                    slint::Image cover_img;

                    mainWindow->set_currTrackTitle(slint::SharedString(displayTitle));
                    mainWindow->set_currArtistName(slint::SharedString(displayArtist));
                    mainWindow->set_isPlaying(true);
                    mainWindow->set_currPos(0.0f);
                    
                    if (!trackOpt->cover_path.empty()) cover_img = slint::Image::load_from_path(slint::SharedString(trackOpt->cover_path));
                    mainWindow->set_currTrackCover(cover_img);
                }
            }
        });

        // Toggle Play/Pause Callback
        mainWindow->on_togglePlay([&audioManager, &mainWindow]() {
            audioManager.togglePlay();
            mainWindow->set_isPlaying(audioManager.isPlaying());
        });

        // Next Track Callback
        mainWindow->on_next([&audioManager, &queueManager, &db, &mainWindow]() {
            if (auto next = queueManager.next()) {
                auto nextId = queueManager.currentSongId();
                auto trackOpt = db.getTrackById(nextId);
                slint::Image cover_img;
                if (trackOpt && audioManager.playTrack(trackOpt->path)) {
                    std::string displayTitle = trackOpt->title;
                    std::string displayArtist = trackOpt->artist;
                    mainWindow->set_currTrackTitle(slint::SharedString(displayTitle));
                    mainWindow->set_currArtistName(slint::SharedString(displayArtist));
                    mainWindow->set_isPlaying(true);
                    mainWindow->set_currPos(0.0f);

                    if (!trackOpt->cover_path.empty()) cover_img = slint::Image::load_from_path(slint::SharedString(trackOpt->cover_path));
                    mainWindow->set_currTrackCover(cover_img);
                }
            }
        });

        // Previous Track Callback
        mainWindow->on_prev([&audioManager, &queueManager, &db, &mainWindow]() {
            if (auto prev = queueManager.previous()) {
                auto prevId = queueManager.currentSongId();
                auto trackOpt = db.getTrackById(prevId);
                slint::Image cover_img;
                if (trackOpt && audioManager.playTrack(trackOpt->path)) {
                    std::string displayTitle = trackOpt->title;
                    std::string displayArtist = trackOpt->artist;
                    mainWindow->set_currTrackTitle(slint::SharedString(displayTitle));
                    mainWindow->set_currArtistName(slint::SharedString(displayArtist));
                    mainWindow->set_isPlaying(true);
                    mainWindow->set_currPos(0.0f);

                    if (!trackOpt->cover_path.empty()) cover_img = slint::Image::load_from_path(slint::SharedString(trackOpt->cover_path));
                    mainWindow->set_currTrackCover(cover_img);
                }
            }
        });

        mainWindow->on_openAlbum([&db, &mainWindow](int index) {
            auto albums = db.getGroupedMedia(FilterBy::Album);

            if(index >= 0 && index < albums.size()) {
                std::string albumName = albums[index].name;
                mainWindow->set_in_album_name(slint::SharedString(albumName));

                auto albumTracksModel = std::make_shared<slint::VectorModel<TrackData>>();

                for(const auto& track : db.getTracksByAlbum(albumName)) {
                    slint::Image cover;
                    if(!track.cover_path.empty()) {
                        cover = slint::Image::load_from_path(slint::SharedString(track.cover_path));
                    }

                    albumTracksModel->push_back({
                        track.id,
                        slint::SharedString(track.title),
                        slint::SharedString(track.artist),
                        cover
                    });
                }

                mainWindow->set_in_album_tracks(albumTracksModel);
            }
        });
        mainWindow->on_openArtist([&db, &mainWindow](int index) {
            auto artists = db.getGroupedMedia(FilterBy::Artist);

            if(index >= 0 && index < artists.size()) {
                std::string artistName = artists[index].name;
                mainWindow->set_in_artist_name(slint::SharedString(artistName));

                auto artistTracksModel = std::make_shared<slint::VectorModel<TrackData>>();

                for(const auto& track : db.getTracksByArtist(artistName)) {
                    slint::Image cover;
                    if(!track.cover_path.empty()) {
                        cover = slint::Image::load_from_path(slint::SharedString(track.cover_path));
                    }

                    artistTracksModel->push_back({
                        track.id,
                        slint::SharedString(track.title),
                        slint::SharedString(track.artist),
                        cover
                    });
                }

                mainWindow->set_in_artist_tracks(artistTracksModel);
            }
        });

        mainWindow->on_addPlaylist([&db, &mainWindow, favoritesModel]() {
            std::string newPlaylistName = "My Playlist " + std::to_string(time(nullptr));

            if(db.createPlaylist(newPlaylistName)) {
                refreshUiData(*mainWindow, db, favoritesModel);
            }
        });
    }

    mainWindow->run();
    return 0;
}