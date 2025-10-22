#include "queue.h"

std::queue<String> messageQueue = {};

PositionTime lastPositionTime = {0, 0};
PositionTime targetPositionTime = {0, 0};

bool hasTargetChanged() {
    return lastPositionTime.position != targetPositionTime.position ||
           lastPositionTime.inTime != targetPositionTime.inTime;
}
