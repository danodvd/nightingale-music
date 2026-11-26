#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <random>
#include <string>
#include <vector>
#include "../../persistence/repositories/TrackRepository.hpp"

struct Track;

class QueueManager {
    public:
    QueueManager();
    ~QueueManager();

    QueueManager(const QueueManager&) = delete;
    QueueManager& operator=(const QueueManager&) = delete;

    /**
    * @brief Adds a track to the end of this queue.
    * @pre track != null
    * @post size = (old)size + 1 && 'track' is now the last element of this queue.
    */
    void addTrack(const Track& track);

    /**
    * @brief Removes the track in the given index.
    * @pre 0 <= index < size
    * @post Returns true if the track was succesfully removed.
    */
    bool removeTrack(size_t index);

    /**
    * @pre !isEmpty
    * @post Shuffles the current queue.
    */
    void shuffle();

    /**
    * @brief Clears the current queue.
    * @post size == 0
    */
    void clear();

    /**
    * @brief Advances the current track.
    * @pre currentIndex < queueSize - 1 && !isEmpty
    * @post currentIndex++
    */
    void next();

    /**
    * @brief Backs to the previous track.
    * @pre currentIndex > 0 && !isEmpty
    * @post currentIndex--
    */
    void prev();

    /**
    * @brief Jumps to the given index
    * @pre 0 <= index < queueSize && !isEmpty
    * @post currentIndex = index
    */
    void jump(size_t index);

    /**
    * @post Return the current size of this queue
    */
    size_t size() const;

    /**
    * @pre !isEmpty
    * @post returns the track at currentIndex
    */
    const Track* getCurrentTrack() const;

    /**
    * @pre !isEmpty && currentIndex < size - 1
    * @post Returns the track at currentIndex + 1
    */
    const Track* peekNext() const;

    /**
    * @pre !isEmpty && currentIndex > 0
    * @post Returns the track at currentIndex - 1
    */
    const Track* peekPrev() const;

    /**
    * @pre !isEmpty && 0 <= index < size
    * @post Returns the track at the 'index' position of this queue.
    */
    const Track* peek(size_t index) const;

    /**
    * @post Returns true size == 0
    */
    bool isEmpty() const;

    private:
    std::mt19937 rng{std::random_device{}()};
    std::optional<size_t> currentIndex = std::nullopt;
    std::vector<Track> queue;
};