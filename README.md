# Reactor Firmware

ESP-IDF C++ firmware organized around the reactor system architecture.
The design targets the ESP32-S3 DevKitC-1 N16R8 and uses the installed SDK's
default C++ standard (currently C++26).

Current stage: pH/EC monitoring, 60-sample RAM storage, TFT/serial output, and
pump control APIs are implemented. EC and turbidity are required sensors;
turbidity acquisition and EC-based theoretical-pH calculation remain pending,
along with automatic reactor operation and full safety handling. The latest
verification passed 26 host tests and the N16R8 firmware build; integrated hardware testing remains.
See [development progress](docs/progress.md) for the status and next steps.

## Structure

```text
reactor/
├── main/CMakeLists.txt          # Registers firmware sources and SDK dependencies
├── src/
│   ├── main.cpp                # Single ESP-IDF app_main entry point
│   ├── application/
│   │   ├── app.{h,cpp}          # Application orchestration
│   │   ├── data_store/         # Latest 60 sensor records in RAM
│   │   ├── display_manager/    # Sensor values and status for TFT
│   │   ├── scheduler/          # Timer / scheduler
│   │   ├── sensor_manager/     # Read / filter / calibrate
│   │   ├── turbidity_manager/  # Turbidity monitoring scaffold
│   │   ├── state_machine/      # State enum only; transitions pending
│   │   ├── pump_manager/       # Pump 1 / Pump 2 / Pump 3
│   │   └── telemetry/          # Telemetry / serial monitor
│   ├── driver/
│   │   ├── pump_pwm/           # Pump PWM driver
│   │   ├── ph/                 # pH driver
│   │   ├── ec/                 # EC driver
│   │   ├── turbidity/          # Turbidity driver scaffold
│   │   ├── tft/                # Text renderer for 240x240 TFT
│   │   └── serial/             # USB / UART driver
│   ├── hal/
│   │   ├── timers_watchdog/    # Timers / watchdog
│   │   ├── pwm/                # PWM / LEDC
│   │   ├── adc/                # ADC
│   │   ├── lcd/                # SPI2 / ST7789 / DMA transfers
│   │   └── serial/             # UART / USB console
│   ├── common/                # Shared configuration and utilities
│   └── test/                  # Pump, sensor, storage/display host tests
├── docs/                      # Architecture and module responsibilities
├── sdkconfig                  # Existing active target configuration
├── sdkconfig.defaults.esp32s3  # N16R8 board defaults
└── build/                     # Generated output (ignored)
```

Implemented modules have matching headers and implementation files. The
`state_machine/` directory currently contains only the planned state enum.
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

The Makefile uses those same settings: `make build`, `make flash`, and
`make monitor` all select the N16R8 build. `make clean` cleans only that build.

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

pH (SEN0161-V2) uses GPIO1; EC (DFR0300 V2 K=1) uses GPIO2 through a 10k/20k
voltage divider. The main loop averages 32 ADC samples per sensor and prints
readings approximately once per second. Default coefficients are marked as
uncalibrated estimates. EC uses a manual temperature setting, initially 25 C.
See [sensor acquisition](docs/sensors.md) for wiring, calibration, and tests.

Each sample is saved with its uptime and status in a 60-record RAM history,
then sent to serial telemetry and the GMT130/ST7789 240x240 TFT. The TFT uses
GPIO12 SCLK, GPIO11 MOSI, GPIO10 DC, and GPIO9 RESET on the DevKitC-1 N16R8.
Turbidity displays unavailable until its acquisition code is implemented.
RAM history clears at power-off; there is no persistent log yet.
See [sensor storage and TFT](docs/display.md) for the data flow and wiring.

Safety interlocks, watchdog management, state transitions, operator commands,
NVS calibration storage, and the other sensors remain to be implemented. The
main loop does not start pumps automatically.

Turbidity driver and application scaffolds are reserved for the next sensor
implementation and are not registered in the firmware build yet. Other planned
additions are recorded in the architecture guide. FreeRTOS is supplied by ESP-IDF.

See the [code review](docs/code_review.md) for consistency fixes and maintenance guidance.
