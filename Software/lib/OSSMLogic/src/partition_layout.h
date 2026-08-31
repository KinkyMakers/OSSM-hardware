#pragma once

#include <cstdint>

namespace firmware {

// Identify the actual installed OTA geometry, not the compiled flash capacity.
// The two 4 MB layouts are distinct and must never share a layout identifier.
constexpr const char *detectOtaPartitionLayout(std::uint32_t app0Address,
                                              std::uint32_t app0Size,
                                              std::uint32_t app1Address,
                                              std::uint32_t app1Size) {
    if (app0Address == 0x10000 && app0Size == 0x1E0000 &&
        app1Address == 0x1F0000 && app1Size == 0x1E0000) {
        return "ossm-ota-4mb-v1";
    }
    if (app0Address == 0x10000 && app0Size == 0x1F0000 &&
        app1Address == 0x200000 && app1Size == 0x1F0000) {
        return "ossm-ota-v1";
    }
    if (app0Address == 0x10000 && app0Size == 0x780000 &&
        app1Address == 0x790000 && app1Size == 0x780000) {
        return "ossm-ota-16mb-v1";
    }
    return "unknown";
}

}  // namespace firmware
