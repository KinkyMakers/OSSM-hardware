#include "StrokeEngine.h"

#include <Arduino.h>

#include "pattern.h"

#ifdef DEBUG_TALKATIVE
static constexpr const char *verboseState[] = {
    "[0] Servo disabled", "[1] Servo ready", "[2] Servo pattern running"};
#endif

// static pointer to engine and _servo
void StrokeEngine::begin(machineGeometry *physics, motorProperties *motor,
                         FastAccelStepper *servo) {
    _servo = servo;
    // store the machine geometry and motor properties pointer
    _physics = physics;
    _motor = motor;

    // Derived Machine Geometry & Motor Limits in steps:
    _travel = (_physics->physicalTravel - (2 * _physics->keepoutBoundary));
    _minStep = 0;
    _maxStep = int(0.5 + _travel * _motor->stepsPerMillimeter);
    _maxStepPerSecond =
        int(0.5 + _motor->maxSpeed * _motor->stepsPerMillimeter);
    _maxStepAcceleration =
        int(0.5 + _motor->maxAcceleration * _motor->stepsPerMillimeter);

    // Initialize with default values
    _state = UNDEFINED;
    _index = 0;
    _depth = _maxStep;
    _stroke = _maxStep / 3;
    _speedPercent = 0.0f;
    _sensation = 0.0;
    _recalcTimeOfStroke();

    if (_servo) {
        _servo->setDirectionPin(_motor->directionPin, _motor->invertDirection);
        _servo->setEnablePin(_motor->enablePin, _motor->enableActiveLow);
        _servo->setAutoEnable(false);
        _servo->disableOutputs();
    }
    Serial.println("_servo initialized");

#ifdef DEBUG_TALKATIVE
    Serial.print("Stroke Engine State: ");
    Serial.println(verboseState[_state]);
#endif
}

void StrokeEngine::setSpeed(float speedPercent, bool applyNow = false) {
    if (xSemaphoreTake(_patternMutex, portMAX_DELAY) == pdTRUE) {
        _speedPercent = constrain(speedPercent, 0.0f, 100.0f);
        _recalcTimeOfStroke();

        pattern->setTimeOfStroke(_timeOfStroke);

#ifdef DEBUG_TALKATIVE
        Serial.println("setSpeed: " + String(_speedPercent, 2) +
                       "% -> T=" + String(_timeOfStroke, 3) + "s");
#endif

        // When running a pattern and immediate update requested:
        if ((_state == PATTERN) && (applyNow == true)) {
            // set flag to apply update from stroking thread
            _applyUpdate = true;

#ifdef DEBUG_TALKATIVE
            Serial.println("Apply New Settings Now");
#endif
        }

        // give back mutex
        xSemaphoreGive(_patternMutex);
    }
}

void StrokeEngine::setDepth(float depth, bool applyNow = false) {
    if (xSemaphoreTake(_patternMutex, portMAX_DELAY) == pdTRUE) {
        // Convert depth from mm into steps
        // Constrain depth between minStep and maxStep
        _depth = constrain(int(depth * _motor->stepsPerMillimeter), _minStep,
                           _maxStep);

        pattern->setDepth(_depth);

#ifdef DEBUG_TALKATIVE
        Serial.println("setDepth: " + String(_depth));
#endif
        // When running a pattern and immediate update requested:
        if ((_state == PATTERN) && (applyNow == true)) {
            // set flag to apply update from stroking thread
            _applyUpdate = true;

#ifdef DEBUG_TALKATIVE
            Serial.println("Apply New Settings Now");
#endif
        }

        // give back mutex
        xSemaphoreGive(_patternMutex);
    }
}

