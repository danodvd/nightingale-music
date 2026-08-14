#pragma once
#include <filesystem>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <iostream>
#include <vector>
#include <taglib/fileref.h>
#include <taglib/tag.h>

enum class FilterBy {
    Artist,
    Album,
    Title,
    Favorite
};

struct Track {
    int id;
    std::string title;
    std::string artist;
    std::string album;
    std::string path;
    std::string cover_path; 
    bool is_favorite;
};

struct MediaGroup {
    std::string name;
    int track_count;
    std::string cover_path; 
};

struct PlaylistRecord {
    int id;
    std::string title;
};

class DatabaseManager {
private:
    sqlite3* db = nullptr;

public:
    DatabaseManager() = default;
    ~DatabaseManager() { close(); }

    bool open(const std::string& path) {
        if(sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
            std::cerr << "Oops! An error occured while trying to open database: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }
        std::cout << "Database succesfully opened at: " << path << std::endl;
        return create_tables();
    }

    void close() {
        if(db) {
            sqlite3_close(db);
            db = nullptr;
        }
    }

    void toggleFavorite(int id) {

        sqlite3_stmt* stmt = nullptr;

        const char* sql = "UPDATE tracks SET is_favorite = (1 - is_favorite) WHERE id = ?;";

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to prepare statement: "
                    << sqlite3_errmsg(db) << std::endl;
            return;
        }

        sqlite3_bind_int(stmt, 1, id);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr << "Failed to update favorite: "
                    << sqlite3_errmsg(db) << std::endl;
        }
        sqlite3_finalize(stmt);
    }

    std::vector<Track> searchTracks(const std::string& term) {
        std::vector<Track> tracks;
        if(term.empty()) return tracks;
        
        std::string sql = "SELECT t.id, t.title, t.artist, t.album, t.filePath, t.cover_path, t.is_favorite "
                          "FROM tracks t JOIN tracks_search ts ON t.id = ts.rowid "
                          "WHERE ts.tracks_search MATCH ? LIMIT 20;";

        sqlite3_stmt* stmt;

        if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            std::string search_term = term + "*";

            sqlite3_bind_text(stmt, 1, search_term.c_str(), -1, SQLITE_TRANSIENT);

            while(sqlite3_step(stmt) == SQLITE_ROW) {
                Track track;
                track.id = sqlite3_column_int(stmt, 0);
                track.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                track.artist = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                track.album = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                track.path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                auto cover_val = sqlite3_column_text(stmt, 5);
                track.cover_path = cover_val ? reinterpret_cast<const char*>(cover_val) : "";
                track.is_favorite = sqlite3_column_int(stmt, 6) == 1;
                
                tracks.push_back(track);
            }
        }
        sqlite3_finalize(stmt);
        return tracks;
    }

    std::vector<MediaGroup> searchGroupedMedia(FilterBy type, const std::string& term) {
        std::vector<MediaGroup> results;
        if(term.empty()) return results;

        std::string column = getFilters(type);
        if(column.empty() || type == FilterBy::Title) return results;

        std::string sql = "SELECT " + column + ", COUNT(id), MIN(cover_path) FROM tracks "
                          "WHERE " + column + " LIKE ? GROUP BY " + column + " ORDER BY " + column + " ASC LIMIT 20";
        sqlite3_stmt* stmt;

        if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            std::string search_term = "%" + term + "%";
            sqlite3_bind_text(stmt, 1, search_term.c_str(), -1, SQLITE_TRANSIENT);

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                MediaGroup item;
                auto text_val = sqlite3_column_text(stmt, 0);
                item.name = text_val ? reinterpret_cast<const char*>(text_val) : "Unknown";
                item.track_count = sqlite3_column_int(stmt, 1);
                auto cover_val = sqlite3_column_text(stmt, 2);
                item.cover_path = cover_val ? reinterpret_cast<const char*>(cover_val) : "";

                results.push_back(item);
            }
        }
        sqlite3_finalize(stmt);
        return results;
    }

    bool createPlaylist(const std::string& playlistTitle) {
        const char* sql = "INSERT INTO playlists (title) VALUES (?);";
        sqlite3_stmt* stmt;

        if(sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Oops! An error has occured while trying to insert the selected track: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }
        if(sqlite3_bind_text(stmt, 1, playlistTitle.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
            std::cerr << "Oops! Failed to bind playlist title: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_finalize(stmt);
            return false;
        }
        if(sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr << "Oops! Failed to create playlist: "// TODO: Distinguis "playlist already exists" from other database errors
                  << sqlite3_errmsg(db) << std::endl;
            sqlite3_finalize(stmt);
            return false;
        }

        sqlite3_finalize(stmt);
        return true;
    }

    bool addToPlaylist(int trackId, int playlistId, int pos) {
        const char* sql = "INSERT OR IGNORE INTO playlist_tracks (playlist_id, track_id, position) VALUES (?, ?, ?);";
        sqlite3_stmt* stmt;

        if(sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Oops! An error has occured while trying to add the track to playlist: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }
        if(sqlite3_bind_int(stmt, 1, playlistId) != SQLITE_OK ||
            sqlite3_bind_int(stmt, 2, trackId) != SQLITE_OK ||
            sqlite3_bind_int(stmt, 3, pos) != SQLITE_OK) {
              std::cerr << "Failed to bind track/playlist: " << sqlite3_errmsg(db) << std::endl;  
              sqlite3_finalize(stmt);
              return false;
        }
        if(sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr << "Oops! Failed to add track to playlist: "// TODO: Distinguis "track already in playlist" from other database errors
                  << sqlite3_errmsg(db) << std::endl;
            sqlite3_finalize(stmt);
            return false;
        }

        sqlite3_finalize(stmt);
        return true;
    }

    bool removeFromPlaylist(int trackId, int playlistId) {
        const char* sql = "DELETE FROM playlist_tracks WHERE track_id = ? AND playlist_id = ?;";
        sqlite3_stmt* stmt;

        if(sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "An error has occured while trying to remove track from playlist: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }
        if(sqlite3_bind_int(stmt, 1, trackId) != SQLITE_OK || sqlite3_bind_int(stmt, 2, playlistId) != SQLITE_OK) {
            std::cerr << "Failed to delete track (" << trackId << ") from playlist (" << playlistId << "): " << sqlite3_errmsg(db) << std::endl;
            sqlite3_finalize(stmt);
            return false;
        }
        if(sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr << "Oops! Failed to remove track from playlist: "
                  << sqlite3_errmsg(db) << std::endl;
            sqlite3_finalize(stmt);
            return false;
        }

        sqlite3_finalize(stmt);
        return true;
    }

    int insertTracks(const std::string& rootDirectory) {
        const char* sql = "INSERT OR IGNORE INTO tracks (title, artist, album, filePath) VALUES (?, ?, ?, ?);";
        sqlite3_stmt* stmt;

        if(sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Oops! An error has occured while trying to insert the selected track: " << sqlite3_errmsg(db) << std::endl;
            return -1;
        }

        int added = 0;
        sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

        for (const auto& entry : std::filesystem::recursive_directory_iterator(rootDirectory)) {
            if (entry.is_regular_file() && isAudioFile(entry.path().string())) {
                Track track;
                track.path = entry.path().string();

                if (readMetadata(track.path, track)) {
                    if (insertIntoDatabase(stmt, track)) {
                        added++;
                    }
                }
            }
        }   

        sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);

        sqlite3_finalize(stmt);

        return added;
    }

    std::vector<Track> getRecentlyAdded(int limit) {
        const char* sql = "SELECT id, title, artist, album, filePath, cover_path, is_favorite FROM tracks ORDER BY added_at DESC LIMIT ?;";
        sqlite3_stmt* stmt;
        std::vector<Track> tracks;

        if(sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Oops! An unknown error has occured: " << sqlite3_errmsg(db);
            return tracks;
        }

        sqlite3_bind_int(stmt, 1, limit);

        while(sqlite3_step(stmt) == SQLITE_ROW) {
            Track track;

            track.id = sqlite3_column_int(stmt, 0);
            track.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            track.artist = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            track.album = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            track.path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            const unsigned char* cover_val = sqlite3_column_text(stmt, 5);
            track.cover_path = cover_val ? reinterpret_cast<const char*>(cover_val) : "";
            track.is_favorite = sqlite3_column_int(stmt, 6) == 1;
            
            tracks.push_back(track);
        }

        sqlite3_finalize(stmt);
        return tracks;
    }

    std::vector<Track> getTracks() {
        const char* sql = "SELECT id, title, artist, album, filePath, cover_path, is_favorite FROM tracks;";
        sqlite3_stmt* stmt;
        std::vector<Track> tracks;

        if(sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Oops! An unknown error has occured: " << sqlite3_errmsg(db);
            return tracks;
        }

        while(sqlite3_step(stmt) == SQLITE_ROW) {
            Track track;

            track.id = sqlite3_column_int(stmt, 0);
            track.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            track.artist = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            track.album = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            track.path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            const unsigned char* cover_val = sqlite3_column_text(stmt, 5);
            track.cover_path = cover_val ? reinterpret_cast<const char*>(cover_val) : "";
            track.is_favorite = sqlite3_column_int(stmt, 6) == 1;
            tracks.push_back(track);
        }

        sqlite3_finalize(stmt);
        return tracks;
    }

    std::vector<MediaGroup> getGroupedMedia(FilterBy type) {
        std::string column = getFilters(type);
        std::vector<MediaGroup> results;

        if (column.empty() || type == FilterBy::Title) {
            return results; 
        }

        std::string sql = "SELECT " + column + ", COUNT(id), MIN(cover_path) FROM tracks GROUP BY " + column + " ORDER BY " + column + " ASC;";
        sqlite3_stmt* stmt;

        if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Oops! An error occurred in getGroupedMedia: " << sqlite3_errmsg(db) << '\n';
            return results;
        }

        while(sqlite3_step(stmt) == SQLITE_ROW) {
            MediaGroup item;
            
            const unsigned char* text_val = sqlite3_column_text(stmt, 0);
            item.name = text_val ? reinterpret_cast<const char*>(text_val) : "Unknown";
            
            item.track_count = sqlite3_column_int(stmt, 1);
            const unsigned char* cover_val = sqlite3_column_text(stmt, 2);
            item.cover_path = cover_val ? reinterpret_cast<const char*>(cover_val) : "";

            results.push_back(item);
        }

        sqlite3_finalize(stmt);
        return results;
    }

    std::optional<Track> getTrackById(int id) {
        const char* sql = "SELECT title, artist, filePath, cover_path, is_favorite FROM tracks WHERE id = ?;";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, id);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                Track t;
                auto title_val = sqlite3_column_text(stmt, 0);
                auto artist_val = sqlite3_column_text(stmt, 1);
                auto path_val = sqlite3_column_text(stmt, 2);
                auto cover_path_val = sqlite3_column_text(stmt, 3);
            
                t.title = title_val ? reinterpret_cast<const char*>(title_val) : "Unknown";
                t.artist = artist_val ? reinterpret_cast<const char*>(artist_val) : "Unknown";
                t.path = path_val ? reinterpret_cast<const char*>(path_val) : "";
                t.cover_path = cover_path_val ? reinterpret_cast<const char*>(cover_path_val) : "";
                t.is_favorite = sqlite3_column_int(stmt, 4) == 1 ? true : false;
                sqlite3_finalize(stmt);
                return t;
            }
        }
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    std::vector<PlaylistRecord> getPlaylists() {
        const char* sql = "SELECT id, title FROM playlists;";
        sqlite3_stmt* stmt;
        std::vector<PlaylistRecord> results;
        
        if(sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            while(sqlite3_step(stmt) == SQLITE_ROW) {
                PlaylistRecord p;
                p.id = sqlite3_column_int(stmt, 0);
                auto text_val = sqlite3_column_text(stmt, 1);
                p.title = text_val ? reinterpret_cast<const char*>(text_val) : "My playlist";
                results.push_back(p);
            }
        }
        sqlite3_finalize(stmt);
        return results;
    }

    std::vector<Track> getTracksByPlaylist(int playlistId) {
        const char* sql = "SELECT t.id, t.title, t.artist, t.album, t.filePath, t.cover_path, t.is_favorite "
                        "FROM tracks t "
                        "JOIN playlist_tracks pt ON t.id = pt.track_id "
                        "WHERE pt.playlist_id = ? ORDER BY pt.position ASC;";
        sqlite3_stmt* stmt;
        std::vector<Track> tracks;
        
        if(sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, playlistId);
            while(sqlite3_step(stmt) == SQLITE_ROW) {
                Track track;
                track.id = sqlite3_column_int(stmt, 0);
                track.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                track.artist = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                track.album = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                track.path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                
                const unsigned char* cover_val = sqlite3_column_text(stmt, 5);
                track.cover_path = cover_val ? reinterpret_cast<const char*>(cover_val) : "";
                track.is_favorite = sqlite3_column_int(stmt, 6) == 1;
                
                tracks.push_back(track);
            }
        }
        sqlite3_finalize(stmt);
        return tracks;
    }

    std::vector<Track> getTracksByAlbum(const std::string& albumName) {
        const char* sql = "SELECT id, title, artist, album, filePath, cover_path, is_favorite FROM tracks WHERE album = ?;";
        sqlite3_stmt* stmt;
        std::vector<Track> tracks;
        
        if(sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Oops! An error occurred: " << sqlite3_errmsg(db) << '\n';
            return tracks;
        }
        
        sqlite3_bind_text(stmt, 1, albumName.c_str(), -1, SQLITE_TRANSIENT);
        
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            Track track;
            track.id = sqlite3_column_int(stmt, 0);
            track.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            track.artist = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            track.album = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            track.path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            
            const unsigned char* cover_val = sqlite3_column_text(stmt, 5);
            track.cover_path = cover_val ? reinterpret_cast<const char*>(cover_val) : "";
            track.is_favorite = sqlite3_column_int(stmt, 6) == 1;
            
            tracks.push_back(track);
        }
        sqlite3_finalize(stmt);
        return tracks;
    }
    std::vector<Track> getTracksByArtist(const std::string& artistName) {
        const char* sql = "SELECT id, title, artist, album, filePath, cover_path, is_favorite FROM tracks WHERE artist = ?;";
        sqlite3_stmt* stmt;
        std::vector<Track> tracks;
        
        if(sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Oops! An error occurred: " << sqlite3_errmsg(db) << '\n';
            return tracks;
        }
        
        sqlite3_bind_text(stmt, 1, artistName.c_str(), -1, SQLITE_TRANSIENT);
        
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            Track track;
            track.id = sqlite3_column_int(stmt, 0);
            track.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            track.artist = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            track.album = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            track.path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            
            const unsigned char* cover_val = sqlite3_column_text(stmt, 5);
            track.cover_path = cover_val ? reinterpret_cast<const char*>(cover_val) : "";
            track.is_favorite = sqlite3_column_int(stmt, 6) == 1;
            
            tracks.push_back(track);
        }
        sqlite3_finalize(stmt);
        return tracks;
    }
