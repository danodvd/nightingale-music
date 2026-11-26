#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct Album {
    int64_t id;
    std::string name = "Unknown Album.";
    std::string coverPath = "";
    std::optional<int> year;
    std::vector<int64_t> artists;
    std::vector<int64_t> tracks; 
};