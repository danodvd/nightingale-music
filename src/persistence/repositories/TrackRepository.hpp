#pragma once

#include "Repository.hpp"
#include "sqlite3.h"
#include "../../domain/models/Track.hpp"
#include "../DatabaseManager.hpp"
#include <cstddef>
#include <optional>
#include <vector>

struct Track;

class TrackRepository : public Repository<Track> {
    public:
    explicit TrackRepository(DatabaseManager& db);
    ~TrackRepository();

    TrackRepository(const TrackRepository&) = delete;
    TrackRepository& operator=(TrackRepository&) = delete;

    /**
     * @brief Creates the Tracks table if it does not already exist.
     * @pre db_.isOpen()
     * @post Returns true iff the Tracks schema is ready.
     */
    bool initSchema() override;

    /**
    * @pre Track != nullptr
    * @post Converts the 'Track' struct in a database Track entity and inserts it.
    */
    bool insert(Track& track) override;

    /**
    * @pre Track_id > 0
    * @post Removes the Track with the matching Track_id from the database.
    */
    bool deleteById(int64_t track_id) override;

    /**
    * @pre 'Track' != nullptr && 'Track' already exists in database
    * @post Modifies the Track with the matching Track_id with the new 'Track' parameter's values.
    */
    bool update(const Track& track) override;

    bool recordPlay(int64_t trackId, int64_t unixTimestamp);

    bool setFavorite(int64_t trackId, bool favorite);

    /**
    * @pre Track_id > 0
    * @post returns the Track with matching Track_id in a struct.
    */
    std::optional<Track> getById(int64_t track_id) const override;

    /**
     * @brief Retrieves all Tracks stored in the library.
     * @pre db_.isOpen()
     */
    std::vector<Track> getAll() const override;

    std::vector<Track> getRecentlyAdded(size_t limit = 10) const;

    std::vector<Track> getRecentlyPlayed(size_t limit = 10) const;

    std::vector<Track> getMostPlayed(size_t limit = 10) const;

    std::vector<Track> getFavorites() const;

    private:
};
