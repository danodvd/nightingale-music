#pragma once

#include "Repository.hpp"
#include "../../domain/models/Playlist.hpp"
#include "../DatabaseManager.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

struct Playlist;

class PlaylistRepository : public Repository<Playlist>{
    public:
    explicit PlaylistRepository(DatabaseManager& db);
    ~PlaylistRepository();

    PlaylistRepository(const PlaylistRepository&) = delete;
    PlaylistRepository& operator=(PlaylistRepository&) = delete;

    /**
     * @brief Creates the Playlists table if it does not already exist.
     * @pre db_.isOpen()
     * @post Returns true iff the Playlists schema is ready.
     */
    bool initSchema() override;

    /**
    * @pre Playlist != nullptr
    * @post Converts the 'Playlist' struct in a database Playlist entity and inserts it.
    */
    bool insert(Playlist& playlist) override;

    /**
    * @pre Playlist_id > 0
    * @post Removes the Playlist with the matching Playlist_id from the database.
    */
    bool deleteById(int64_t playlist_id) override;

    /**
    * @pre 'Playlist' != nullptr && 'Playlist' already exists in database
    * @post Modifies the Playlist with the matching Playlist_id with the new 'Playlist' parameter's values.
    */
    bool update(const Playlist& playlist) override;

    /**
    * @pre Playlist_id > 0
    * @post returns the Playlist with matching Playlist_id in a struct.
    */
    std::optional<Playlist> getById(int64_t playlist_id) const override;

    /**
     * @brief Retrieves all Playlists stored in the library.
     * @pre db_.isOpen()
     */
    std::vector<Playlist> getAll() const override;

    /**
    * @brief Adds the track matching track_id to the playlist matching playlist_id
    * @pre playlist_id, track_id >
    * @post Returns true iff insertion on playlist_track was succesful
    */
    bool addTrack(int64_t playlist_id, int64_t track_id);

    /**
    * @brief Removes the track matching track_id to the playlist matching playlist_id
    * @pre playlist_id, track_id > 0
    * @post Returns true iff deletion on playlist_track was succesful
    */
    bool removeTrack(int64_t playlist_id, int64_t track_id);

    bool updateTrackPos(int64_t playlist_id, size_t from, size_t to);

    /**
    * @brief Returns all the tracks contained in playlist matching playlist_id
    * @pre playlist_id > 0
    * @post Returns all track_id in the specified playlist
    */
    std::vector<int64_t> getTrackIds(int64_t playlist_id) const;

    private:
};