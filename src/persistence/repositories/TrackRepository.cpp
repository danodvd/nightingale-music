#include "TrackRepository.hpp"

#include <cassert>
#include <cstdint>
#include <sqlite3.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

void hydrateRelations(DatabaseManager& db, std::vector<Track>& tracks) {
    if (tracks.empty()) return;

    std::unordered_map<int64_t, size_t> indexByTrackId;
    indexByTrackId.reserve(tracks.size());
    for (size_t i = 0; i < tracks.size(); ++i) {
        indexByTrackId.emplace(tracks[i].id, i);
    }

    auto loadRelation = [&](const char* tableName, const char* idColumn, auto memberPtr) {
        const bool useInClause = (tracks.size() <= 500);
        std::string sql = std::string("SELECT track_id, ") + idColumn + " FROM " + tableName;
        if (useInClause) {
            sql += " WHERE track_id IN (";
            for (size_t i = 0; i < tracks.size(); ++i) {
                if (i > 0) sql += ',';
                sql += '?';
            }
            sql += ')';
        }
        sql += ';';

        sqlite3_stmt* stmt = db.prepare(sql);
        if (!stmt) return;

        if (useInClause) {
            for (size_t i = 0; i < tracks.size(); ++i) {
                sqlite3_bind_int64(stmt, static_cast<int>(i + 1), tracks[i].id);
            }
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const int64_t trackId = sqlite3_column_int64(stmt, 0);
            const int64_t relatedId = sqlite3_column_int64(stmt, 1);
            if (auto it = indexByTrackId.find(trackId); it != indexByTrackId.end()) {
                (tracks[it->second].*memberPtr).push_back(relatedId);
            }
        }
        sqlite3_finalize(stmt);
    };

    loadRelation("artist_track", "artist_id", &Track::artists);
    loadRelation("album_track", "album_id", &Track::albums);
}

template <typename Binder>
std::vector<Track> queryTracks(DatabaseManager& db, const std::string& sql, Binder&& binder) {
    std::vector<Track> tracks;
    sqlite3_stmt* stmt = db.prepare(sql);
    if (!stmt) return tracks;

    binder(stmt);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Track track{};
        track.id = sqlite3_column_int64(stmt, 0);

        const auto* titleText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        track.name = titleText ? titleText : "";

        const auto* coverText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        track.coverPath = coverText ? coverText : "";

        const auto* filePathText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        track.filePath = filePathText ? filePathText : "";

        track.durationMs = static_cast<uint32_t>(sqlite3_column_int64(stmt, 4));
        track.playCount = static_cast<uint32_t>(sqlite3_column_int64(stmt, 5));
        track.lastPlayedAt = sqlite3_column_int64(stmt, 6);
        track.isFavorite = (sqlite3_column_int(stmt, 7) != 0);

        tracks.push_back(std::move(track));
    }

    sqlite3_finalize(stmt);
    hydrateRelations(db, tracks);
    return tracks;
}

constexpr const char* kSelectTrackColumns =
    "SELECT track_id, title, coverPath, filePath, durationMs, playCount, lastPlayedAt, isFavorite FROM track ";

} // namespace

TrackRepository::TrackRepository(DatabaseManager& db) : Repository<Track>(db) {}
TrackRepository::~TrackRepository() = default;

bool TrackRepository::initSchema() {
    assert(db_.isOpen());
    if (!db_.isOpen()) return false;

    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS track (
            track_id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT DEFAULT 'Untitled Track',
            coverPath TEXT,
            filePath TEXT NOT NULL UNIQUE,
            durationMs INTEGER DEFAULT 0,
            playCount INTEGER DEFAULT 0,
            lastPlayedAt INTEGER DEFAULT 0,
            isFavorite INTEGER DEFAULT 0
        );
    )";

    if (!db_.exec(sql)) return false;

    std::unordered_set<std::string> existingCols;
    if (sqlite3_stmt* infoStmt = db_.prepare("PRAGMA table_info(track);"); infoStmt) {
        while (sqlite3_step(infoStmt) == SQLITE_ROW) {
            const auto* colName = reinterpret_cast<const char*>(sqlite3_column_text(infoStmt, 1));
            if (colName) {
                existingCols.emplace(colName);
            }
        }
        sqlite3_finalize(infoStmt);
    }

    if (!existingCols.contains("playCount") &&
        !db_.exec("ALTER TABLE track ADD COLUMN playCount INTEGER DEFAULT 0;")) {
        return false;
    }
    if (!existingCols.contains("lastPlayedAt") &&
        !db_.exec("ALTER TABLE track ADD COLUMN lastPlayedAt INTEGER DEFAULT 0;")) {
        return false;
    }
    if (!existingCols.contains("isFavorite") &&
        !db_.exec("ALTER TABLE track ADD COLUMN isFavorite INTEGER DEFAULT 0;")) {
        return false;
    }

    return true;
}

