#pragma once

#include "../miniaudio.h"
#include <string>

class AudioManager {
public:
    AudioManager();

    ~AudioManager();

    bool playTrack(const std::string& path);

    void togglePlay();

    bool isPlaying() const;

    float getTotalLengthSecs() const;

    float getCurrentPositionSecs() const;

    float getProgress() const;

private:
    ma_engine engine;
    ma_sound sound;
    bool is_loaded = false;
};