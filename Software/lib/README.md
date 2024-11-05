# Libraries Directory

This directory contains project-specific libraries that cannot be installed directly through platformio.ini dependencies.

## Current Libraries

- **StrokeEngine**: Modified version of [Elims' StrokeEngine](https://github.com/theelims/StrokeEngine) customized for OSSM usage.

## Adding Libraries

For most dependencies, prefer adding them to `platformio.ini`. Only place libraries here when:
- They require local modifications
- They aren't available through PlatformIO's Library Registry
- They need to be version controlled with the project

For more information about PlatformIO library management, see:
- [Library Manager](https://docs.platformio.org/en/latest/librarymanager/index.html)
- [Library Dependencies](https://docs.platformio.org/en/latest/librarymanager/dependencies.html)