void StrokeEngine::setStroke(float stroke, bool applyNow = false) {
    // Update pattern with new stroke, will be used with the next stroke or on
    // update request
    if (xSemaphoreTake(_patternMutex, portMAX_DELAY) == pdTRUE) {
        // Convert stroke from mm into steps
        // Constrain stroke between minStep and maxStep
        _stroke = constrain(int(stroke * _motor->stepsPerMillimeter), _minStep,
                            _maxStep);

        pattern->setStroke(_stroke);

        _recalcTimeOfStroke();
        pattern->setTimeOfStroke(_timeOfStroke);

#ifdef DEBUG_TALKATIVE
        Serial.println("setStroke: " + String(_stroke));
#endif

        // When running a pattern and immediate update requested:
        if ((_state == PATTERN) && (applyNow == true)) {
            // set flag to apply update from stroking thread
            _applyUpdate = true;

#ifdef DEBUG_TALKATIVE
            Serial.println("Apply New Settings Now");
#endif
        }

        // give back mutex
        xSemaphoreGive(_patternMutex);
    }
}

void StrokeEngine::setSensation(float sensation, bool applyNow = false) {
    // Update pattern with new sensation, will be used with the next stroke or
    // on update request
    if (xSemaphoreTake(_patternMutex, portMAX_DELAY) == pdTRUE) {
        // Constrain sensation between -100 and 100
        _sensation = constrain(sensation, -100, 100);

        pattern->setSensation(_sensation);

#ifdef DEBUG_TALKATIVE
        Serial.println("setSensation: " + String(_sensation));
#endif

        // When running a pattern and immediate update requested:
        if ((_state == PATTERN) && (applyNow == true)) {
            // set flag to apply update from stroking thread
            _applyUpdate = true;

#ifdef DEBUG_TALKATIVE
            Serial.println("Apply New Settings Now");
#endif
        }

        // give back mutex
        xSemaphoreGive(_patternMutex);
    }
}

bool StrokeEngine::setPattern(Pattern *NextPattern,
                              bool applyNow = false) {
    // Free up memory from previous pattern

    delete pattern;
    pattern = NextPattern;

    // Inject current motion parameters into new pattern
    if (xSemaphoreTake(_patternMutex, portMAX_DELAY) == pdTRUE) {
        pattern->setSpeedLimit(_maxStepPerSecond, _maxStepAcceleration,
                              _motor->stepsPerMillimeter);
        pattern->setTimeOfStroke(_timeOfStroke);
        pattern->setStroke(_stroke);
        pattern->setDepth(_depth);
        pattern->setSensation(_sensation);

        // When running a pattern and immediate update requested:
        if ((_state == PATTERN) && (applyNow == true)) {
            // set flag to apply update from stroking thread
            _applyUpdate = true;

#ifdef DEBUG_TALKATIVE
            Serial.println("Apply New Settings Now");
#endif
        }

        // Reset index counter
        _index = -1;

        // give back mutex
        xSemaphoreGive(_patternMutex);
    }

#ifdef DEBUG_TALKATIVE
    Serial.println("setPattern: " + String(pattern->getName()));
    Serial.println("setTimeOfStroke: " + String(_timeOfStroke, 2));
    Serial.println("setDepth: " + String(_depth));
    Serial.println("setStroke: " + String(_stroke));
    Serial.println("setSensation: " + String(_sensation));
#endif
    return true;
}

