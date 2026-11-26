#pragma once

#include <cstdint>
#include <string>

class IAudioEffect {
    public:
    virtual ~IAudioEffect() = default;
    
    virtual std::string getName() const = 0;
    virtual void enable(bool enabled) = 0;
    virtual bool isEnabled() const = 0;

    virtual void process(float* frames, uint32_t frameCount, uint32_t channels, uint32_t sampleRate) = 0;
};