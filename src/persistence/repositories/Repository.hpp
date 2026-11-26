#pragma once

#include "../DatabaseManager.hpp"
#include <optional>
#include <vector>

template <typename T>
class Repository {
public:

    virtual ~Repository() = default;

    virtual bool initSchema() = 0;

    virtual bool insert(T& entity) = 0;

    virtual bool deleteById(int64_t id) = 0;

    virtual bool update(const T& entity) = 0;

    virtual std::optional<T> getById(int64_t id) const = 0;

    virtual std::vector<T> getAll() const = 0;

protected:
    DatabaseManager& db_;

    Repository(DatabaseManager& db) : db_(db) {}
};