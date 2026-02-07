# Proof that you can play patterns on the OSSM

# what is a pattern?

A pattern is a short, looping function which describes the position of the OSSM over time.

call this function x(t)

Where 0%<=t<=100%

And 0%<=y<=100%

And where y(t) is a continuous function.

- [ ] A pattern is an array of position, speed in, speed out, and time values, all stored as percents. Ex:

```json
[
  {t: 0, y: 0},
  {t: 50%, y: 100, left_control_slope: 0, left_control_magnitude: 25, right_control_slope:0, right_control_magnitude: 25},
  {t: 100%, y: 0}
]
```

- [ ] This array must have at least 2 points. One at t=0%, one at t=100%.
- [ ] A pattern can have at most 101 points with a gap of t=1 between each.
- [ ] The first and the last points must be the same.

UI / UX for creating and modifying the patterns - DO NOT START until you and AJ review his previous work.

Here's an example pseudocode pattern

# Test Pattern 1

```javascript
[
  {t: 0, y: 0},
  {t: 50%, y: 100},
  {t: 100%, y: 0}
]
```

![image.png](https://uploads.linear.app/abf7c297-b7cb-4842-8624-c8736f04f47b/00936736-dfb0-4346-a88a-0d5d06e145f6/ae2c1fc6-4659-4992-abdd-bd387a5d86fb)

# Test Pattern 2

```json
[
  {t: 0, y: 0},
  {t: 50%, y: 100, left_control_slope: 0, left_control_magnitude: 25, right_control_slope:0, right_control_magnitude: 25},
  {t: 100%, y: 0}
]
```

![image.png](https://uploads.linear.app/abf7c297-b7cb-4842-8624-c8736f04f47b/618e62b8-4b09-4883-9d32-50a78916b7cc/007835f6-bd50-4f9f-b616-b22e6c9c6d8f)

# Implementation Questions from Burns

Patterns will be the dashboard

---

```javascript
const pattern = [
  {t: 0, x: 0},
  {t: 0.5, x: 1},
  {t: 1, x: 0}
]
```

# X calculation

x = 0% implies the position is `min_position = depth - depth * stroke`

x = 60% implies `= depth * ( 1 - stroke + stroke * 60% )`

x = 100% implies position is `max_position = depth`

this means that

`position = min_position + (max_position - min_position) * x`

`position = depth - depth * stroke + depth * stroke * x`

`position = depth * ( 1 - stroke + stroke * x)`

Everything above is a percent of `maxStrokeSteps`

You will use this as follows (pseudocode)

```
ossm->stepper.setTar  get(position * ossm->maxStrokeSteps * (1_mm))
```

# Speed Calculation

delta t = 1/3 seconds \* 1/speed_percent

pseudo code.

Assume Speed = 100%

```csharp
last_point = {t:0, x:0}
target_point = {t:50%, x:100%}
current_progress = 0; // time percent on graph
delta_t = 0.333 * 1 / (speed = 100%) = 333 milliseconds

current_progress = ms_elapsed / delta_t

// for example
ms_elapse = 333 ms
delta_t = 333 ms

current_progress = 1

// so we wait
ms_elapse = 3330 ms
delta_t = 333 ms

current_progress = 10

// so we wait
ms_elapse = 16650 ms
delta_t = 333 ms

current_progress = 50

// And now! We update the last point and target point. Because we're at our t = 50%
```



So what does this actually mean?

Speed = 900 mm /s = 100%

t = 0 to t=1 should take 1/3 of second.

t=0 to t=100 should take 100/3 seconds

Speed = 450 mm /s = 50%

t = 0 to t=1 should take 2/3 of second.

t=0 to t=100 should take 100/3 seconds

Speed = 225 mm /s = 25%

t = 0 to t=1 should take 4/3 of second.

Speed = 112.5 mm /s = 12.5%

t = 0 to t=1 should take 8/3 of second.

Speed = 0 mm /s = 0%

t = 0 to t=1 should take infinity of second.

time between steps = (1/3 s) \* 1/speed

Position = 100% = maxStrokeSteps \~= 300 mm

Position = 0 = 0mm = home.

Position is a function of Depth and Stroke

**Calculations of min & max position: -----**

Max Position = Depth = x = 100%

Min Position = Depth - Depth \* Stroke = x = 0%

Depth = 100%

Stroke = 100%

Max Position =  100%

Min Position = 100% - (100% \* 100%) = 0%

Depth = 100%

Stroke = 50%

Max Position = 100%

Min Position = 100% - (100% \* 50% ) = 50%

Depth = 50%

Stroke = 50%

Max Position = 50%

Min Position = 50% - (50% \* 50% ) = 25%
