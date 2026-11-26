#include "PlaybackService.hpp"

#include <algorithm>
#include <chrono>
#include <numeric>
#include <random>
#include <utility>

PlaybackService::PlaybackService(PlaybackController& playbackController, ConfigManager& configManager)
    : playbackController_(playbackController)
    , configManager_(configManager) {
    const AppConfig cfg = configManager_.getConfig();
    volume_ = std::clamp(cfg.defaultVolume, 0.0f, 1.0f);
    repeatMode_ = cfg.repeatMode;
    shuffle_ = cfg.shuffleEnabled;

    playbackController_.setVolume(volume_);

    monitorRunning_.store(true, std::memory_order_release);
    monitorThread_ = std::thread(&PlaybackService::monitorLoop, this);
}

PlaybackService::~PlaybackService() {
    monitorRunning_.store(false, std::memory_order_release);
    monitorCv_.notify_all();
    if (monitorThread_.joinable()) {
        monitorThread_.join();
    }
    playbackController_.stop();
}

void PlaybackService::play() {
    PlaybackState prevState;
    bool shouldLoadFromStart = false;
    bool shouldResume = false;
    StateChangedCallback stateCb;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty() || playOrder_.empty()) {
            return;
        }

        prevState = state_;
        if (state_ == PlaybackState::Playing) {
            return;
        }

        if (state_ == PlaybackState::Paused) {
            state_ = PlaybackState::Playing;
            shouldResume = true;
            stateCb = onStateChanged_;
        } else {
            shouldLoadFromStart = true;
        }
    }

    if (shouldResume) {
        if (!playbackController_.isPlaying()) {
            playbackController_.togglePlay();
        }
        if (stateCb && prevState != PlaybackState::Playing) {
            stateCb(PlaybackState::Playing);
        }
    } else if (shouldLoadFromStart) {
        loadAndPlayCurrent();
    }
}

void PlaybackService::pause() {
    StateChangedCallback stateCb;
    bool shouldToggle = false;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != PlaybackState::Playing) {
            return;
        }
        state_ = PlaybackState::Paused;
        shouldToggle = true;
        stateCb = onStateChanged_;
    }

    if (shouldToggle && playbackController_.isPlaying()) {
        playbackController_.togglePlay();
    }
    if (stateCb) {
        stateCb(PlaybackState::Paused);
    }
}

void PlaybackService::togglePlayPause() {
    if (getState() == PlaybackState::Playing) {
        pause();
    } else {
        play();
    }
}

void PlaybackService::stop() {
    StateChangedCallback stateCb;
    PositionChangedCallback posCb;
    uint32_t durationMs = 0;
    bool stateChanged = false;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        stateChanged = (state_ != PlaybackState::Stopped);
        state_ = PlaybackState::Stopped;
        if (!playOrder_.empty() && currentOrderIndex_ < playOrder_.size()) {
            durationMs = queue_[playOrder_[currentOrderIndex_]].durationMs;
        }
        stateCb = onStateChanged_;
        posCb = onPositionChanged_;
    }

    playbackController_.stop();

    if (stateChanged && stateCb) {
        stateCb(PlaybackState::Stopped);
    }
    if (posCb) {
        posCb(0, durationMs);
    }
}

void PlaybackService::next() {
    bool shouldStop = false;
    bool shouldPlay = false;
    QueueChangedCallback queueCb;
    std::vector<Track> queueCopy;
    size_t currentQueueIdx = 0;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (playOrder_.empty()) {
            return;
        }

        if (currentOrderIndex_ + 1 < playOrder_.size()) {
            ++currentOrderIndex_;
            shouldPlay = true;
        } else if (repeatMode_ == RepeatMode::All) {
            currentOrderIndex_ = 0;
            shouldPlay = true;
        } else {
            currentOrderIndex_ = 0;
            shouldStop = true;
        }

        queueCopy = queue_;
        currentQueueIdx = playOrder_[currentOrderIndex_];
        queueCb = onQueueChanged_;
    }

    if (queueCb) {
        queueCb(queueCopy, currentQueueIdx);
    }

    if (shouldPlay) {
        loadAndPlayCurrent();
    } else if (shouldStop) {
        stop();
    }
}

