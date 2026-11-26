#include "AlbumRepository.hpp"
#include <cassert>
#include <sqlite3.h>
#include <unordered_map>

AlbumRepository::AlbumRepository(DatabaseManager& db) : Repository<Album>(db) {}
AlbumRepository::~AlbumRepository() = default;

bool AlbumRepository::initSchema() {
    assert(db_.isOpen());
    if (!db_.isOpen()) return false;

    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS album (
            album_id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            coverPath TEXT NOT NULL,
            year INTEGER
        );

        CREATE TABLE IF NOT EXISTS album_artist (
            album_id INTEGER NOT NULL,
            artist_id INTEGER NOT NULL,
            PRIMARY KEY (album_id, artist_id),
            FOREIGN KEY (album_id) REFERENCES album(album_id) ON DELETE CASCADE,
            FOREIGN KEY (artist_id) REFERENCES artist(artist_id) ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS album_track (
            album_id INTEGER NOT NULL,
            track_id INTEGER NOT NULL,
            PRIMARY KEY (album_id, track_id),
            FOREIGN KEY (album_id) REFERENCES album(album_id) ON DELETE CASCADE,
            FOREIGN KEY (track_id) REFERENCES track(track_id) ON DELETE CASCADE
        );
    )";

    return db_.exec(sql);
}

bool AlbumRepository::insert(Album& album) {
    assert(db_.isOpen());
    if (!db_.isOpen() || !db_.exec("BEGIN TRANSACTION;")) return false;

    sqlite3_stmt* stmt = db_.prepare("INSERT INTO album (name, coverPath, year) VALUES (?, ?, ?);");
    if (!stmt) {
        db_.exec("ROLLBACK;");
        return false;
    }

    sqlite3_bind_text(stmt, 1, album.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, album.coverPath.c_str(), -1, SQLITE_TRANSIENT);
    if (album.year.has_value()) sqlite3_bind_int(stmt, 3, *album.year);
    else sqlite3_bind_null(stmt, 3);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        db_.exec("ROLLBACK;");
        return false;
    }
    sqlite3_finalize(stmt);

    sqlite3_stmt* idStmt = db_.prepare("SELECT last_insert_rowid();");
    if (idStmt && sqlite3_step(idStmt) == SQLITE_ROW) {
        album.id = sqlite3_column_int64(idStmt, 0);
    }
    if (idStmt) sqlite3_finalize(idStmt);

    if (!syncRelations(album.id, album.artists, album.tracks)) {
        db_.exec("ROLLBACK;");
        return false;
    }

    return db_.exec("COMMIT;");
}

bool AlbumRepository::update(const Album& album) {
    assert(db_.isOpen() && album.id > 0);
    if (!db_.isOpen() || album.id <= 0) return false;

    sqlite3_stmt* stmt = db_.prepare("UPDATE album SET name = ?, coverPath = ?, year = ? WHERE album_id = ?;");
    if (!stmt) return false;

    sqlite3_bind_text(stmt, 1, album.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, album.coverPath.c_str(), -1, SQLITE_TRANSIENT);
    if (album.year.has_value()) {
        sqlite3_bind_int(stmt, 3, *album.year);
    } else {
        sqlite3_bind_null(stmt, 3);
    }
    sqlite3_bind_int64(stmt, 4, album.id);

    const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    if (!ok) return false;

    int affectedRows = 0;
    sqlite3_stmt* changesStmt = db_.prepare("SELECT changes();");
    if (changesStmt) {
        if (sqlite3_step(changesStmt) == SQLITE_ROW) {
            affectedRows = sqlite3_column_int(changesStmt, 0);
        }
        sqlite3_finalize(changesStmt);
    }

    return affectedRows > 0;
}

bool AlbumRepository::deleteById(int64_t album_id) {
    assert(db_.isOpen() && album_id > 0);
    if (!db_.isOpen() || album_id <= 0) return false;

    sqlite3_stmt* stmt = db_.prepare("DELETE FROM album WHERE album_id = ?;");
    if (!stmt) return false;

    sqlite3_bind_int64(stmt, 1, album_id);

    const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    if (!ok) return false;

    int affectedRows = 0;
    sqlite3_stmt* changesStmt = db_.prepare("SELECT changes();");
    if (changesStmt) {
        if (sqlite3_step(changesStmt) == SQLITE_ROW) {
            affectedRows = sqlite3_column_int(changesStmt, 0);
        }
        sqlite3_finalize(changesStmt);
    }

    return affectedRows > 0;
}

