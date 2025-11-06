#include "queue.h"

std::queue<String> messageQueue = {};

PositionTime lastPositionTime = {0, 0};
PositionTime targetPositionTime = {0, 0};

bool hasTargetChanged() {
    return abs(lastPositionTime.position - targetPositionTime.position) > 2 ||
           abs(lastPositionTime.inTime - targetPositionTime.inTime) > 10;
}
