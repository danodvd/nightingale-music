#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct Artist {
    int64_t id;
    std::string name = "Unknown Artist.";
    std::string coverPath = "";
    std::string bio;
    std::vector<int64_t> albums;
    std::vector<int64_t> tracks; 
};