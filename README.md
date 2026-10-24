# Reactor Firmware

ESP-IDF C++ firmware scaffold organized around the reactor system architecture.
The design targets the ESP32-S3 DevKitC-1 N16R8 and uses the installed SDK's
default C++ standard (currently C++26).

## Structure

```text
reactor/
├── main/CMakeLists.txt          # Registers firmware sources and SDK dependencies
├── src/
│   ├── main.cpp                # Single ESP-IDF app_main entry point
│   ├── application/
│   │   ├── app.{h,cpp}          # Application orchestration
│   │   ├── scheduler/          # Timer / scheduler
│   │   ├── safety_manager/     # Interlocks / emergency stop
│   │   ├── sensor_manager/     # Read / filter / calibrate
│   │   ├── state_machine/      # INIT / IDLE / PRIME / RUNNING / FAULT
│   │   ├── pump_manager/       # Pump 1 / Pump 2 / Pump 3
│   │   └── telemetry/          # Telemetry / serial monitor
│   ├── driver/
│   │   ├── pump_pwm/           # Pump PWM driver
│   │   ├── ph/                 # pH driver
│   │   ├── ec/                 # EC driver
│   │   ├── turbidity/          # Turbidity driver
│   │   ├── ina226/             # Candidate current / voltage monitor
│   │   └── serial/             # USB / UART driver
│   ├── hal/
│   │   ├── timers_watchdog/    # Timers / watchdog
│   │   ├── gpio/               # GPIO
│   │   ├── pwm/                # PWM / LEDC
│   │   ├── adc/                # ADC
│   │   ├── i2c/                # I2C
│   │   └── serial/             # UART / USB console
│   ├── common/                # Shared configuration and utilities
│   └── test/                  # Pump host tests; outside firmware build
├── docs/                      # Architecture and module responsibilities
├── external/                  # Third-party RTOS integration placeholder
├── tools/                     # Development scripts and helper tools
├── sdkconfig                  # Existing active target configuration
├── sdkconfig.defaults.esp32s3  # N16R8 board defaults
└── build/                     # Generated output (ignored)
```

Each module directory contains a matching header and implementation file.
See [the architecture guide](docs/architecture.md) for the layer boundaries.

## Build (ESP-IDF)

Activate the ESP-IDF environment first. To build for the architecture's
ESP32-S3 N16R8 board using a separate build directory and configuration:

```bash
idf.py -B build/esp32s3 \
  -D IDF_TARGET=esp32s3 \
  -D SDKCONFIG=build/esp32s3/sdkconfig \
  -D SDKCONFIG_DEFAULTS=sdkconfig.defaults.esp32s3 build
```

Use the same options with `flash monitor` to flash that build. The board profile
selects 16 MB flash and Octal PSRAM. Pump GPIOs are assigned in shared configuration.

`idf.py build` continues to use the existing root `sdkconfig` (currently ESP32-S3,
2 MB flash, PSRAM disabled).
The separate S3 build preserves that configuration.

## Notes

Local per-component bench tests live in the sibling folder `../benchtest/`,
outside this repository and its firmware build.

The pump manager configures three PWM outputs at startup, all initially off:
GPIO 4 is the 70 mL/min inlet, GPIO 5 is the 49 mL/min outlet, and GPIO 6 is the
21 mL/min outlet. PWM uses 10 kHz and 10-bit resolution. The inlet's saved duty is
90%, measured at 70 mL/min; both outlet duties remain 0% pending calibration.
Saved duties are applied only when explicitly commanded, not at startup.
See [pump control](docs/pumps.md) for APIs, calibration, and host tests.

Startup logging and the idle scheduler delay are wired through the layers.
Sensor acquisition, safety interlocks, watchdog management, state transitions,
operator commands, and sensor telemetry remain to be implemented. The main loop
does not start pumps automatically.
