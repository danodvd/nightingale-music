#pragma once

#include "../persistence/ConfigManager.hpp"
#include "../audio/Playback/PlaybackController.hpp"
#include "../domain/models/Track.hpp"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <optional>
#include <thread>
#include <vector>

enum class PlaybackState {
    Stopped,
    Playing,
    Paused
};

class PlaybackService {
public:
    using StateChangedCallback = std::function<void(PlaybackState)>;
    using TrackChangedCallback = std::function<void(const std::optional<Track>&)>;
    using PositionChangedCallback = std::function<void(uint32_t positionMs, uint32_t durationMs)>;
    using QueueChangedCallback = std::function<void(const std::vector<Track>&, size_t currentIndex)>;

    explicit PlaybackService(PlaybackController& playbackController, ConfigManager& configManager);
    ~PlaybackService();

    void play();
    void pause();
    void togglePlayPause();
    void stop();
    void next();
    void previous();
    void seek(uint32_t positionMs);

    void setQueue(const std::vector<Track>& tracks, size_t startIndex = 0);
    void enqueueNext(const Track& track);
    void enqueueEnd(const Track& track);
    void removeFromQueue(size_t index);
    void clearQueue();

    void setVolume(float volume); // 0.0f - 1.0f
    [[nodiscard]] float getVolume() const;
    void setRepeatMode(RepeatMode mode);
    [[nodiscard]] RepeatMode getRepeatMode() const;
    void setShuffle(bool enabled);
    [[nodiscard]] bool isShuffleEnabled() const;

    [[nodiscard]] PlaybackState getState() const;
    [[nodiscard]] std::optional<Track> getCurrentTrack() const;
    [[nodiscard]] uint32_t getPositionMs() const;
    [[nodiscard]] uint32_t getDurationMs() const;

    void setOnStateChanged(StateChangedCallback callback);
    void setOnTrackChanged(TrackChangedCallback callback);
    void setOnPositionChanged(PositionChangedCallback callback);
    void setOnQueueChanged(QueueChangedCallback callback);

    void syncFromConfig();

private:
   PlaybackController& playbackController_;
    ConfigManager& configManager_;

    mutable std::mutex mutex_;
    PlaybackState state_ = PlaybackState::Stopped;
    std::vector<Track> queue_;
    std::vector<size_t> playOrder_;
    size_t currentOrderIndex_ = 0;

    float volume_ = 0.8f;
    RepeatMode repeatMode_ = RepeatMode::None;
    bool shuffle_ = false;

    std::atomic<bool> monitorRunning_{false};
    std::condition_variable monitorCv_;
    std::thread monitorThread_;

    StateChangedCallback onStateChanged_;
    TrackChangedCallback onTrackChanged_;
    PositionChangedCallback onPositionChanged_;
    QueueChangedCallback onQueueChanged_;

    void rebuildPlayOrder(std::optional<size_t> pinnedQueueIndex = std::nullopt);
    void loadAndPlayCurrent();
    void handleTrackFinished();
    void monitorLoop();
};