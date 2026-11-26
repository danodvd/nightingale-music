#include "PlaybackController.hpp"

#include <algorithm>
#include <atomic>
#include <mutex>

PlaybackController::PlaybackController() = default;

PlaybackController::~PlaybackController() {
    stop();
    device_.close();
}

bool PlaybackController::init() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_initialized_) {
        return true;
    }

    device_.setCallback([this](float* output, uint32_t frameCount, uint32_t channels) {
        if (!is_playing_.load(std::memory_order_acquire)) {
            return;
        }

        std::unique_lock<std::mutex> audioLock(mutex_, std::try_to_lock);
        if (!audioLock.owns_lock() || !source_.isLoaded()) {
            return;
        }

        const uint64_t framesRead = source_.readFrames(output, frameCount);

        // Si deseas aplicar efectos DSP sobre 'output' (framesRead * channels muestras),
        // invoca aquí tu pipeline de fx_ antes de verificar el fin de pista.
        (void)channels;

        if (framesRead == 0 || source_.isFinished()) {
            is_playing_.store(false, std::memory_order_release);
        }
    });

    if (!device_.open()) {
        return false;
    }

    device_.setMasterVolume(volume_);

    if (!device_.start()) {
        device_.close();
        return false;
    }

    is_initialized_ = true;
    return true;
}

void PlaybackController::setVolume(float volume) {
    const float clamped = std::clamp(volume, 0.0f, 1.0f);
    std::lock_guard<std::mutex> lock(mutex_);
    volume_ = clamped;
    if (is_initialized_) {
        device_.setMasterVolume(clamped);
    }
}

bool PlaybackController::playTrack(const std::string& path) {
    is_playing_.store(false, std::memory_order_release);

    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_initialized_) {
        return false;
    }

    source_.close();
    if (!source_.openFile(path, device_.getSampleRate(), device_.getChannels())) {
        return false;
    }

    is_playing_.store(true, std::memory_order_release);
    return true;
}

void PlaybackController::togglePlay() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!source_.isLoaded()) {
        return;
    }

    if (!is_playing_.load(std::memory_order_acquire) && source_.isFinished()) {
        source_.seekToSeconds(0.0f);
    }

    is_playing_.store(!is_playing_.load(std::memory_order_acquire), std::memory_order_release);
}

void PlaybackController::stop() {
    is_playing_.store(false, std::memory_order_release);

    std::lock_guard<std::mutex> lock(mutex_);
    if (source_.isLoaded()) {
        source_.close();
    }
}

void PlaybackController::seek(float seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!source_.isLoaded()) {
        return;
    }

    const float duration = source_.getDurationSeconds();
    if (seconds >= 0.0f && seconds <= duration) {
        source_.seekToSeconds(seconds);
    }
}

bool PlaybackController::isPlaying() const {
    return is_playing_.load(std::memory_order_acquire);
}

float PlaybackController::getTotalLengthSecs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!source_.isLoaded()) {
        return 0.0f;
    }
    return source_.getDurationSeconds();
}

float PlaybackController::getCurrentPositionSecs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!source_.isLoaded()) {
        return 0.0f;
    }
    return source_.getPositionSeconds();
}

float PlaybackController::getProgress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!source_.isLoaded()) {
        return 0.0f;
    }
    const float total = source_.getDurationSeconds();
    return total > 0.0f ? (source_.getPositionSeconds() / total) : 0.0f;
}