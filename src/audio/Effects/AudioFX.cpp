#include "AudioFX.hpp"

#include "IAudioEffect.hpp"
#include <cassert>
#include <memory>
#include <vector>

AudioFX::AudioFX() = default;
AudioFX::~AudioFX() = default;

void AudioFX::addEffect(std::unique_ptr<IAudioEffect> effect) {
    assert(effect != nullptr);
    if(!effect) return;

    effects_.push_back(std::move(effect));
}

void AudioFX::setEnabled(IAudioEffect& effect, bool enabled) {
    effect.enable(enabled);
}

void AudioFX::processChain(float* frames, uint32_t frameCount, uint32_t channels, uint32_t sampleRate) {
    if(!frames || frameCount == 0 || channels == 0) {
        return;
    }
    for (const auto& effect : effects_) {
        if(effect && effect->isEnabled()) {
            effect->process(frames, frameCount, channels, sampleRate);
        }
    }
}
