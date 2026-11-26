#include "DatabaseManager.hpp"
#include <iostream>
#include <sqlite3.h>

bool DatabaseManager::open(const std::string& path) {
    if (db != nullptr) {
        std::cerr << "Database is already open.\n";
        return false;
    }
    int result = sqlite3_open_v2(
        path.c_str(),
        &db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
        nullptr
    );

    if (result != SQLITE_OK) {
        std::cerr << "Unable to open database: "
                  << sqlite3_errmsg(db) << '\n';

        sqlite3_close(db);
        db = nullptr;

        return false;
    }

    if(!exec("PRAGMA foreign_keys = ON;")) {
        close();
        return false;
    }

    return true;
}

bool DatabaseManager::close() {
    if(db == nullptr) return false;

    int result = sqlite3_close(db);

    if(result != SQLITE_OK) {
        std::cerr << "Unable to close database: " 
                  << sqlite3_errmsg(db) << '\n';
        return false;
    }

    db = nullptr;
    return true;
}

bool DatabaseManager::exec(const std::string& sql) {
    char* errorMsg = nullptr;

    int result = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errorMsg);

    if(result != SQLITE_OK) {
        std::cerr << "SQL execution failed: " 
                  << (errorMsg ? errorMsg : sqlite3_errmsg(db))
                  << '\n';
        sqlite3_free(errorMsg);
        return false;
    }   

    return true;
}

sqlite3_stmt* DatabaseManager::prepare(const std::string& sql) const{
    sqlite3_stmt* stmt = nullptr;

    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);

    if(result != SQLITE_OK) {
        std::cerr << "Unable to prepare statement: " 
                  << sqlite3_errmsg(db)
                  << '\n';
        return nullptr;
    }
    return stmt;
}
