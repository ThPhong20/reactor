# System Architecture

The firmware follows the supplied application → device driver → hardware
abstraction → ESP32-S3 DevKitC-1 N16R8 architecture. Source files are registered
explicitly in `main/CMakeLists.txt`; a project-level `components/` directory is
not required. `src/main.cpp` provides the only firmware entry point, `app_main()`.

## Application layer

| Module | Responsibility | Intended dependencies |
| --- | --- | --- |
| `app` | Initialize services and run the main loop | Application modules, shared configuration |
| `scheduler` | Schedule sampling, control, and reporting work | Timers/watchdog HAL |
| `safety_manager` | Evaluate interlocks and emergency stop; force FAULT and stop pumps | GPIO HAL, state machine, pump manager |
| `sensor_manager` | Read, filter, and calibrate sensor samples | pH, EC, turbidity, optional INA226 drivers |
| `state_machine` | INIT, IDLE, PRIME, RUNNING, FAULT | Scheduler events, safety status, sensor readings, pump manager |
| `pump_manager` | Coordinate three pumps | Pump PWM driver |
| `telemetry` | Report sensor readings, state, and faults | Serial driver |

The state enum is declared in `src/application/state_machine/state_machine.h`.
Transitions, events, guards, and recovery rules must be defined before control
behavior is implemented. The scaffold does not assume automatic priming or a
transition to RUNNING.

## Device driver layer

| Module | Hardware-facing role | HAL dependency |
| --- | --- | --- |
| `pump_pwm` | Per-pump output control | PWM / LEDC |
| `ph` | pH conversion and calibration | ADC |
| `ec` | EC conversion and calibration | ADC |
| `turbidity` | Turbidity conversion and calibration | ADC |
| `ina226` | Candidate current / voltage monitoring | I2C |
| `serial` | Telemetry transport | UART / USB console |

Device drivers own device-specific behavior. Application code should use these
interfaces rather than call peripheral SDK functions directly. INA226 remains a
candidate: its scaffold is compiled, but no device is initialized or polled.

## Hardware abstraction layer

| Module | ESP-IDF headers / facility |
| --- | --- |
| `timers_watchdog` | `freertos/task.h`, `esp_timer.h`, `esp_task_wdt.h` |
| `gpio` | `driver/gpio.h` |
| `pwm` | `driver/ledc.h` |
| `adc` | `esp_adc/adc_oneshot.h` |
| `i2c` | `driver/i2c_master.h` |
| `serial` | `driver/uart.h`, configured ESP-IDF logging console |

The HAL owns SDK-specific peripheral calls and eventual pin/bus setup. It must
not depend on application managers or device drivers. Managers use the timer
and GPIO HAL directly where shown in the architecture. These boundaries are
source organization conventions; the current build registers all layers in one
ESP-IDF component.

`serial::write_line()` currently uses the configured ESP-IDF logging console.
Dedicated UART or USB transport initialization remains to be implemented.

## Board and build

The S3 board defaults select 16 MB flash and Octal PSRAM. The isolated S3 build
command in the [project README](../README.md) uses those defaults and an S3
target without replacing the existing ESP32 `sdkconfig`.

GPIO assignments, sensor calibration data, PWM frequency and duty resolution,
I2C addresses, interlock polarity, and communication transport have not been
specified. Add those choices to shared configuration as the hardware is wired.

Reference: [Espressif DevKitC-1 board guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.0.html)
and [ESP-IDF build system](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html).

## Current behavior

The entry point calls `app::init()` and `app::run()`. Startup telemetry travels
through the application telemetry module, serial driver, and serial HAL. The
main loop waits for the shared sample interval through the scheduler and timer
HAL. All other modules are explicit placeholders with no hardware activity.

Bench tests remain outside the repository in `../benchtest/`.
