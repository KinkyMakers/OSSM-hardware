#include "queue.h"

std::queue<String> messageQueue = {};

PositionTime lastPositionTime = {0, 0};
PositionTime targetPositionTime = {0, 0};
bool targetUpdated = false;

void markTargetUpdated() {
    targetUpdated = true;
}

bool consumeTargetUpdate() {
    if (targetUpdated) {
        targetUpdated = false;
        return true;
    }
    return false;
}
