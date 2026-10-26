# System Architecture

The firmware follows the supplied application → device driver → hardware
abstraction → ESP32-S3 DevKitC-1 N16R8 architecture. Source files are registered
explicitly in `main/CMakeLists.txt`; a project-level `components/` directory is
not required. `src/main.cpp` provides the only firmware entry point, `app_main()`.

## Application layer

| Module | Responsibility | Intended dependencies |
| --- | --- | --- |
| `app` | Initialize services and run the main loop | Application modules, shared configuration |
| `data_store` | Keep latest 60 timestamped sensor records in RAM | Shared sensor data types |
| `display_manager` | Format readings, units, calibration/error status for TFT | TFT driver |
| `scheduler` | Schedule sampling, control, and reporting work | Timers/watchdog HAL |
| `sensor_manager` | Read pH/EC, expose calibration and temperature settings; other sensors pending | pH and EC drivers; planned turbidity/INA226 |
| `turbidity_manager` | Scaffold for turbidity sampling, filtering, and monitoring status | Planned turbidity driver calls |
| `state_machine` | Header-only enum: INIT, IDLE, PRIME, RUNNING, FAULT | Transitions and output control pending |
| `pump_manager` | Coordinate three pumps | Pump PWM driver |
| `telemetry` | Report sensor readings, state, and faults | Serial driver |

The state enum is declared in `src/application/state_machine/state_machine.h`.
Transitions, events, guards, and recovery rules must be defined before control
behavior is implemented. The scaffold does not assume automatic priming or a
transition to RUNNING.

EC and turbidity are required sensors. EC is also an input to the planned
theoretical-pH calculation. That calculation belongs in the application layer,
with a separate derived result; it must not replace electrode-measured pH or
be embedded in the EC hardware driver. Required turbidity acquisition and
theoretical-pH output are not implemented yet.

## Device driver layer

| Module | Hardware-facing role | HAL dependency |
| --- | --- | --- |
| `pump_pwm` | Per-pump output control | PWM / LEDC |
| `ph` | pH conversion and calibration | ADC |
| `ec` | EC conversion and calibration | ADC |
| `turbidity` | Scaffold for sensor acquisition and calibration | Pending sensor/interface selection |
| `serial` | Telemetry transport | UART / USB console |
| `tft` | Render status/value text in 240x240 strips | LCD HAL |

Device drivers own device-specific behavior. Application code should use these
interfaces rather than call peripheral SDK functions directly. Turbidity has
driver and application scaffolds with review comments, but no acquisition code.
They are not registered in `main/CMakeLists.txt` or called by the application yet.
The sensor model, electrical interface, pin assignment, and calibration method
must be confirmed before implementation. INA226 remains a planned addition
without source files.

## Hardware abstraction layer

| Module | ESP-IDF headers / facility |
| --- | --- |
| `timers_watchdog` | `freertos/task.h`, `esp_timer.h`, `esp_task_wdt.h` |
| `pwm` | `driver/ledc.h` |
| `adc` | Shared ADC1, 32-sample averages, `adc_oneshot.h`, `adc_cali_scheme.h` |
| `serial` | `driver/uart.h`, configured ESP-IDF logging console |
| `lcd` | SPI2, `esp_lcd` ST7789, DMA strip allocation/completion |

The HAL owns SDK-specific peripheral calls and eventual pin/bus setup. It must
not depend on application managers or device drivers. The scheduler uses the
timer HAL directly; the PWM HAL configures its output pins using ESP-IDF GPIO.
These boundaries are
source organization conventions; the current build registers all layers in one
ESP-IDF component.

Shared configuration owns board pin restrictions and cross-module pin checks.
`app.cpp` passes the same sensor/display settings to both managers; they reject
conflicting pins before initialization. The LCD HAL also validates its GPIOs
through the SDK. Shared reading helpers ensure serial and TFT consumers apply
the same finite/nonnegative-value checks, while drivers own physical ranges.

`serial::write_line()` currently uses the configured ESP-IDF logging console.
Dedicated UART or USB transport initialization remains to be implemented.

## Board and build

The S3 board defaults select 16 MB flash and Octal PSRAM. The isolated S3 build
command in the [project README](../README.md) uses those defaults and an S3
target without replacing the existing `sdkconfig` (now ESP32-S3 with 2 MB flash
and PSRAM disabled).

The Makefile now selects this isolated N16R8 configuration for all its targets.

Pump GPIOs are 4 (70 mL/min inlet), 5 (49 mL/min outlet), and 6 (21 mL/min outlet).
The PWM HAL uses LEDC low-speed timer 0, channels 0..2, at 10 kHz with 10-bit duty
resolution. Shared configuration stores pin mappings, flow targets, and adjustable
duties. The inlet's measured operating point is 70 mL/min at 90% duty; the outlet
duties remain 0% pending calibration. See [pump control](pumps.md).
pH uses GPIO1 directly; EC uses GPIO2 through the bench test's 10k/20k divider.
The drivers target SEN0161-V2 and DFR0300 V2 K=1. Liquid calibration measurements
are still needed; default coefficients are explicitly marked as estimates.
See [sensor acquisition](sensors.md). I2C addresses, interlock polarity, and
dedicated communication transport remain unspecified.

Reference: [Espressif DevKitC-1 board guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.0.html)
and [ESP-IDF build system](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html).

## Current behavior

The entry point calls `app::init()` and `app::run()`. The pump manager initializes
three outputs at zero duty through the pump driver and LEDC HAL. Startup and pump
initialization telemetry travel through the telemetry module, serial driver,
and serial HAL. Both sensors initialize through their drivers and a shared ADC1
HAL. The main loop samples pH and EC, stores a timestamped record in RAM, and
displays/reports readings or per-sensor errors through the TFT and serial output,
then waits for the shared sample interval through the scheduler and timer HAL;
it does not issue pump start commands. Pump APIs support
manual duty adjustment, stored operating duties, coordinated start, and stop.
Sensor calibration/temperature setters are C++ APIs with RAM-only updates; NVS
and serial calibration commands remain pending. Reactor state transitions,
safety interlocks, turbidity, and INA226 acquisition remain unimplemented.

The TFT is GMT130/ST7789 240x240 with SCLK/MOSI/DC/RESET on GPIO12/11/10/9.
Storage includes a turbidity field that remains unavailable until acquisition
is implemented. The 60-record history is RAM-only and resets at reboot.
See [sensor storage and TFT](display.md) for ownership, wiring, and failure handling.

## Planned additions

Implement these features when their hardware and behavior are defined:

- Safety manager and GPIO input support for interlocks and STOP handling.
- State-machine transitions and electrolysis power control.
- Turbidity acquisition and INA226 current/voltage monitoring with I2C support.
- EC-based theoretical-pH calculation, stored separately from measured pH.

Unused common/test starter files and empty `external/` and `tools/` scaffolding
have been removed. Turbidity scaffolds are retained for upcoming development.
ESP-IDF supplies FreeRTOS; no separate external RTOS source tree is needed.

Hardware bench tests remain outside the repository in `../benchtest/`. Pump
manager/driver host tests live in `src/test/` and use a fake PWM HAL. Sensor tests
use a fake ADC to exercise conversions, calibration, errors, and telemetry.
