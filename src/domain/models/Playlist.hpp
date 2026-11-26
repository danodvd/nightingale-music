#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct Playlist {
    int64_t id;
    std::string name = "My Awesome Playlist";
    std::string coverPath = "";
    std::string description;
    long durationSecs = 0L;
    std::vector<int64_t> tracks; 
};