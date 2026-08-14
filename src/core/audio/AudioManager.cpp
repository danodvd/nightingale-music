#define MINIAUDIO_IMPLEMENTATION

#include "AudioManager.hpp"
#include <iostream>

AudioManager::AudioManager() {
    if(ma_engine_init(NULL, &engine) != MA_SUCCESS) {
        std::cerr << "Failed to initialize audio engine." << std::endl; 
    }
}

AudioManager::~AudioManager() {
    if(is_loaded) ma_sound_uninit(&sound);
    ma_engine_uninit(&engine);
}

bool AudioManager::playTrack(const std::string& path) {
    if (path.empty()) return false;

    if (is_loaded) {
        ma_sound_stop(&sound);   // Stop before uninit to prevent thread locks
        ma_sound_uninit(&sound);
        is_loaded = false;
    }

    if (ma_sound_init_from_file(&engine, path.c_str(), 0, nullptr, nullptr, &sound) == MA_SUCCESS) {
        is_loaded = true;
        ma_sound_start(&sound);
        return true;
    }
    
    std::cerr << "Failed to load track: " << path << std::endl;
    return false;
}

void AudioManager::togglePlay() {
    if(!is_loaded) return;
    if(ma_sound_is_playing(&sound)) {
        ma_sound_stop(&sound);
    }
    else {
        ma_sound_start(&sound);
    }
}

bool AudioManager::isPlaying() const {
    return is_loaded && ma_sound_is_playing(&sound);
}

float AudioManager::getTotalLengthSecs() const {
    if(!is_loaded) {
        return 0.0f;
    }
    float length = 0.f;

    ma_sound_get_length_in_seconds(const_cast<ma_sound*>(&sound), &length);
    
    return length;
}

float AudioManager::getCurrentPositionSecs() const {
    if(!is_loaded) {
        return 0.0f;
    }
    float pos = 0.f;
    ma_sound_get_cursor_in_seconds(const_cast<ma_sound*>(&sound), &pos);

    return pos;
}

float AudioManager::getProgress() const
{
    const float total = getTotalLengthSecs();

    if (total <= 0.0f)
        return 0.0f;

    return (getCurrentPositionSecs() / total) * 100;
}