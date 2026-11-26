#pragma once

#include "../../third_party/miniaudio.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct DeviceInfo {
    std::string name;
    bool isDefault = false;
};

class AudioDevice {
    public: 
    using AudioCallback = std::function<void(float* output, uint32_t frameCount, uint32_t channels)>;

    AudioDevice() = default;
    ~AudioDevice();

    AudioDevice(const AudioDevice&) = delete;
    AudioDevice& operator=(const AudioDevice&) = delete;

    std::vector<DeviceInfo> listPlaybackDevices() const;

    /**
    * @brief Initializes ma_device on this device.
    * @pre !isOpen
    * @post Returns true iff the playback device was initialized (isOpen() == true).
    */
    bool open(uint32_t sampleRate = 48000, uint32_t channels = 2, int deviceIndex = -1);

    /**
     * @brief Stops and uninitializes the active device if open.
     * @post !isOpen()
     */
    void close();

    /**
    * @pre isOpen
    */
    bool start();

    /**
    * @pre isOpen
    */
    bool stop();

    /**
     * @brief Sets the master volume for this specific device.
     * @pre isOpen() && 0.0f <= volume && volume <= 1.0f
     * @post device volume == volume
     */
    void setMasterVolume(float volume); // [0.0f,1.0f]

    /**
     * @pre callback != nullptr && !isOpen()
     * @post Sets the audio rendering callback for this device.
     */
    void setCallback(AudioCallback callback);

    uint32_t getSampleRate() const { return sampleRate_; }
    uint32_t getChannels() const { return channels_; }
    bool isOpen() const { return isOpen_; }

    private:
    ma_context context_{};
    ma_device device_{};
    AudioCallback callback_;
    uint32_t sampleRate_ = 48000;
    uint32_t channels_ = 2;
    bool isOpen_ = false;
};