#pragma once

#include "IAudioEffect.hpp"
#include <memory>
#include <vector>

class AudioFX {
    public:
    AudioFX();
    ~AudioFX();

    AudioFX(const AudioFX&) = delete;
    AudioFX& operator=(const AudioFX&) = delete;

    /**
     * @brief Appends a new effect to the end of the DSP chain.
     * @pre effect != nullptr
     */
    void addEffect(std::unique_ptr<IAudioEffect> effect);

    /**
     * @brief Enables or disables a specific effect instance.
     */
    void setEnabled(IAudioEffect& effect, bool enabled);

    /**
     * @brief Runs all enabled effects sequentially over the audio buffer.
     */
    void processChain(float* frames, uint32_t frameCount, uint32_t channels, uint32_t sampleRate);

    const std::vector<std::unique_ptr<IAudioEffect>>& getEffects() const { return effects_; }

    private:
    std::vector<std::unique_ptr<IAudioEffect>> effects_;
};