bool StrokeEngine::startPattern() {
    // Only valid if state is ready
    if (_state == READY) {
        // Stop current move, should one be pending
        if (_servo->isRunning()) {
            // Stop _servo motor as fast as legally allowed
            _servo->setAcceleration(_maxStepAcceleration);
            _servo->applySpeedAcceleration();
            _servo->stopMove();
        }

        // Set state to PATTERN
        _state = PATTERN;

        // Reset Stroke and Motion parameters
        _index = -1;
        if (xSemaphoreTake(_patternMutex, portMAX_DELAY) == pdTRUE) {
            pattern->setSpeedLimit(_maxStepPerSecond, _maxStepAcceleration,
                                  _motor->stepsPerMillimeter);
            pattern->setTimeOfStroke(_timeOfStroke);
            pattern->setStroke(_stroke);
            pattern->setDepth(_depth);
            pattern->setSensation(_sensation);
            xSemaphoreGive(_patternMutex);
        }

#ifdef DEBUG_TALKATIVE
        Serial.print(" _timeOfStroke: " + String(_timeOfStroke));
        Serial.print(" | _depth: " + String(_depth));
        Serial.print(" | _stroke: " + String(_stroke));
        Serial.println(" | _sensation: " + String(_sensation));
#endif

        if (_taskStrokingHandle == NULL) {
            // Create Stroke Task
            xTaskCreatePinnedToCore(
                this->_strokingImpl,   // Function that should be called
                "Stroking",            // Name of the task (for debugging)
                4096,                  // Stack size (bytes)
                this,                  // Pass reference to this class instance
                24,                    // Pretty high task priority
                &_taskStrokingHandle,  // Task handle
                1                      // Pin to application core
            );
        } else {
            // Resume task, if it already exists
            vTaskResume(_taskStrokingHandle);
        }

#ifdef DEBUG_TALKATIVE
        Serial.println("Started motion task");
        Serial.print("Stroke Engine State: ");
        Serial.println(verboseState[_state]);
#endif

        return true;

    } else {
#ifdef DEBUG_TALKATIVE
        Serial.println("Failed to start motion");
#endif
        return false;
    }
}

void StrokeEngine::stopMotion() {
    // only valid when
    if (_state == PATTERN) {
        // Set state
        _state = READY;

        // Stop _servo motor as fast as legally allowed
        _servo->setAcceleration(_maxStepAcceleration);
        _servo->applySpeedAcceleration();
        _servo->stopMove();

#ifdef DEBUG_TALKATIVE
        Serial.println("Motion stopped");
#endif

        // Wait for _servo stopped
        while (_servo->isRunning())
            ;
    }

#ifdef DEBUG_TALKATIVE
    Serial.print("Stroke Engine State: ");
    Serial.println(verboseState[_state]);
#endif
}

void StrokeEngine::thisIsHome(float, bool resetOrigin) {
    if (_state == UNDEFINED) {
        // Enable _servo
        _servo->enableOutputs();

        // Only (re-)establish the home origin on a genuine fresh physical home.
        // On a re-entry (resetOrigin == false) the carriage was NOT re-homed,
        // so the position counter is already valid in the established frame;
        // re-basing it here is what accumulated ~6 mm of drift per entry.
        if (resetOrigin) {
            // Home == -keepoutBoundary, a constant independent of the current
            // counter. (The previous abs()-of-current-position re-based the
            // origin off wherever the carriage happened to sit.)
            _servo->setCurrentPosition(-_motor->stepsPerMillimeter *
                                       _physics->keepoutBoundary);
        }

        // Change state
        _state = READY;

#ifdef DEBUG_TALKATIVE
        Serial.println("This is Home now");
#endif

        return;
    }

#ifdef DEBUG_TALKATIVE
    Serial.println("Manual homing failed. Not in state UNDEFINED");
#endif
}

ServoState StrokeEngine::getState() { return _state; }