bool TrackRepository::insert(Track& track) {
    assert(db_.isOpen());
    if (!db_.isOpen()) return false;

    sqlite3_stmt* stmt = db_.prepare(
        "INSERT INTO track (title, coverPath, filePath, durationMs) VALUES (?, ?, ?, ?);"
    );
    if (!stmt) return false;

    sqlite3_bind_text(stmt, 1, track.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, track.coverPath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, track.filePath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, track.durationMs);

    const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);

    if (ok) {
        sqlite3_stmt* idStmt = db_.prepare("SELECT last_insert_rowid();");
        if (idStmt) {
            if (sqlite3_step(idStmt) == SQLITE_ROW) {
                track.id = sqlite3_column_int64(idStmt, 0);
            }
            sqlite3_finalize(idStmt);
        }
    }

    return ok;
}

bool TrackRepository::update(const Track& track) {
    assert(db_.isOpen() && track.id > 0);
    if (!db_.isOpen() || track.id <= 0) return false;

    sqlite3_stmt* stmt = db_.prepare(
        "UPDATE track SET title = ?, coverPath = ?, filePath = ?, durationMs = ? WHERE track_id = ?;"
    );
    if (!stmt) return false;

    sqlite3_bind_text(stmt, 1, track.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, track.coverPath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, track.filePath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, track.durationMs);
    sqlite3_bind_int64(stmt, 5, track.id);

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

bool TrackRepository::deleteById(int64_t track_id) {
    assert(db_.isOpen() && track_id > 0);
    if (!db_.isOpen() || track_id <= 0) return false;

    sqlite3_stmt* stmt = db_.prepare("DELETE FROM track WHERE track_id = ?;");
    if (!stmt) return false;

    sqlite3_bind_int64(stmt, 1, track_id);

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

std::optional<Track> TrackRepository::getById(int64_t track_id) const {
    assert(db_.isOpen() && track_id > 0);
    if (!db_.isOpen() || track_id <= 0) return std::nullopt;

    auto tracks = queryTracks(
        db_,
        std::string(kSelectTrackColumns) + "WHERE track_id = ?;",
        [track_id](sqlite3_stmt* stmt) {
            sqlite3_bind_int64(stmt, 1, track_id);
        }
    );

    if (tracks.empty()) return std::nullopt;
    return std::move(tracks.front());
}

std::vector<Track> TrackRepository::getAll() const {
    assert(db_.isOpen());
    if (!db_.isOpen()) return {};

    return queryTracks(
        db_,
        std::string(kSelectTrackColumns) + "ORDER BY title ASC;",
        [](sqlite3_stmt*) {}
    );
}

std::vector<Track> TrackRepository::getRecentlyAdded(size_t limit) const {
    assert(db_.isOpen());
    if (!db_.isOpen() || limit == 0) return {};

    // track_id usa AUTOINCREMENT, por lo que los IDs más altos son los últimos añadidos
    return queryTracks(
        db_,
        std::string(kSelectTrackColumns) + "ORDER BY track_id DESC LIMIT ?;",
        [limit](sqlite3_stmt* stmt) {
            sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(limit));
        }
    );
}

std::vector<Track> TrackRepository::getRecentlyPlayed(size_t limit) const {
    assert(db_.isOpen());
    if (!db_.isOpen() || limit == 0) return {};

    return queryTracks(
        db_,
        std::string(kSelectTrackColumns) +
            "WHERE lastPlayedAt > 0 ORDER BY lastPlayedAt DESC, track_id DESC LIMIT ?;",
        [limit](sqlite3_stmt* stmt) {
            sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(limit));
        }
    );
}

std::vector<Track> TrackRepository::getMostPlayed(size_t limit) const {
    assert(db_.isOpen());
    if (!db_.isOpen() || limit == 0) return {};

    return queryTracks(
        db_,
        std::string(kSelectTrackColumns) +
            "WHERE playCount > 0 ORDER BY playCount DESC, lastPlayedAt DESC, title ASC LIMIT ?;",
        [limit](sqlite3_stmt* stmt) {
            sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(limit));
        }
    );
}

std::vector<Track> TrackRepository::getFavorites() const {
    assert(db_.isOpen());
    if (!db_.isOpen()) return {};

    return queryTracks(
        db_,
        std::string(kSelectTrackColumns) + "WHERE isFavorite = 1 ORDER BY title ASC;",
        [](sqlite3_stmt*) {}
    );
}

bool TrackRepository::recordPlay(int64_t trackId, int64_t unixTimestamp) {
    assert(db_.isOpen() && trackId > 0);
    if (!db_.isOpen() || trackId <= 0) return false;

    sqlite3_stmt* stmt = db_.prepare(
        "UPDATE track SET playCount = playCount + 1, lastPlayedAt = ? WHERE track_id = ?;"
    );
    if (!stmt) return false;

    sqlite3_bind_int64(stmt, 1, unixTimestamp);
    sqlite3_bind_int64(stmt, 2, trackId);
    const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool TrackRepository::setFavorite(int64_t trackId, bool favorite) {
    assert(db_.isOpen() && trackId > 0);
    if (!db_.isOpen() || trackId <= 0) return false;

    sqlite3_stmt* stmt = db_.prepare(
        "UPDATE track SET isFavorite = ? WHERE track_id = ?;"
    );
    if (!stmt) return false;

    sqlite3_bind_int(stmt, 1, favorite ? 1 : 0);
    sqlite3_bind_int64(stmt, 2, trackId);
    const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}