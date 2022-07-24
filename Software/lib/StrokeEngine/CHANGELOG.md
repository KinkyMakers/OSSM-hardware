# Release 0.3.0
- set and get functions for maximum speed and maximum acceleration. Allows to change these limits during runtime.
- Renamed `#define DEBUG_VERBOSE` to `#define DEBUG_TALKATIVE` to make StrokeEngine play nice with WifiManager.
- Removed state `ERROR` from state machine. After tests with the servo this has become pointless. `disable()` and homing will clear any error state. 
- Fixed bug with missing implementation of inverted direction signal: https://github.com/theelims/StrokeEngine/issues/3
- Added an improved pause handling mechanism to pattern:
  - removed `void patternDelay(int milliseconds)` function as unfit for the purpose.
  - Pattern-Class has 3 private functions `void _startDelay()`, `void _updateDelay(int delayInMillis)` and `bool _isStillDelayed()` to add an internal delay function based on comparing `millis()`.
  - A pattern can request StrokeEngine to skip the current motion command by returning `_nextMove.skip = true;`. 
- Steps per mm, speed and acceleration limits are available inside pattern, now. 
- Refactored code to improve thread safety: 
  - Pinned the working threads to the application core 1.
  - During state `PATTERN` only the `_stroking()`-Thread communicates with the FastAccelStepper library. Mid-stroke parameter updates only set a flag for the `_stroking()`-Thread to query the pattern for an updated motion calculation.
  - All accesses to a pattern that influence it's internal states (set-functions & `nextTarget()`) are made thread safe and guarded by a mutex.
  - Function `applyNewSettingsNow()` had to disappear. Set-functions got an additional bool `applyNow` to apply the changes immediately. Value defaults to `false` which means changes take effect only with the next stroke.
- It is now possible to receive telemetry information's about each trapezoidal motion a pattern generates. You may register a callback function `void registerTelemetryCallback(void(*callbackTelemetry)(float, float, bool))` with the following signature `void callbackTelemetry(float position, float speed, bool clipping)`. 
- New pattern available:
  - Stop'n'Go
  - Insist
  - Jack Hammer
  - Stroke Nibble

## Update Notes
### Mid-Stroke Parameter Updates
The function `applyNewSettingsNow()` had to disappear to make the code thread safe. However, all set-functions got an additional parameter `applyNow` of type bool to apply the changes immediately. This parameter defaults to `false` which means changes take effect only with the next stroke. To get the same functionality as calling `applyNewSettingsNow()` append `true` as the last parameter in all set-function calls.
```cpp
Stroker.setSpeed(float speed, bool applyNow);          // Speed in Cycles (in & out) per minute, constrained from 0.5 to 6000
Stroker.setDepth(float depth, bool applyNow);          // Depth in mm, constrained to [0, _travel]
Stroker.setStroke(float stroke, bool applyNow);        // Stroke length in mm, constrained to [0, _travel]
Stroker.setSensation(float sensation, bool applyNow);  // Sensation (arbitrary value a pattern may use to alter its behavior), 
                                                       // constrained to [-100, 100] with 0 being neutral.
Stroker.setPattern(int index, bool applyNow);          // Pattern, index must be < Stroker.getNumberOfPattern()
```

# Release 0.2.0
- Stroking-task uses suspend and resume instead of deleting and recreation to prevent heap fragmentation.
- Refined homing procedure. Fully configurable via a struct including front or back  switch and internal pull-up or pull-down resistors. 
- Added a delay-function for patterns: `void patternDelay(int milliseconds)`
- Debug messages report real world units instead of steps.
- Special debug messages for clipping when speed or acceleration limits are hit.
- Several changes how pattern feed their information back:
  - Removed `setDepth()`, as this information is not needed inside a pattern.
  - Pattern return relative stroke and not absolute coordinates.
  - Depth-offset is calculated and properly constrained inside StrokeEngine.
  - All pattern updated accordingly. 
  - Also fixed the erratic move bug. When depth changes the necessary transfer motion is carried out at the same speed as the overall motion.
- Some minor default parameter changes in the example code snippets.

## Update Notes
### Homing Procedure
New syntax for `Stroker.enableAndHome()`. It now expects a pointer to a `endstopProperties`-Struct configuring the  behavior.
```cpp
// Configure Homing Procedure
static endstopProperties endstop = {
  .homeToBack = true,             // Endstop sits at the rear of the machine
  .activeLow = true,              // switch is wired active low
  .endstopPin = SERVO_ENDSTOP,    // Pin number
  .pinMode = INPUT                // pinmode INPUT with external pull-up resistor
};

...

// Home StrokeEngine
Stroker.enableAndHome(&endstop);
```

# Release 0.1.1
- Constructor of Pattern-class changed to type `const char*` to get rid of compiler warning.
- Fixed homing-Bug.

# Release 0.1.0
- First "official" release
- Renamed function `Stroker.startMotion()` to `Stroker.startPattern()`
- Renamed all states of the state machine
- Provide set and get functions for maxSpeed and maxAcceleration
- Fancy adjustment mode
- Changed members of struct `motorProperties` to streamline interface:
  - introduced `maxSpeed`
  - deleted `maxRPM` and `stepsPerRevolution`
