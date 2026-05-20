# Libraries Directory

This directory contains project-specific libraries that cannot be installed directly through platformio.ini dependencies.

## Current Libraries

- **StrokeEngine**: Modified version of [Elims' StrokeEngine](https://github.com/theelims/StrokeEngine) customized for OSSM usage.

- **ruckig** (`lib/ruckig/`): Vendored [Ruckig](https://github.com/pantor/ruckig) Community Edition (MIT, version pinned in `library.json`). Jerk-limited online trajectory generation for future motion control. Built with C++ exceptions (`-fexceptions` in `library.json`); cloud/waypoints client headers are not shipped (`WITH_CLOUD_CLIENT` is never defined).

- **toppra** (`lib/toppra/`): Vendored time-parameterization library (not wired into firmware yet).

## Using Ruckig in firmware

```cpp
#include <ruckig/ruckig.hpp>

// 1-DOF example (see test/test_ruckig_smoke for a minimal Unity smoke test)
ruckig::Ruckig<1> otg(0.01);  // cycle time [s]
ruckig::InputParameter<1> input;
ruckig::OutputParameter<1> output;
// ... fill input limits and state ...
ruckig::Result result = otg.update(input, output);
```

PlatformIO auto-discovers `lib/ruckig` via LDF; no `lib_deps` entry is required. Use `-std=gnu++17` and `-fexceptions` on environments that compile or link Ruckig (see `development` in `platformio.ini`).

## Adding Libraries

For most dependencies, prefer adding them to `platformio.ini`. Only place libraries here when:
- They require local modifications
- They aren't available through PlatformIO's Library Registry
- They need to be version controlled with the project

For more information about PlatformIO library management, see:
- [Library Manager](https://docs.platformio.org/en/latest/librarymanager/index.html)
- [Library Dependencies](https://docs.platformio.org/en/latest/librarymanager/dependencies.html)
