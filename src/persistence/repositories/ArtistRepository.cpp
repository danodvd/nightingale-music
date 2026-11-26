#include "ArtistRepository.hpp"
#include <cassert>
#include <cstdint>
#include <sqlite3.h>
#include <unordered_map>

ArtistRepository::ArtistRepository(DatabaseManager& db) : Repository<Artist>(db) {}
ArtistRepository::~ArtistRepository() = default;

bool ArtistRepository::initSchema() {
    assert(db_.isOpen());
    if (!db_.isOpen()) return false;

    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS artist (
            artist_id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            coverPath TEXT NOT NULL,
            bio TEXT DEFAULT 'No info.'
        );

        CREATE TABLE IF NOT EXISTS artist_track (
            artist_id INTEGER NOT NULL,
            track_id INTEGER NOT NULL,
            PRIMARY KEY (artist_id, track_id),
            FOREIGN KEY (artist_id) REFERENCES artist(artist_id) ON DELETE CASCADE,
            FOREIGN KEY (track_id) REFERENCES track(track_id) ON DELETE CASCADE
        );
    )";

    return db_.exec(sql);
}

bool ArtistRepository::insert(Artist& artist) {
    assert(db_.isOpen());
    if (!db_.isOpen() || !db_.exec("BEGIN TRANSACTION;")) return false;

    sqlite3_stmt* stmt = db_.prepare("INSERT INTO artist (name, coverPath, bio) VALUES (?, ?, ?);");
    if (!stmt) {
        db_.exec("ROLLBACK;");
        return false;
    }

    sqlite3_bind_text(stmt, 1, artist.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, artist.coverPath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, artist.bio.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        db_.exec("ROLLBACK;");
        return false;
    }
    sqlite3_finalize(stmt);

    sqlite3_stmt* idStmt = db_.prepare("SELECT last_insert_rowid();");
    if (idStmt && sqlite3_step(idStmt) == SQLITE_ROW) {
        artist.id = sqlite3_column_int64(idStmt, 0);
    }
    if (idStmt) sqlite3_finalize(idStmt);

    if (!syncRelations(artist.id,artist.tracks)) {
        db_.exec("ROLLBACK;");
        return false;
    }

    return db_.exec("COMMIT;");
}