void StrokeEngine::_stroking() {
    motionParameter currentMotion;

    while (1) {  // infinite loop

        // Suspend task, if not in PATTERN state
        if (_state != PATTERN) {
            vTaskSuspend(_taskStrokingHandle);
        }

        // Take mutex to ensure no interference / race condition with
        // communication threat on other core
        if (xSemaphoreTake(_patternMutex, 0) == pdTRUE) {
            if (_applyUpdate == true) {
                // Ask pattern for update on motion parameters
                currentMotion = pattern->nextTarget(_index);

                // Increase deceleration if required to avoid crash
                if (_servo->getAcceleration() > currentMotion.acceleration) {
#ifdef DEBUG_CLIPPING
                    Serial.print("Crash avoidance! Set Acceleration from " +
                                 String(currentMotion.acceleration));
                    Serial.println(" to " + String(_servo->getAcceleration()));
#endif
                    currentMotion.acceleration = _servo->getAcceleration();
                }

                // Apply new trapezoidal motion profile to _servo
                _applyMotionProfile(&currentMotion);

                // clear update flag
                _applyUpdate = false;
            }

            // If motor has stopped issue moveTo command to next position
            else if (_servo->isRunning() == false) {
                // Increment index for pattern
                _index++;

                // Querey new set of pattern parameters
                currentMotion = pattern->nextTarget(_index);

                // Pattern may introduce pauses between strokes
                if (currentMotion.skip == false) {
#ifdef DEBUG_STROKE
                    Serial.println("Stroking Index: " + String(_index));
#endif
                    // Apply new trapezoidal motion profile to _servo
                    _applyMotionProfile(&currentMotion);

                } else {
                    // decrement _index so that it stays the same until the next
                    // valid stroke parameters are delivered
                    _index--;
                }
            }

            // give back mutex
            xSemaphoreGive(_patternMutex);
        }

        // Delay 10ms
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void StrokeEngine::_applyMotionProfile(motionParameter *motion) {
    // Apply new trapezoidal motion profile to _servo if pattern does not skip
    if (motion->skip == false) {
        // Constrain speed to below _maxStepPerSecond
        if (motion->speed > _maxStepPerSecond) {
#ifdef DEBUG_CLIPPING
            Serial.println(
                "Max Speed Exceeded: " +
                String(float(motion->speed / _motor->stepsPerMillimeter), 2) +
                "mm/s --> Limit: " +
                String(float(_maxStepPerSecond / _motor->stepsPerMillimeter),
                       2) +
                "mm/s");
#endif
            motion->speed = _maxStepPerSecond;
        }

        // Constrain acceleration between 1 step/sec^2 and _maxStepAcceleration
        if (motion->acceleration > _maxStepAcceleration) {
#ifdef DEBUG_CLIPPING
            Serial.println(
                "Max Acceleration Exceeded: " +
                String(float(motion->acceleration / _motor->stepsPerMillimeter),
                       2) +
                "mm/s² --> Limit: " +
                String(float(_maxStepAcceleration / _motor->stepsPerMillimeter),
                       2) +
                "mm/s²");
#endif
            motion->acceleration = _maxStepAcceleration;
        }

        // Constrain stroke to motion envelope
        int pos = constrain((motion->stroke), _minStep, _maxStep);

        // write values to _servo
        _servo->setSpeedInHz(motion->speed);
        _servo->setAcceleration(motion->acceleration);
        _servo->moveTo(pos);

#ifdef DEBUG_STROKE
        const float speed = float(motion->speed / _motor->stepsPerMillimeter);
        const float position = float(pos / _motor->stepsPerMillimeter);
        Serial.println("motion.stroke: " + String(position, 2) + "mm");
        Serial.println("motion.speed: " + String(speed, 2) + "mm/s");
        Serial.println(
            "motion.acceleration: " +
            String(float(motion->acceleration / _motor->stepsPerMillimeter),
                   2) +
            "mm/s²");
#endif
    }
}

void StrokeEngine::_recalcTimeOfStroke() {
    // Every built-in pattern produces peakStepsPerSec = 3 * stroke / T at
    // neutral sensation. Solve for T given the desired peak as a percentage
    // of the motor's max step rate.
    if (_stroke <= 0 || _maxStepPerSecond <= 0 || _speedPercent <= 0.0f) {
        _timeOfStroke = 120.0f;
        return;
    }
    float desiredPeak = (_speedPercent / 100.0f) * float(_maxStepPerSecond);
    _timeOfStroke =
        constrain(3.0f * float(_stroke) / desiredPeak, 0.01f, 120.0f);
}
