#pragma once

#include "Repository.hpp"
#include "sqlite3.h"
#include "../../domain/models/Artist.hpp"
#include "../DatabaseManager.hpp"
#include <cstdint>
#include <optional>
#include <vector>

struct Artist;

struct ArtistPlayStats {
    Artist artist;
    uint32_t totalPlays = 0;
};

class ArtistRepository : public Repository<Artist> {
    public:
    explicit ArtistRepository(DatabaseManager& db);
    ~ArtistRepository();

    ArtistRepository(const ArtistRepository&) = delete;
    ArtistRepository& operator=(ArtistRepository&) = delete;

    /**
     * @brief Creates the Artists table if it does not already exist.
     * @pre db_.isOpen()
     * @post Returns true iff the Artists schema is ready.
     */
    bool initSchema() override;

    /**
    * @pre Artist != nullptr
    * @post Converts the 'Artist' struct in a database Artist entity and inserts it.
    */
    bool insert(Artist& artist) override;

    /**
    * @pre Artist_id > 0
    * @post Removes the Artist with the matching Artist_id from the database.
    */
    bool deleteById(int64_t artist_id) override;

    /**
    * @pre 'Artist' != nullptr && 'Artist' already exists in database
    * @post Modifies the Artist with the matching Artist_id with the new 'Artist' parameter's values.
    */
    bool update(const Artist& artist) override;

    /**
    * @pre Artist_id > 0
    * @post returns the Artist with matching Artist_id in a struct.
    */
    std::optional<Artist> getById(int64_t artist_id) const override;

    /**
     * @brief Retrieves all Artists stored in the library.
     * @pre db_.isOpen()
     */
    std::vector<Artist> getAll() const override;

    bool syncRelations(int64_t artistId, const std::vector<int64_t>& trackIds);

    [[nodiscard]] std::vector<ArtistPlayStats> getTopArtists(size_t limit = 10) const;

    private:
};