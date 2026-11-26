#pragma once

#include "../../third_party/miniaudio.h"
#include <cstdint>
#include <string>

class AudioSource {
    public:
    AudioSource() = default;
    ~AudioSource();

    AudioSource(const AudioSource&) = delete;
    AudioSource& operator=(const AudioSource&) = delete;

    /**
    * @brief Opens an audio file and reconfigures resampling to match target output format.
    */
    bool openFile(const std::string& path, uint32_t targetSampleRate, uint32_t targetChannels);

    void close();

    /**
    * @brief Decodes up to frameCount PCM frames into outBuffer.
    * @return Actual number of frames read (0 indicates EDF or error).
    */
    uint64_t readFrames(float* outBuffer, uint64_t frameCount);

    /**
    * @brief Moves the read cursor to the specified timestamp in seconds.
    * @pre 0.0f <= seconds <= getDurationSeconds()
    * @post getPositionSeconds() = seconds
    */
    bool seekToSeconds(float seconds);

    float getDurationSeconds() const;
    float getPositionSeconds() const;
    bool isLoaded() const { return isLoaded_; }
    bool isFinished() const { return isFinished_; }

    private:
    ma_decoder decoder_{};
    uint32_t sampleRate_ = 0;
    uint32_t channels_ = 0;
    uint64_t totalFrames_ = 0;
    uint64_t currentFrame_ = 0;
    bool isLoaded_ = false;
    bool isFinished_ = false;
};