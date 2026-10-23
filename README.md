# Reactor Firmware (Initial Frame)

This repository contains the initial C++ firmware project frame for the reactor system.

## Structure

- `build/` Generated build output
- `docs/` Project documentation
- `src/` Host-side source skeleton
- `components/` Sensor/actuator modules
- `benchtest/` Per-component bench tests
- `tools/` Development scripts and helper tools
- `main/` ESP-IDF firmware entry point (`main.cpp`)

## Build (ESP-IDF)

```bash
idf.py build
```

## Notes

This is intentionally minimal and meant for incremental implementation.
