#pragma once

#include "Repository.hpp"
#include "sqlite3.h"
#include "../../domain/models/Album.hpp"
#include "../DatabaseManager.hpp"
#include <optional>
#include <vector>

struct Album;

class AlbumRepository : public Repository<Album>{
    public:
    explicit AlbumRepository(DatabaseManager& db);
    ~AlbumRepository();

    AlbumRepository(const AlbumRepository&) = delete;
    AlbumRepository& operator=(AlbumRepository&) = delete;

    /**
     * @brief Creates the albums table if it does not already exist.
     * @pre db_.isOpen()
     * @post Returns true iff the albums schema is ready.
     */
    bool initSchema() override;

    /**
    * @pre album != nullptr
    * @post Converts the 'album' struct in a database Album entity and inserts it.
    */
    bool insert(Album& album) override;

    /**
    * @pre album_id > 0
    * @post Removes the album with the matching album_id from the database.
    */
    bool deleteById(int64_t album_id) override;

    /**
    * @pre 'album' != nullptr && 'album' already exists in database
    * @post Modifies the album with the matching album_id with the new 'album' parameter's values.
    */
    bool update(const Album& album) override;

    /**
    * @pre album_id > 0
    * @post returns the album with matching album_id in a struct.
    */
    std::optional<Album> getById(int64_t album_id) const override;

    /**
     * @brief Retrieves all albums stored in the library.
     * @pre db_.isOpen()
     */
    std::vector<Album> getAll() const override;

    bool syncRelations(int64_t albumId, const std::vector<int64_t>& artistIds, const std::vector<int64_t>& trackIds);



    private:
};