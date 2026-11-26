#include "PlaylistRepository.hpp"
#include <cassert>
#include <sqlite3.h>

PlaylistRepository::PlaylistRepository(DatabaseManager& db) : Repository<Playlist>(db) {}
PlaylistRepository::~PlaylistRepository() = default;

bool PlaylistRepository::initSchema() {
    assert(db_.isOpen());
    if (!db_.isOpen()) return false;

    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS playlist (
            playlist_id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            cover_path TEXT NOT NULL,
            duration_secs INTEGER DEFAULT 0
        );
        CREATE TABLE IF NOT EXISTS playlist_track (
            playlist_id INTEGER NOT NULL,
            track_id INTEGER NOT NULL,
            position INTEGER NOT NULL CHECK(position >= 0),

            PRIMARY KEY (playlist_id, track_id),

            UNIQUE(playlist_id, position),

            FOREIGN KEY (playlist_id) REFERENCES playlist(playlist_id) ON DELETE CASCADE,
            FOREIGN KEY (track_id) REFERENCES track(track_id) ON DELETE CASCADE
        );

        CREATE TRIGGER IF NOT EXISTS assign_playlist_track_position
        AFTER INSERT ON playlist_track
        WHEN NEW.position = -1
        BEGIN
            UPDATE playlist_track
            SET position = (
                SELECT COALESCE(MAX(position) + 1, 0)
                FROM playlist_track
                WHERE playlist_id = NEW.playlist_id
                AND track_id != NEW.track_id
            )
            WHERE playlist_id = NEW.playlist_id
            AND track_id = NEW.track_id;
        END;
    )";

    return db_.exec(sql);
}

bool PlaylistRepository::insert(Playlist& playlist) {
    assert(db_.isOpen());
    if (!db_.isOpen()) return false;

    sqlite3_stmt* stmt = db_.prepare("INSERT INTO playlist (name, cover_path, duration_secs) VALUES (?, ?, ?);");
    if (!stmt) return false;

    sqlite3_bind_text(stmt, 1, playlist.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, playlist.coverPath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, playlist.durationSecs);

    const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);

    if (ok) {
        sqlite3_stmt* idStmt = db_.prepare("SELECT last_insert_rowid();");
        if (idStmt) {
            if (sqlite3_step(idStmt) == SQLITE_ROW) {
                playlist.id = sqlite3_column_int64(idStmt, 0);
            }
            sqlite3_finalize(idStmt);
        }
    }

    return ok;
}

bool PlaylistRepository::update(const Playlist& playlist) {
    assert(db_.isOpen() && playlist.id > 0);
    if (!db_.isOpen() || playlist.id <= 0) return false;

    sqlite3_stmt* stmt = db_.prepare("UPDATE playlist SET name = ?, cover_path = ?, duration_secs = ? WHERE playlist_id = ?;");
    if (!stmt) return false;

    sqlite3_bind_text(stmt, 1, playlist.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, playlist.coverPath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, playlist.durationSecs);
    sqlite3_bind_int64(stmt, 4, playlist.id);

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

bool PlaylistRepository::deleteById(int64_t playlist_id) {
    assert(db_.isOpen() && playlist_id > 0);
    if (!db_.isOpen() || playlist_id <= 0) return false;

    sqlite3_stmt* stmt = db_.prepare("DELETE FROM playlist WHERE playlist_id = ?;");
    if (!stmt) return false;

    sqlite3_bind_int64(stmt, 1, playlist_id);

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

std::optional<Playlist> PlaylistRepository::getById(int64_t playlist_id) const {
    assert(db_.isOpen() && playlist_id > 0);
    if (!db_.isOpen() || playlist_id <= 0) return std::nullopt;

    sqlite3_stmt* stmt = db_.prepare("SELECT playlist_id, name, cover_path, duration_secs FROM playlist WHERE playlist_id = ?;");
    if (!stmt) return std::nullopt;

    sqlite3_bind_int64(stmt, 1, playlist_id);

    std::optional<Playlist> result = std::nullopt;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        Playlist playlist{};
        playlist.id = sqlite3_column_int64(stmt, 0);

        const auto* nameText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        playlist.name = nameText ? nameText : "";

        const auto* coverText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        playlist.coverPath = coverText ? coverText : "";

        playlist.durationSecs = sqlite3_column_int64(stmt, 3);
        result = std::move(playlist);
    }

    sqlite3_finalize(stmt);

    if (result.has_value()) {
        result->tracks = getTrackIds(playlist_id);
    }

    return result;
}

std::vector<Playlist> PlaylistRepository::getAll() const {
    assert(db_.isOpen());
    std::vector<Playlist> playlists;
    if (!db_.isOpen()) return playlists;

    sqlite3_stmt* stmt = db_.prepare("SELECT playlist_id, name, cover_path, duration_secs FROM playlist ORDER BY name ASC;");
    if (!stmt) return playlists;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Playlist playlist{};
        playlist.id = sqlite3_column_int64(stmt, 0);

        const auto* nameText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        playlist.name = nameText ? nameText : "";

        const auto* coverText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        playlist.coverPath = coverText ? coverText : "";

        playlist.durationSecs = sqlite3_column_int64(stmt, 3);
        playlists.push_back(std::move(playlist));
    }

    sqlite3_finalize(stmt);
    for (auto& playlist : playlists) {
        playlist.tracks = getTrackIds(playlist.id);
    }
    return playlists;
}

