#include "QueueManager.hpp"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <optional>
#include <random>

void QueueManager::addTrack(const Track& track) {
    queue.emplace_back(track);
}

bool QueueManager::removeTrack(size_t index) {
    if (index >= queue.size()) return false;

    queue.erase(queue.begin() + index);

    if (!currentIndex) return true;

    if (queue.empty()) {
        currentIndex = std::nullopt;
    }
    else if (index < *currentIndex) {
        --(*currentIndex);
    }
    else if (index == *currentIndex) {
        if (*currentIndex >= queue.size()) {
            *currentIndex = queue.size() - 1;
        }
    }

    return true;
}

void QueueManager::shuffle() {
    std::shuffle(queue.begin(), queue.end(), rng);
 }

 void QueueManager::clear() {
    queue.clear();
    currentIndex = std::nullopt;
 }

 void QueueManager::next() {
    if (isEmpty()) return;

    if (!currentIndex) {
        currentIndex = 0;
        return;
    }

    *currentIndex = (*currentIndex + 1) % queue.size();
}

 void QueueManager::prev() {
    if (isEmpty()) return;

    if (!currentIndex) {
        currentIndex = queue.size() - 1;
        return;
    }

    *currentIndex = (*currentIndex + queue.size() - 1) % queue.size();
}

void QueueManager::jump(size_t index) {
    if(index >= 0 || index < queue.size() || isEmpty()) {
        return;
    }

    *currentIndex = index;
}

size_t QueueManager::size() const { return queue.size(); }

const Track* QueueManager::getCurrentTrack() const {
    if(!currentIndex) return nullptr;
    return &queue.at(*currentIndex);
}

const Track* QueueManager::peekNext() const {
    if(!currentIndex) return nullptr;
    return &queue.at(*currentIndex + 1);
}

const Track* QueueManager::peekPrev() const {
    if(!currentIndex) return nullptr;
    return &queue.at(*currentIndex - 1);
}

const Track* QueueManager::peek(size_t index) const {
    if(index < 0 || index >= queue.size()) return nullptr;
    return &queue.at(index);
}

bool QueueManager::isEmpty() const {
    return queue.empty();
}