bool ArtistRepository::update(const Artist& artist) {
    assert(db_.isOpen() && artist.id > 0);
    if (!db_.isOpen() || artist.id <= 0) return false;

    sqlite3_stmt* stmt = db_.prepare("UPDATE artist SET name = ?, coverPath = ?, bio = ? WHERE artist_id = ?;");
    if (!stmt) return false;

    sqlite3_bind_text(stmt, 1, artist.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, artist.coverPath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, artist.bio.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, artist.id);

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

bool ArtistRepository::deleteById(int64_t artist_id) {
    assert(db_.isOpen() && artist_id > 0);
    if (!db_.isOpen() || artist_id <= 0) return false;

    sqlite3_stmt* stmt = db_.prepare("DELETE FROM artist WHERE artist_id = ?;");
    if (!stmt) return false;

    sqlite3_bind_int64(stmt, 1, artist_id);

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

std::optional<Artist> ArtistRepository::getById(int64_t artist_id) const {
    assert(db_.isOpen() && artist_id > 0);
    if (!db_.isOpen() || artist_id <= 0) return std::nullopt;

    sqlite3_stmt* stmt = db_.prepare("SELECT artist_id, name, coverPath, bio FROM artist WHERE artist_id = ?;");
    if (!stmt) return std::nullopt;

    std::unordered_map<int64_t, std::vector<int64_t>> tracksByArtist;
    if (sqlite3_stmt* relStmt = db_.prepare("SELECT artist_id, track_id FROM artist_track;"); relStmt) {
        while (sqlite3_step(relStmt) == SQLITE_ROW) {
            tracksByArtist[sqlite3_column_int64(relStmt, 0)].push_back(sqlite3_column_int64(relStmt, 1));
        }
        sqlite3_finalize(relStmt);
    }

    sqlite3_bind_int64(stmt, 1, artist_id);

    std::optional<Artist> result = std::nullopt;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        Artist artist{};
        artist.id = sqlite3_column_int64(stmt, 0);

        const auto* nameText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        artist.name = nameText ? nameText : "";

        const auto* coverText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        artist.coverPath = coverText ? coverText : "";

        const auto* bio = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        artist.bio = bio;

        if (auto it = tracksByArtist.find(artist.id); it != tracksByArtist.end()) {
            artist.tracks = std::move(it->second);
        }

        result = std::move(artist);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<Artist> ArtistRepository::getAll() const {
    assert(db_.isOpen());
    std::vector<Artist> artists;
    if (!db_.isOpen()) return artists;

    sqlite3_stmt* stmt = db_.prepare("SELECT artist_id, name, coverPath, bio FROM artist ORDER BY name ASC;");
    if (!stmt) return artists;

    std::unordered_map<int64_t, std::vector<int64_t>> tracksByArtist;
    if (sqlite3_stmt* relStmt = db_.prepare("SELECT artist_id, track_id FROM artist_track;"); relStmt) {
        while (sqlite3_step(relStmt) == SQLITE_ROW) {
            tracksByArtist[sqlite3_column_int64(relStmt, 0)].push_back(sqlite3_column_int64(relStmt, 1));
        }
        sqlite3_finalize(relStmt);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Artist artist{};
        artist.id = sqlite3_column_int64(stmt, 0);

        const auto* nameText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        artist.name = nameText ? nameText : "";

        const auto* coverText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        artist.coverPath = coverText ? coverText : "";

        const auto* bio = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        artist.bio = bio ? bio : "";

        if (auto it = tracksByArtist.find(artist.id); it != tracksByArtist.end()) {
            artist.tracks = std::move(it->second);
        }
        
        artists.push_back(std::move(artist));
    }

    sqlite3_finalize(stmt);
    return artists;
}

bool ArtistRepository::syncRelations(int64_t artist_id, const std::vector<int64_t>& trackIds) {
    sqlite3_stmt* delTracks = db_.prepare("DELETE FROM artist_track WHERE artist_id = ?;");
    if(!delTracks) return false;
    sqlite3_bind_int64(delTracks, 1, artist_id);
    bool ok = (sqlite3_step(delTracks) == SQLITE_DONE);
    sqlite3_finalize(delTracks);
    if(!ok) return false;

    sqlite3_stmt* insTrack = db_.prepare("INSERT OR IGNORE INTO artist_track (artist_id, track_id) VALUES (?, ?);");
    if(!insTrack) return false;
    for(int64_t trackId : trackIds) {
        sqlite3_bind_int64(insTrack, 1, artist_id);
        sqlite3_bind_int64(insTrack, 2, trackId);
        if(sqlite3_step(insTrack) != SQLITE_DONE) {
            sqlite3_finalize(insTrack);
            return false;
        }
        sqlite3_reset(insTrack);
    }
    sqlite3_finalize(insTrack);
    return true;
}

std::vector<ArtistPlayStats> ArtistRepository::getTopArtists(size_t limit) const {
    assert(db_.isOpen());
    std::vector<ArtistPlayStats> result;
    if (!db_.isOpen() || limit == 0) return result;

    const std::string sql = R"(
        SELECT
            a.artist_id,
            a.name,
            COALESCE(NULLIF(a.coverPath, ''), MAX(NULLIF(t.coverPath, '')), '') AS effectiveCover,
            a.bio,
            SUM(t.playCount) AS totalPlays
        FROM artist a
        INNER JOIN artist_track at ON a.artist_id = at.artist_id
        INNER JOIN track t ON at.track_id = t.track_id
        WHERE t.playCount > 0
        GROUP BY a.artist_id, a.name, a.coverPath, a.bio
        ORDER BY totalPlays DESC, a.name ASC
        LIMIT ?;
    )";

    sqlite3_stmt* stmt = db_.prepare(sql);
    if (!stmt) return result;

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(limit));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ArtistPlayStats entry{};
        entry.artist.id = sqlite3_column_int64(stmt, 0);

        const auto* nameText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        entry.artist.name = nameText ? nameText : "";

        const auto* coverText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        entry.artist.coverPath = coverText ? coverText : "";

        const auto* bioText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        entry.artist.bio = bioText ? bioText : "";

        entry.totalPlays = static_cast<uint32_t>(sqlite3_column_int64(stmt, 4));
        result.push_back(std::move(entry));
    }

    sqlite3_finalize(stmt);
    return result;
}