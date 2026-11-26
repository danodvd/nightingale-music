#pragma once

#include <sqlite3.h>
#include <string>

class DatabaseManager {
    public:
    DatabaseManager() = default;
    ~DatabaseManager() { close(); }

    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    /**
    @brief Opens the local SQLITE database
    @pre path != null
    @post Returns true iff the database exists && could be opened.
    */
    bool open(const std::string& path);

    /**
    @brief Closes the currently open database
    @post  if(db) then close_database(db) && db = nullptr
     */
    bool close();

    /**
    @brief Executes a raw SQL statement without result rows (DDL, PRAGMAs, transactions).
    @pre sql != null
    @post returns true if the statement executed without errors.
    */
    bool exec(const std::string& sql);

    /**
     * @brief Prepares a parameterized SQL statement for repositories.
     * @pre db != nullptr && !sql.empty()
     * @post Returns a valid sqlite3_stmt* on success, or nullptr on failure.
     */
    sqlite3_stmt* prepare(const std::string& sql) const;

    bool isOpen() const { return db != nullptr; }

    private:
    sqlite3* db = nullptr;
};