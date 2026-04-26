#include "AnalogSampler.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "services/tasks.h"

namespace AnalogSampler {

namespace {

constexpr size_t kMaxPins = 4;

struct PinSlot {
    int pin = -1;
    float alpha = kDefaultAlpha;
    bool initialized = false;
    // Atomic so any task can read the latest value without locking. ESP32
    // 32-bit aligned loads/stores are atomic, but std::atomic<float> makes the
    // contract explicit and silences the analyzer.
    std::atomic<float> percent{0.0f};
};

PinSlot slots[kMaxPins];
size_t slotCount = 0;
TaskHandle_t taskHandle = nullptr;

PinSlot *findSlot(int pin) {
    for (size_t i = 0; i < slotCount; ++i) {
        if (slots[i].pin == pin) {
            return &slots[i];
        }
    }
    return nullptr;
}

void samplerTask(void *) {
    TickType_t lastWake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(kSamplePeriodMs);

    for (;;) {
        for (size_t i = 0; i < slotCount; ++i) {
            PinSlot &slot = slots[i];
            int raw = analogRead(slot.pin);
            float sample = (raw * 100.0f) / 4095.0f;

            float current = slot.percent.load(std::memory_order_relaxed);
            float next;
            if (!slot.initialized) {
                // Seed the EMA with the first reading so we don't ramp up from
                // zero on boot.
                next = sample;
                slot.initialized = true;
            } else {
                next = current + slot.alpha * (sample - current);
            }
            slot.percent.store(next, std::memory_order_relaxed);
        }

        vTaskDelayUntil(&lastWake, period);
    }
}

}  // namespace

bool registerPin(int pin, float alpha) {
    if (taskHandle != nullptr) {
        return false;
    }

    if (PinSlot *existing = findSlot(pin)) {
        existing->alpha = alpha;
        return true;
    }

    if (slotCount >= kMaxPins) {
        return false;
    }

    slots[slotCount].pin = pin;
    slots[slotCount].alpha = alpha;
    slots[slotCount].initialized = false;
    slots[slotCount].percent.store(0.0f, std::memory_order_relaxed);
    ++slotCount;
    return true;
}

void start() {
    if (taskHandle != nullptr) {
        return;
    }

    xTaskCreatePinnedToCore(samplerTask, "analogSampler",
                            2 * configMINIMAL_STACK_SIZE, nullptr,
                            tskIDLE_PRIORITY + 1, &taskHandle,
                            Tasks::operationTaskCore);
}

float readPercent(int pin) {
    if (PinSlot *slot = findSlot(pin)) {
        return slot->percent.load(std::memory_order_relaxed);
    }
    return 0.0f;
}

}  // namespace AnalogSampler
