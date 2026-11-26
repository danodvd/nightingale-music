#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct Track {
    int64_t id;
    uint32_t trackNumber = 0;
    uint32_t discNumber = 1;
    std::string name = "Untitled Track";
    std::string coverPath = "";
    std::string filePath;
    uint32_t playCount = 0;
    int64_t lastPlayedAt = 0; // Unix epoch in seconds
    bool isFavorite = false;
    std::vector<int64_t> artists;
    std::vector<int64_t> albums;
    uint32_t durationMs; 
};