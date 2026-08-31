## Requirements:
- PlatformIO
- ESP32 microcontroller

If you aren't sure what ESP board to select in PlatformIO, choose "ESP32 Dev Module" - it works on all ESP32 I have tested so far.

![ESP32 Architecture](https://github.com/KinkyMakers/OSSM-hardware/blob/master/PlatformIO%20ESP32%20code/OSSM_ESP32/OSSM%20ESP32%20Architecture.png)


## Logging

This project uses ESP32's [native logging library](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/log.html).


## Commands

**Upload and monitor**
```bash
pio run -t upload -t monitor
```

**Run Unit Test tests**
```bash
pio test -e test
```

## Other Documentation
- [Git Branching Strategy](docs/Git Branching Strategy)
