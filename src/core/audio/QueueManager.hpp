#pragma once

#include <vector>


class QueueManager {
public:
    QueueManager() = default;
    
    QueueManager(std::vector<int>& q, size_t currentIndex) {
        if(!q.empty()) queue = q;
        if(currentIndex > 0 && currentIndex < q.size()) {
            this->currentIndex = currentIndex;
        }
    }
    QueueManager(std::vector<int>& q) {
        if(!q.empty()) queue = q;
    }

    void enqueue(int id);
    void playNext(int id);

    void remove(size_t index);

    bool next();
    bool previous();

    int currentSongId() const;

    bool empty() const;
    size_t size() const;

private:
    std::vector<int> queue;
    size_t currentIndex = 0;
};