void PlaybackService::previous() {
    if (getPositionMs() > 3000) {
        seek(0);
        return;
    }

    bool shouldPlay = false;
    QueueChangedCallback queueCb;
    std::vector<Track> queueCopy;
    size_t currentQueueIdx = 0;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (playOrder_.empty()) {
            return;
        }

        if (currentOrderIndex_ > 0) {
            --currentOrderIndex_;
        } else if (repeatMode_ == RepeatMode::All) {
            currentOrderIndex_ = playOrder_.size() - 1;
        } else {
            currentOrderIndex_ = 0;
        }

        shouldPlay = true;
        queueCopy = queue_;
        currentQueueIdx = playOrder_[currentOrderIndex_];
        queueCb = onQueueChanged_;
    }

    if (queueCb) {
        queueCb(queueCopy, currentQueueIdx);
    }

    if (shouldPlay) {
        loadAndPlayCurrent();
    }
}

void PlaybackService::seek(uint32_t positionMs) {
    PositionChangedCallback posCb;
    uint32_t durationMs = 0;
    bool canSeek = false;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ == PlaybackState::Stopped || playOrder_.empty()) {
            return;
        }
        durationMs = queue_[playOrder_[currentOrderIndex_]].durationMs;
        if (durationMs == 0) {
            durationMs = static_cast<uint32_t>(playbackController_.getTotalLengthSecs() * 1000.0f);
        }
        canSeek = true;
        posCb = onPositionChanged_;
    }

    if (!canSeek) {
        return;
    }

    const uint32_t clampedMs = (durationMs > 0) ? std::min(positionMs, durationMs) : positionMs;
    playbackController_.seek(static_cast<float>(clampedMs) / 1000.0f);

    if (posCb) {
        posCb(clampedMs, durationMs);
    }
}

void PlaybackService::setQueue(const std::vector<Track>& tracks, size_t startIndex) {
    if (tracks.empty()) {
        clearQueue();
        return;
    }

    QueueChangedCallback queueCb;
    std::vector<Track> queueCopy;
    size_t activeQueueIdx = 0;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_ = tracks;
        const size_t safeStart = (startIndex < queue_.size()) ? startIndex : 0;
        rebuildPlayOrder(safeStart);
        currentOrderIndex_ = shuffle_ ? 0 : safeStart;

        queueCopy = queue_;
        activeQueueIdx = playOrder_[currentOrderIndex_];
        queueCb = onQueueChanged_;
    }

    if (queueCb) {
        queueCb(queueCopy, activeQueueIdx);
    }

    loadAndPlayCurrent();
}

void PlaybackService::enqueueNext(const Track& track) {
    bool wasEmpty = false;
    QueueChangedCallback queueCb;
    std::vector<Track> queueCopy;
    size_t activeQueueIdx = 0;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        wasEmpty = queue_.empty();
        const size_t newIndex = queue_.size();
        queue_.push_back(track);

        if (wasEmpty) {
            playOrder_ = {0};
            currentOrderIndex_ = 0;
        } else {
            const size_t insertPos = std::min(currentOrderIndex_ + 1, playOrder_.size());
            playOrder_.insert(playOrder_.begin() + static_cast<ptrdiff_t>(insertPos), newIndex);
        }

        queueCopy = queue_;
        activeQueueIdx = playOrder_[currentOrderIndex_];
        queueCb = onQueueChanged_;
    }

    if (queueCb) {
        queueCb(queueCopy, activeQueueIdx);
    }

    if (wasEmpty) {
        loadAndPlayCurrent();
    }
}

void PlaybackService::enqueueEnd(const Track& track) {
    bool wasEmpty = false;
    QueueChangedCallback queueCb;
    std::vector<Track> queueCopy;
    size_t activeQueueIdx = 0;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        wasEmpty = queue_.empty();
        const size_t newIndex = queue_.size();
        queue_.push_back(track);
        playOrder_.push_back(newIndex);

        if (wasEmpty) {
            currentOrderIndex_ = 0;
        }

        queueCopy = queue_;
        activeQueueIdx = playOrder_[currentOrderIndex_];
        queueCb = onQueueChanged_;
    }

    if (queueCb) {
        queueCb(queueCopy, activeQueueIdx);
    }

    if (wasEmpty) {
        loadAndPlayCurrent();
    }
}