bool PlaylistRepository::addTrack(int64_t playlist_id, int64_t track_id) {
    assert(db_.isOpen() && playlist_id > 0 && track_id > 0);
    if (!db_.isOpen() || playlist_id <= 0 || track_id <= 0) return false;

    sqlite3_stmt* stmt = db_.prepare(
    "INSERT OR IGNORE INTO playlist_track (playlist_id, track_id, position) "
        "VALUES (?1, ?2, COALESCE((SELECT MAX(position) + 1 FROM playlist_track WHERE playlist_id = ?1), 0));"
    );
    if (!stmt) return false;

    sqlite3_bind_int64(stmt, 1, playlist_id);
    sqlite3_bind_int64(stmt, 2, track_id);

    const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool PlaylistRepository::removeTrack(int64_t playlist_id, int64_t track_id) {
    assert(db_.isOpen() && playlist_id > 0 && track_id > 0);
    if (!db_.isOpen() || playlist_id <= 0 || track_id <= 0) return false;

    sqlite3_stmt* stmt = db_.prepare(
        "DELETE FROM playlist_track WHERE playlist_id = ? AND track_id = ?;"
    );
    if (!stmt) return false;

    sqlite3_bind_int64(stmt, 1, playlist_id);
    sqlite3_bind_int64(stmt, 2, track_id);

    const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool PlaylistRepository::updateTrackPos(int64_t playlist_id, size_t from, size_t to) {
    assert(db_.isOpen() && playlist_id > 0);
    if (!db_.isOpen() || playlist_id <= 0) return false;

    const auto fromPos = static_cast<int64_t>(from);
    const auto toPos = static_cast<int64_t>(to);

    // 1. Verificar que ambas posiciones existan en la playlist
    sqlite3_stmt* checkStmt = db_.prepare(
        "SELECT COUNT(*) FROM playlist_track WHERE playlist_id = ? AND position IN (?, ?);"
    );
    if (!checkStmt) return false;

    sqlite3_bind_int64(checkStmt, 1, playlist_id);
    sqlite3_bind_int64(checkStmt, 2, fromPos);
    sqlite3_bind_int64(checkStmt, 3, toPos);

    int matchCount = 0;
    if (sqlite3_step(checkStmt) == SQLITE_ROW) {
        matchCount = sqlite3_column_int(checkStmt, 0);
    }
    sqlite3_finalize(checkStmt);

    const int expectedMatches = (from == to) ? 1 : 2;
    if (matchCount != expectedMatches) return false;
    if (from == to) return true;

    constexpr int64_t kOffset = 1000000;

    auto execStep = [&](const char* sql, auto&& binder) -> bool {
        sqlite3_stmt* stmt = db_.prepare(sql);
        if (!stmt) return false;
        binder(stmt);
        const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return ok;
    };

    if (!db_.exec("BEGIN IMMEDIATE;")) return false;

    const bool step1 = execStep(
        "UPDATE playlist_track SET position = ? WHERE playlist_id = ? AND position = ?;",
        [&](sqlite3_stmt* s) {
            sqlite3_bind_int64(s, 1, toPos + kOffset);
            sqlite3_bind_int64(s, 2, playlist_id);
            sqlite3_bind_int64(s, 3, fromPos);
        }
    );
    if (!step1) {
        db_.exec("ROLLBACK;");
        return false;
    }

    bool step2 = false;
    if (from < to) {
        step2 = execStep(
            "UPDATE playlist_track SET position = position - 1 + ? "
            "WHERE playlist_id = ? AND position > ? AND position <= ?;",
            [&](sqlite3_stmt* s) {
                sqlite3_bind_int64(s, 1, kOffset);
                sqlite3_bind_int64(s, 2, playlist_id);
                sqlite3_bind_int64(s, 3, fromPos);
                sqlite3_bind_int64(s, 4, toPos);
            }
        );
    } else {
        step2 = execStep(
            "UPDATE playlist_track SET position = position + 1 + ? "
            "WHERE playlist_id = ? AND position >= ? AND position < ?;",
            [&](sqlite3_stmt* s) {
                sqlite3_bind_int64(s, 1, kOffset);
                sqlite3_bind_int64(s, 2, playlist_id);
                sqlite3_bind_int64(s, 3, toPos);
                sqlite3_bind_int64(s, 4, fromPos);
            }
        );
    }
    if (!step2) {
        db_.exec("ROLLBACK;");
        return false;
    }

    const bool step3 = execStep(
        "UPDATE playlist_track SET position = position - ? "
        "WHERE playlist_id = ? AND position >= ?;",
        [&](sqlite3_stmt* s) {
            sqlite3_bind_int64(s, 1, kOffset);
            sqlite3_bind_int64(s, 2, playlist_id);
            sqlite3_bind_int64(s, 3, kOffset);
        }
    );
    if (!step3) {
        db_.exec("ROLLBACK;");
        return false;
    }

    if (!db_.exec("COMMIT;")) {
        db_.exec("ROLLBACK;");
        return false;
    }

    return true;
}

std::vector<int64_t> PlaylistRepository::getTrackIds(int64_t playlist_id) const {
    std::vector<int64_t> ids;
    if (!db_.isOpen() || playlist_id <= 0) return ids;

    sqlite3_stmt* stmt = db_.prepare(
        "SELECT track_id FROM playlist_track WHERE playlist_id = ? ORDER BY position ASC;"
    );
    if (!stmt) return ids;

    sqlite3_bind_int64(stmt, 1, playlist_id);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ids.push_back(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return ids;
}