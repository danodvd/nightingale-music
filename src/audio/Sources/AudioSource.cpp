#include "AudioSource.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>

AudioSource::~AudioSource() { close(); }

bool AudioSource::openFile(const std::string& path, uint32_t targetSampleRate, uint32_t targetChannels) {
    if(isLoaded_) close();

    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, targetChannels, targetSampleRate);
    if(ma_decoder_init_file(path.c_str(), &config, &decoder_) != MA_SUCCESS) {
        return false;
    }

    ma_uint64 totalFrames = 0;
    if(ma_decoder_get_length_in_pcm_frames(&decoder_, &totalFrames) != MA_SUCCESS) {
        totalFrames = 0;
    }
    
    sampleRate_ = targetSampleRate;
    channels_ = targetChannels;
    totalFrames_ = static_cast<uint64_t>(totalFrames);
    currentFrame_ = 0;
    isLoaded_ = true;
    isFinished_ = false;

    return true;
}

void AudioSource::close() {
    if(!isLoaded_) return;

    ma_decoder_uninit(&decoder_);
    decoder_      = ma_decoder{};
    sampleRate_   = 0;
    channels_     = 0;
    totalFrames_  = 0;
    currentFrame_ = 0;
    isLoaded_     = false;
    isFinished_   = false;
}

uint64_t AudioSource::readFrames(float* outBuffer, uint64_t frameCount) {
    if (!isLoaded_ || !outBuffer || frameCount == 0) {
        return 0;
    }

    ma_uint64 framesRead = 0;
    ma_result result = ma_decoder_read_pcm_frames(&decoder_, outBuffer, frameCount, &framesRead);

    currentFrame_ += static_cast<uint64_t>(framesRead);

    if(result == MA_AT_END || framesRead < frameCount) {
        isFinished_ = true;
    }

    if(result != MA_SUCCESS && result != MA_AT_END) {
        return 0;
    }
    return static_cast<uint64_t>(framesRead);
}

bool AudioSource::seekToSeconds(float seconds) {
    if (!isLoaded_ || sampleRate_ == 0) {
        return false;
    }

    const float duration = getDurationSeconds();
    assert(seconds >= 0.0f && seconds <= duration);

    const float clampedSeconds = std::clamp(seconds, 0.0f, duration);
    const auto targetFrame = static_cast<ma_uint64>(clampedSeconds * static_cast<float>(sampleRate_));

    if (ma_decoder_seek_to_pcm_frame(&decoder_, targetFrame) != MA_SUCCESS) {
        return false;
    }

    currentFrame_ = static_cast<uint64_t>(targetFrame);
    isFinished_   = (totalFrames_ > 0 && currentFrame_ >= totalFrames_);

    return true;
}

float AudioSource::getDurationSeconds() const {
    if (!isLoaded_ || sampleRate_ == 0) {
        return 0.0f;
    }
    return static_cast<float>(totalFrames_) / static_cast<float>(sampleRate_);
}

float AudioSource::getPositionSeconds() const {
    if (!isLoaded_ || sampleRate_ == 0) {
        return 0.0f;
    }
    return static_cast<float>(currentFrame_) / static_cast<float>(sampleRate_);
}