void PlaybackService::removeFromQueue(size_t index) {
    bool removedCurrent = false;
    bool becameEmpty = false;
    bool wasPlaying = false;
    QueueChangedCallback queueCb;
    std::vector<Track> queueCopy;
    size_t activeQueueIdx = 0;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (index >= queue_.size()) {
            return;
        }

        wasPlaying = (state_ == PlaybackState::Playing);
        const size_t currentQueueIdx = playOrder_[currentOrderIndex_];
        removedCurrent = (index == currentQueueIdx);

        queue_.erase(queue_.begin() + static_cast<ptrdiff_t>(index));

        std::vector<size_t> updatedOrder;
        updatedOrder.reserve(playOrder_.size());
        size_t newOrderPos = 0;

        for (size_t i = 0; i < playOrder_.size(); ++i) {
            const size_t qIdx = playOrder_[i];
            if (qIdx == index) {
                continue;
            }
            if (i < currentOrderIndex_) {
                ++newOrderPos;
            }
            updatedOrder.push_back(qIdx > index ? qIdx - 1 : qIdx);
        }

        playOrder_ = std::move(updatedOrder);
        if (playOrder_.empty()) {
            becameEmpty = true;
            currentOrderIndex_ = 0;
        } else {
            currentOrderIndex_ = std::min(newOrderPos, playOrder_.size() - 1);
            activeQueueIdx = playOrder_[currentOrderIndex_];
        }

        queueCopy = queue_;
        queueCb = onQueueChanged_;
    }

    if (becameEmpty) {
        clearQueue();
        return;
    }

    if (queueCb) {
        queueCb(queueCopy, activeQueueIdx);
    }

    if (removedCurrent && wasPlaying) {
        loadAndPlayCurrent();
    }
}

void PlaybackService::clearQueue() {
    TrackChangedCallback trackCb;
    QueueChangedCallback queueCb;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.clear();
        playOrder_.clear();
        currentOrderIndex_ = 0;
        trackCb = onTrackChanged_;
        queueCb = onQueueChanged_;
    }

    stop();

    if (trackCb) {
        trackCb(std::nullopt);
    }
    if (queueCb) {
        queueCb({}, 0);
    }
}

void PlaybackService::setVolume(float volume) {
    const float clamped = std::clamp(volume, 0.0f, 1.0f);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        volume_ = clamped;
    }
    playbackController_.setVolume(clamped);
    configManager_.updateConfig([clamped](AppConfig& cfg) {
        cfg.defaultVolume = clamped;
    });
}

float PlaybackService::getVolume() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return volume_;
}

void PlaybackService::setRepeatMode(RepeatMode mode) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (repeatMode_ == mode) {
            return;
        }
        repeatMode_ = mode;
    }
    configManager_.updateConfig([mode](AppConfig& cfg) {
        cfg.repeatMode = mode;
    });
}

RepeatMode PlaybackService::getRepeatMode() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return repeatMode_;
}

void PlaybackService::setShuffle(bool enabled) {
    QueueChangedCallback queueCb;
    std::vector<Track> queueCopy;
    size_t activeQueueIdx = 0;
    bool changed = false;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shuffle_ == enabled) {
            return;
        }
        shuffle_ = enabled;
        changed = true;

        if (!queue_.empty() && !playOrder_.empty()) {
            const size_t currentQueueIdx = playOrder_[currentOrderIndex_];
            rebuildPlayOrder(currentQueueIdx);
            currentOrderIndex_ = enabled
                ? 0
                : currentQueueIdx;
            queueCopy = queue_;
            activeQueueIdx = playOrder_[currentOrderIndex_];
            queueCb = onQueueChanged_;
        }
    }

    if (changed) {
        configManager_.updateConfig([enabled](AppConfig& cfg) {
            cfg.shuffleEnabled = enabled;
        });
        if (queueCb) {
            queueCb(queueCopy, activeQueueIdx);
        }
    }
}

bool PlaybackService::isShuffleEnabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return shuffle_;
}

PlaybackState PlaybackService::getState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

std::optional<Track> PlaybackService::getCurrentTrack() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (playOrder_.empty() || currentOrderIndex_ >= playOrder_.size()) {
        return std::nullopt;
    }
    return queue_[playOrder_[currentOrderIndex_]];
}

uint32_t PlaybackService::getPositionMs() const {
    if (getState() == PlaybackState::Stopped) {
        return 0;
    }
    const float secs = playbackController_.getCurrentPositionSecs();
    return secs > 0.0f ? static_cast<uint32_t>(secs * 1000.0f) : 0;
}

