#pragma once

#include <cstdint>

namespace firmware {

std::uint32_t physicalFlashSizeBytes();
std::uint32_t otaSlotSizeBytes();
const char *currentPartitionLayout();

}  // namespace firmware