public:
    bool exec(const std::string& sql) {
        char* errMsg = nullptr;
        if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::cerr << "SQL Error: " << errMsg << std::endl;
            sqlite3_free(errMsg);
            return false;
        }
        return true;
    }
    bool insertTrack(const Track& track) {
        const char* sql = "INSERT OR IGNORE INTO tracks (title, artist, album, filePath, cover_path) VALUES (?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt;
        if(sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        
        bool result = insertIntoDatabase(stmt, track);
        sqlite3_finalize(stmt);
        return result;
    }
private:
    bool create_tables() {
        std::string sql_tracks = 
            "CREATE TABLE IF NOT EXISTS tracks ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "title TEXT NOT NULL, "
            "artist TEXT NOT NULL, "
            "album TEXT NOT NULL, "
            "filePath TEXT UNIQUE NOT NULL, "
            "cover_path TEXT, "
            "is_favorite INTEGER DEFAULT 0, "
            "added_at DATETIME DEFAULT CURRENT_TIMESTAMP"
            ");";

        std::string sql_artists =
            "CREATE TABLE IF NOT EXISTS artists ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "name TEXT UNIQUE"
            ");";

        std::string sql_albums =
            "CREATE TABLE IF NOT EXISTS albums ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "title TEXT, "
            "cover_path TEXT, "
            "year INTEGER"
            ");";

        std::string sql_album_artists =
            "CREATE TABLE IF NOT EXISTS album_artists ("
            "album_id INTEGER NOT NULL, "
            "artist_id INTEGER NOT NULL, "
            "PRIMARY KEY (album_id, artist_id), "
            "FOREIGN KEY (album_id) REFERENCES albums(id) ON DELETE CASCADE, "
            "FOREIGN KEY (artist_id) REFERENCES artists(id) ON DELETE CASCADE"
            ");";

        std::string sql_track_artists =
            "CREATE TABLE IF NOT EXISTS track_artists ("
            "track_id INTEGER NOT NULL, "
            "artist_id INTEGER NOT NULL, "
            "is_primary INTEGER NOT NULL DEFAULT 0, "
            "PRIMARY KEY (track_id, artist_id), "
            "FOREIGN KEY (track_id) REFERENCES tracks(id) ON DELETE CASCADE, "
            "FOREIGN KEY (artist_id) REFERENCES artists(id) ON DELETE CASCADE"
            ");";

        std::string sql_fts =
            "CREATE VIRTUAL TABLE IF NOT EXISTS tracks_search USING fts5("
            "title, artist, album, content='tracks', content_rowid='id'"
            ");";

        std::string sql_triggers =
            "CREATE TRIGGER IF NOT EXISTS after_track_insert AFTER INSERT ON tracks BEGIN "
            "  INSERT INTO tracks_search(rowid, title, artist, album) VALUES (new.id, new.title, new.artist, new.album); "
            "END;";

        std::string sql_playlists = 
            "CREATE TABLE IF NOT EXISTS playlists ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "title STRING UNIQUE"
            ");";

        std::string sql_playlist_tracks =
            "CREATE TABLE IF NOT EXISTS playlist_tracks ("
            "playlist_id INTEGER NOT NULL, "
            "track_id INTEGER NOT NULL, "
            "position INTEGER NOT NULL, "
            "PRIMARY KEY (playlist_id, track_id), "
            "FOREIGN KEY (playlist_id) REFERENCES playlists(id) ON DELETE CASCADE, "
            "FOREIGN KEY (track_id) REFERENCES tracks(id) ON DELETE CASCADE"
            ");";

        char* errMsg = nullptr;
        if(sqlite3_exec(db, sql_tracks.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK ||
            sqlite3_exec(db, sql_fts.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK ||
            sqlite3_exec(db, sql_triggers.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK ||
            sqlite3_exec(db, sql_albums.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK ||
            sqlite3_exec(db, sql_artists.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK ||
            sqlite3_exec(db, sql_album_artists.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK ||
            sqlite3_exec(db, sql_track_artists.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK ||
            sqlite3_exec(db, sql_playlists.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK ||    
            sqlite3_exec(db, sql_playlist_tracks.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK
        ) {
                std::cerr <<"Oops! An error has occured while trying to create tables: " << errMsg << std::endl;
                sqlite3_free(errMsg);
                return false;
        }
        return true;
    }

    bool insertIntoDatabase(sqlite3_stmt* stmt, const Track& track) {

        sqlite3_bind_text(stmt, 1, track.title.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, track.artist.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, track.album.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, track.path.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, track.cover_path.c_str(), -1, SQLITE_TRANSIENT);
        
        bool success = sqlite3_step(stmt) == SQLITE_DONE;
        
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);

        return success;
    }

    bool isAudioFile(const std::string& path) {
        std::filesystem::path p(path);

        auto ext = p.extension().string();

        return ext == ".mp3" ||
            ext == ".flac" ||
            ext == ".wav";
    }

    bool readMetadata(const std::string& path, Track& track) {
        TagLib::FileRef file(path.c_str());

        if(file.isNull() || !file.tag()) {
            std::cerr << "Invalid file!" << std::endl;
            return false;
        }

        auto* tag = file.tag();
        
        track.title = tag->title().isEmpty() ? std::filesystem::path(path).stem().string() : tag->title().toCString(true);
        track.artist = tag->artist().isEmpty() ? "Unknown Artist" : tag->artist().toCString(true);
        track.album = tag->album().isEmpty() ? "Unknown Album" : tag->album().toCString(true);

        return true;
    }

    std::string getFilters(FilterBy type) {
        std::string column;
        switch (type) {
            case FilterBy::Album:
                column ="album";
                break;
            case FilterBy::Artist:
                column = "artist";
                break;
            case FilterBy::Title:
                column = "title";
                break;
            case FilterBy::Favorite:
                column = "is_favorite";
                break;
            default:
                break;
        }

        return column;
    }
};