std::optional<Album> AlbumRepository::getById(int64_t album_id) const {
    assert(db_.isOpen() && album_id > 0);
    if (!db_.isOpen() || album_id <= 0) return std::nullopt;

    sqlite3_stmt* stmt = db_.prepare("SELECT album_id, name, coverPath, year FROM album WHERE album_id = ?;");
    if (!stmt) return std::nullopt;

    sqlite3_bind_int64(stmt, 1, album_id);

    std::optional<Album> result = std::nullopt;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        Album album{};
        album.id = sqlite3_column_int64(stmt, 0);

        const auto* nameText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        album.name = nameText ? nameText : "";

        const auto* coverText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        album.coverPath = coverText ? coverText : "";

        if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
            album.year = sqlite3_column_int(stmt, 3);
        }

        if (sqlite3_stmt* relStmt = db_.prepare("SELECT artist_id FROM album_artist WHERE album_id = ?;"); relStmt) {
            sqlite3_bind_int64(relStmt, 1, album.id);
            while (sqlite3_step(relStmt) == SQLITE_ROW) {
                album.artists.push_back(sqlite3_column_int64(relStmt, 0));
            }
            sqlite3_finalize(relStmt);
        }

        if (sqlite3_stmt* relStmt = db_.prepare("SELECT track_id FROM album_track WHERE album_id = ?;"); relStmt) {
            sqlite3_bind_int64(relStmt, 1, album.id);
            while (sqlite3_step(relStmt) == SQLITE_ROW) {
                album.tracks.push_back(sqlite3_column_int64(relStmt, 0));
            }
            sqlite3_finalize(relStmt);
        }

        result = std::move(album);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<Album> AlbumRepository::getAll() const {
    assert(db_.isOpen());
    std::vector<Album> albums;
    if (!db_.isOpen()) return albums;

    std::unordered_map<int64_t, std::vector<int64_t>> artistsByAlbum;
    if (sqlite3_stmt* relStmt = db_.prepare("SELECT album_id, artist_id FROM album_artist;"); relStmt) {
        while (sqlite3_step(relStmt) == SQLITE_ROW) {
            artistsByAlbum[sqlite3_column_int64(relStmt, 0)].push_back(sqlite3_column_int64(relStmt, 1));
        }
        sqlite3_finalize(relStmt);
    }

    std::unordered_map<int64_t, std::vector<int64_t>> tracksByAlbum;
    if (sqlite3_stmt* relStmt = db_.prepare("SELECT album_id, track_id FROM album_track;"); relStmt) {
        while (sqlite3_step(relStmt) == SQLITE_ROW) {
            tracksByAlbum[sqlite3_column_int64(relStmt, 0)].push_back(sqlite3_column_int64(relStmt, 1));
        }
        sqlite3_finalize(relStmt);
    }

    sqlite3_stmt* stmt = db_.prepare("SELECT album_id, name, coverPath, year FROM album ORDER BY name ASC;");
    if (!stmt) return albums;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Album album{};
        album.id = sqlite3_column_int64(stmt, 0);

        const auto* nameText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        album.name = nameText ? nameText : "";

        const auto* coverText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        album.coverPath = coverText ? coverText : "";

        if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
            album.year = sqlite3_column_int(stmt, 3);
        }

        if (auto it = artistsByAlbum.find(album.id); it != artistsByAlbum.end()) {
            album.artists = std::move(it->second);
        }
        if (auto it = tracksByAlbum.find(album.id); it != tracksByAlbum.end()) {
            album.tracks = std::move(it->second);
        }

        albums.push_back(std::move(album));
    }

    sqlite3_finalize(stmt);
    return albums;
}

bool AlbumRepository::syncRelations(int64_t albumId, const std::vector<int64_t>& artistIds, const std::vector<int64_t>& trackIds) {
    sqlite3_stmt* delArtists = db_.prepare("DELETE FROM album_artist WHERE album_id = ?;");
    if (!delArtists) return false;
    sqlite3_bind_int64(delArtists, 1, albumId);
    bool ok = (sqlite3_step(delArtists) == SQLITE_DONE);
    sqlite3_finalize(delArtists);
    if (!ok) return false;

    sqlite3_stmt* delTracks = db_.prepare("DELETE FROM album_track WHERE album_id = ?;");
    if (!delTracks) return false;
    sqlite3_bind_int64(delTracks, 1, albumId);
    ok = (sqlite3_step(delTracks) == SQLITE_DONE);
    sqlite3_finalize(delTracks);
    if (!ok) return false;

    sqlite3_stmt* insArtist = db_.prepare("INSERT OR IGNORE INTO album_artist (album_id, artist_id) VALUES (?, ?);");
    if (!insArtist) return false;
    for (int64_t artistId : artistIds) {
        sqlite3_bind_int64(insArtist, 1, albumId);
        sqlite3_bind_int64(insArtist, 2, artistId);
        if (sqlite3_step(insArtist) != SQLITE_DONE) {
            sqlite3_finalize(insArtist);
            return false;
        }
        sqlite3_reset(insArtist);
    }
    sqlite3_finalize(insArtist);

    sqlite3_stmt* insTrack = db_.prepare("INSERT OR IGNORE INTO album_track (album_id, track_id) VALUES (?, ?);");
    if (!insTrack) return false;
    for (int64_t trackId : trackIds) {
        sqlite3_bind_int64(insTrack, 1, albumId);
        sqlite3_bind_int64(insTrack, 2, trackId);
        if (sqlite3_step(insTrack) != SQLITE_DONE) {
            sqlite3_finalize(insTrack);
            return false;
        }
        sqlite3_reset(insTrack);
    }
    sqlite3_finalize(insTrack);

    return true;
}