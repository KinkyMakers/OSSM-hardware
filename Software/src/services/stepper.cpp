#include "stepper.h"

#include "driver/gpio.h"
#include "driver/rmt.h"
#include "esp_err.h"
#include "esp_log.h"

FastAccelStepperEngine stepperEngine = FastAccelStepperEngine();
FastAccelStepper *stepper = nullptr;
class StrokeEngine Stroker;

static bool s_rawRmtReady = false;

void initStepper() {
    stepperEngine.init();
    stepper = stepperEngine.stepperConnectToPin(Pins::Driver::motorStepPin);
    if (stepper) {
        stepper->setDirectionPin(Pins::Driver::motorDirectionPin, false);
        stepper->setEnablePin(Pins::Driver::motorEnablePin, true);
        stepper->setAutoEnable(false);
    }

    // disable motor briefly in case we are against a hard stop.
    digitalWrite(Pins::Driver::motorEnablePin, HIGH);
    delay(600);
    digitalWrite(Pins::Driver::motorEnablePin, LOW);
    delay(100);
}

void teardownStepperForRawRmt() {
    static const char *TAG = "RawRmtStepper";

    if (s_rawRmtReady) {
        ESP_LOGW(TAG, "raw RMT already initialised, skipping");
        return;
    }

    // 1. Stop the FAS planner and release the STEP pin.
    if (stepper) {
        stepper->forceStop();
        stepper->disableOutputs();
        stepper->detachFromPin();
        ESP_LOGI(TAG, "FAS detached from step pin %d",
                 Pins::Driver::motorStepPin);
    }

    // 2. Reconfigure DIR/ENABLE as plain GPIO outputs we control directly.
    //    ENABLE is active-low on this driver — drive LOW = motor enabled.
    gpio_config_t io = {};
    io.pin_bit_mask = (1ULL << Pins::Driver::motorDirectionPin) |
                      (1ULL << Pins::Driver::motorEnablePin);
    io.mode = GPIO_MODE_OUTPUT;
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&io));
    gpio_set_level((gpio_num_t)Pins::Driver::motorEnablePin, 0);
    gpio_set_level((gpio_num_t)Pins::Driver::motorDirectionPin, 0);

    // 3. Bring up RMT TX on the STEP pin, 1 us per tick (80 MHz / 80).
    rmt_config_t cfg = RMT_DEFAULT_CONFIG_TX(
        (gpio_num_t)Pins::Driver::motorStepPin, kRawStepRmtChannel);
    cfg.clk_div = 80;            // 1 us tick
    cfg.mem_block_num = 4;       // 4 * 64 = 256 items of HW buffer
    cfg.tx_config.carrier_en = false;
    cfg.tx_config.loop_en = false;
    cfg.tx_config.idle_output_en = true;
    cfg.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
    ESP_ERROR_CHECK(rmt_config(&cfg));
    ESP_ERROR_CHECK(rmt_driver_install(kRawStepRmtChannel, 0, 0));

    s_rawRmtReady = true;
    ESP_LOGI(TAG,
             "raw RMT step pulse driver ready on pin %d (channel %d, 1us)",
             Pins::Driver::motorStepPin, (int)kRawStepRmtChannel);
}

void rawStepSetDirection(bool forward) {
    gpio_set_level((gpio_num_t)Pins::Driver::motorDirectionPin,
                   forward ? 1 : 0);
}

void rawStepWritePulses(const rmt_item32_t *items, size_t count) {
    if (!s_rawRmtReady || count == 0 || items == nullptr) return;
    // Blocking write keeps the producer task (~200 Hz) in sync with hw output.
    rmt_write_items(kRawStepRmtChannel, items, (int)count, true);
}
