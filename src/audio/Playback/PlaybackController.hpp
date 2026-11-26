#pragma  once

#include <atomic>
#include <mutex>
#include <string>
#include "../Effects/AudioFX.hpp"
#include "../Sources/AudioDevice.hpp"
#include "../Sources/AudioSource.hpp"

/**
*   PlaybackController is the designated controller for the current audio playback. Its functions are entirely related to
*   playback states.
*/
class PlaybackController {
    public:
    PlaybackController();
    ~PlaybackController();

    // Non-copyable and non-movable: miniaudio structs rely on stable memory addresses.
    PlaybackController(const PlaybackController&) = delete;
    PlaybackController& operator=(const PlaybackController&) = delete;

    /**
     * @brief Initializes the underlying audio engine.
     * @post Returns true iff the audio device/engine initialized properly.
     */
    bool init();

    /**
    * @brief Plays a designated track
    * @pre path != null
    * @post isPlaying == true
    */
    bool playTrack(const std::string& path);

    /**
    * @brief If a song is currently playing, pauses it. If it's paused, it toggles back playback.
    * @pre isLoaded
    * @post playbackState = !playbackState
    */
    void togglePlay();

    /**
     * @brief Stops playback and unloads the current track.
     * @post !isLoaded() && !isPlaying()
     */
    void stop();

    /**
     * @brief Seeks to a specific position in the current track.
     * @pre isLoaded() && seconds >= 0.0f && seconds <= getTotalLengthSecs()
     * @post getCurrentPositionSecs() ~= seconds
     */
    void seek(float seconds);

    /**
     * @brief Sets master playback volume.
     * @pre volume >= 0.0f && volume <= 1.0f
     */
    void setVolume(float volume);

    // Getters

    /**
    * @post Return true iff miniaudio is playing a track 
    */
    bool isPlaying() const;

    /**
    * @pre isLoaded
    * @post Return the total length in seconds of the currently loaded track.
    */
    float getTotalLengthSecs() const;

    /**
    * @pre isLoaded
    * @post Return the current position in seconds of the currently loaded track.
    */
    float getCurrentPositionSecs() const;

    /**
    * @pre isLoaded
    * @post Returns the current percentage of the currently loaded track in a scale from 0.0f to 1.0f.
    */
    float getProgress() const;

    AudioFX& getFX() { return fx_; }
    const AudioFX& getFX() const { return fx_; }

    private:
    AudioFX fx_{};
    AudioDevice device_{};
    AudioSource source_{};

    mutable std::mutex mutex_;
    std::atomic<bool> is_playing_{false};

    bool is_initialized_ = false;
    float volume_ = 0.8f;
};