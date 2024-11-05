# Include Directory

This directory currently contains the Boost.SML header-only library (`boost/sml.hpp`) for state machine functionality.

## Current Status

While header-only libraries can be placed in either `lib/` or `include/`, the current placement in `include/` is suboptimal. The `lib/` directory would be more appropriate because:

1. It maintains consistency with PlatformIO's library management practices
2. It groups all third-party dependencies in one location
3. It follows the convention established by other dependencies in the project

### Future Changes

The Boost.SML header will be relocated to the `lib/` directory in a future update to align with PlatformIO best practices.

## Adding Headers

For most dependencies, prefer:
1. Adding them to `platformio.ini` if available through the PlatformIO Registry
2. Placing them in the `lib/` directory for third-party libraries
3. Using this directory only for project-specific headers

For more information about PlatformIO dependency management, see:
- [Library Manager](https://docs.platformio.org/en/latest/librarymanager/index.html)
- [Project Structure](https://docs.platformio.org/en/latest/projectconf/sections/platformio.html#projectconf-pio-src-dir)