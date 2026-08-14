#include "QueueManager.hpp"
#include <algorithm>

void QueueManager::enqueue(int id) {
    if (std::find(queue.begin(), queue.end(), id) != queue.end())
        return;

    queue.push_back(id);
}

void QueueManager::playNext(int id) {
    if (queue.empty()) {
        queue.push_back(id);
        return;
    }
    queue.insert(queue.begin() + ++currentIndex, id);
}

void QueueManager::remove(size_t index) {
    if(index >= queue.size()) {
        return;
    }

    queue.erase(queue.begin() + index);
}

bool QueueManager::next() {
    if(currentIndex + 1 >= queue.size()) {
        return false;
    }

    ++currentIndex;
    return true;
}

bool QueueManager::previous() {
    if(currentIndex == 0) {
        return false;
    }

    --currentIndex;
    return true;
}

int QueueManager::currentSongId() const {
    return queue[currentIndex];
}

bool QueueManager::empty() const {
    return queue.empty();
}

size_t QueueManager::size() const {
    return queue.size();
}