uint32_t PlaybackService::getDurationMs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (playOrder_.empty() || currentOrderIndex_ >= playOrder_.size()) {
        return 0;
    }
    const uint32_t modelDuration = queue_[playOrder_[currentOrderIndex_]].durationMs;
    if (modelDuration > 0) {
        return modelDuration;
    }
    const float secs = playbackController_.getTotalLengthSecs();
    return secs > 0.0f ? static_cast<uint32_t>(secs * 1000.0f) : 0;
}

void PlaybackService::setOnStateChanged(StateChangedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onStateChanged_ = std::move(callback);
}

void PlaybackService::setOnTrackChanged(TrackChangedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onTrackChanged_ = std::move(callback);
}

void PlaybackService::setOnPositionChanged(PositionChangedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onPositionChanged_ = std::move(callback);
}

void PlaybackService::setOnQueueChanged(QueueChangedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onQueueChanged_ = std::move(callback);
}

void PlaybackService::rebuildPlayOrder(std::optional<size_t> pinnedQueueIndex) {
    playOrder_.resize(queue_.size());
    std::iota(playOrder_.begin(), playOrder_.end(), 0);

    if (!shuffle_ || playOrder_.size() <= 1) {
        return;
    }

    std::random_device rd;
    std::mt19937 gen(rd());

    if (pinnedQueueIndex && *pinnedQueueIndex < playOrder_.size()) {
        std::swap(playOrder_[0], playOrder_[*pinnedQueueIndex]);
        std::shuffle(playOrder_.begin() + 1, playOrder_.end(), gen);
    } else {
        std::shuffle(playOrder_.begin(), playOrder_.end(), gen);
    }
}

void PlaybackService::loadAndPlayCurrent() {
    Track trackToPlay;
    StateChangedCallback stateCb;
    TrackChangedCallback trackCb;
    PositionChangedCallback posCb;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (playOrder_.empty() || currentOrderIndex_ >= playOrder_.size()) {
            return;
        }
        trackToPlay = queue_[playOrder_[currentOrderIndex_]];
        // Marcar Stopped transitoriamente mientras se carga la nueva pista en el dispositivo
        state_ = PlaybackState::Stopped;
        stateCb = onStateChanged_;
        trackCb = onTrackChanged_;
        posCb = onPositionChanged_;
    }

    if (!playbackController_.playTrack(trackToPlay.filePath)) {
        if (stateCb) {
            stateCb(PlaybackState::Stopped);
        }
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = PlaybackState::Playing;
    }

    const uint32_t durationMs = getDurationMs();
    if (trackCb) {
        trackCb(trackToPlay);
    }
    if (stateCb) {
        stateCb(PlaybackState::Playing);
    }
    if (posCb) {
        posCb(0, durationMs);
    }
}

void PlaybackService::handleTrackFinished() {
    RepeatMode mode;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        mode = repeatMode_;
    }

    if (mode == RepeatMode::One) {
        loadAndPlayCurrent();
    } else {
        next();
    }
}

void PlaybackService::monitorLoop() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (monitorRunning_.load(std::memory_order_acquire)) {
        monitorCv_.wait_for(lock, std::chrono::milliseconds(200), [this] {
            return !monitorRunning_.load(std::memory_order_acquire);
        });

        if (!monitorRunning_.load(std::memory_order_acquire)) {
            break;
        }

        if (state_ != PlaybackState::Playing) {
            continue;
        }

        PositionChangedCallback posCb = onPositionChanged_;
        uint32_t durationMs = 0;
        if (!playOrder_.empty() && currentOrderIndex_ < playOrder_.size()) {
            durationMs = queue_[playOrder_[currentOrderIndex_]].durationMs;
        }

        lock.unlock();

        const bool stillPlaying = playbackController_.isPlaying();
        if (!stillPlaying) {
            handleTrackFinished();
        } else if (posCb) {
            const float posSecs = playbackController_.getCurrentPositionSecs();
            const uint32_t posMs = posSecs > 0.0f ? static_cast<uint32_t>(posSecs * 1000.0f) : 0;
            if (durationMs == 0) {
                durationMs = static_cast<uint32_t>(playbackController_.getTotalLengthSecs() * 1000.0f);
            }
            posCb(posMs, durationMs);
        }

        lock.lock();
    }
}

void PlaybackService::syncFromConfig() {
    const AppConfig cfg = configManager_.getConfig();
    const float clampedVol = std::clamp(cfg.defaultVolume, 0.0f, 1.0f);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        volume_ = clampedVol;
        repeatMode_ = cfg.repeatMode;
        shuffle_ = cfg.shuffleEnabled;
    }

    playbackController_.setVolume(clampedVol);
}