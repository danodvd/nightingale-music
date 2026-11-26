#include "AudioDevice.hpp"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <vector>

AudioDevice::~AudioDevice() { close(); }

std::vector<DeviceInfo> AudioDevice::listPlaybackDevices() const {
    std::vector<DeviceInfo> result;

    ma_context tempContext{};
    if(ma_context_init(nullptr, 0, nullptr, &tempContext) != MA_SUCCESS) return result;

    ma_device_info* pPlaybackInfos = nullptr;
    ma_uint32 playbackCount = 0;

    if(ma_context_get_devices(&tempContext, &pPlaybackInfos, &playbackCount, nullptr, nullptr) == MA_SUCCESS) {
        result.reserve(playbackCount);
        for(ma_uint32 i = 0; i < playbackCount; i++) {
            DeviceInfo info;
            info.name = pPlaybackInfos[i].name;
            info.isDefault = (pPlaybackInfos[i].isDefault == MA_TRUE);
            result.push_back(std::move(info));
        }
    }

    ma_context_uninit(&tempContext);
    return result;
}

bool AudioDevice::open(uint32_t sampleRate, uint32_t channels, int deviceIndex ) {
    assert(!isOpen_);
    if(isOpen_) { return false; }

    if(ma_context_init(nullptr, 0, nullptr, &context_) != MA_SUCCESS) {
        return false;
    }

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = channels;
    config.sampleRate = sampleRate;
    config.pUserData = this;

    config.dataCallback = [](ma_device* pDevice, void* pOutput, const void* /*pInput*/, ma_uint32 frameCount) {
        auto* self = static_cast<AudioDevice*>(pDevice->pUserData);
        if(!self || !pOutput) return;

        float* outFloat = static_cast<float*>(pOutput);
        std::memset(outFloat, 0, sizeof(float) * frameCount * self->channels_);

        if(self->callback_) {
            self->callback_(outFloat, frameCount, self->channels_);
        }
    };

    if(deviceIndex >= 0) {
        ma_device_info* pPlaybackInfos = nullptr;
        ma_uint32 playbackCount = 0;
        if (ma_context_get_devices(&context_, &pPlaybackInfos, &playbackCount, nullptr, nullptr) == MA_SUCCESS) {
            if (static_cast<ma_uint32>(deviceIndex) < playbackCount) {
                config.playback.pDeviceID = &pPlaybackInfos[deviceIndex].id;
            } else {
                ma_context_uninit(&context_);
                return false;
            }
        }
    }
    if (ma_device_init(&context_, &config, &device_) != MA_SUCCESS) {
        ma_context_uninit(&context_);
        return false;
    }

    sampleRate_ = sampleRate;
    channels_   = channels;
    isOpen_     = true;
    return true;
}

void AudioDevice::close() {
    if(!isOpen_) return;

    ma_device_uninit(&device_);
    ma_context_uninit(&context_);
    isOpen_ = false;
}

bool AudioDevice::start() {
    assert(isOpen_);
    if(!isOpen_) return false;
    return ma_device_start(&device_) == MA_SUCCESS;
}

bool AudioDevice::stop() {
    assert(isOpen_);
    if(!isOpen_) return false;
    return ma_device_stop(&device_) == MA_SUCCESS;
}

void AudioDevice::setMasterVolume(float volume) {
    assert(isOpen_ && volume >= 0.0f && volume <= 1.0f);
    if(!isOpen_) { return; }

    volume = std::clamp(volume, 0.0f, 1.0f);
    ma_device_set_master_volume(&device_, volume);
}

void AudioDevice::setCallback(AudioCallback callback) {
    assert(callback != nullptr && !isOpen_);
    if(!isOpen_) {
        callback_ = std::move(callback);
    }
}
