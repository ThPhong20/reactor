# Code review

## Current review — 2026-10-31

Scope: all project C++ sources and headers under `src/`, including tests and
unfinished turbidity modules; build definitions and the generated N16R8 settings.
Relevant ESP-IDF 6.0.1 startup, SPI/LCD, and watchdog behavior was also checked.
The layer boundaries are clear. The firmware is a monitoring prototype with a
tested FSM, not yet a complete unattended reactor controller.

### Confirmed defects corrected

| Priority | Location | Problem and correction |
| --- | --- | --- |
| High | `src/common/sensor_data.h` | The current header lacked `effective_status()`, breaking compilation in the FSM, display, and telemetry. Restored the helper and numeric checks; NaN, infinity, and negative values cannot be accepted merely because status says OK. Preserved the descriptive comments. |
| Medium | `src/driver/ec/ec.cpp` | Accepted factors `low_k=1.4`, `high_k=0.6` made a constant raw EC of 2 alternate between 1.2 and 2.8. Enter the high range only when its corrected result will not immediately trigger a switch back. The regression test failed before the fix and passes now. See the conversion details in [sensors](sensors.md). |
| Medium | `src/application/app.cpp` | Timestamping after both reads hid the age of an earlier reading if a later read stalled. Timestamp the acquisition start. A simulated 4-second EC delay now produces a stale-data warning and recovers on the next normal sample. This test also failed before the fix. |

The FSM and TFT now share `sensor_data::is_fresh()` and one 3-second age limit,
reducing the risk of different validity rules. Removed unused SDK headers and
clarified application-task ownership. Existing shutdown behavior and startup
restrictions are retained.

### Open findings, in priority order

1. **High, before powered reactor operation: control can stall behind I/O.**
   `app::run()` samples sensors, evaluates the FSM, draws the TFT, logs, then
   delays 1000 ms in one task. It cannot detect faults while blocked elsewhere.
   The 250 ms LCD semaphore timeout bounds only the completion wait after
   `esp_lcd_panel_draw_bitmap()` returns. The installed SDK's
   `components/esp_lcd/spi/esp_lcd_panel_io_spi.c` contains SPI waits using
   `portMAX_DELAY`; the complete display call has no proven 250 ms bound.
   Current production START remains blocked, which limits present exposure.
2. **High, before unattended operation: application watchdog management is absent.**
   `hal/timers_watchdog` only provides time and delay. The generated configuration
   monitors idle tasks with a 5-second task watchdog and has panic disabled.
   There is no application task subscription/feed. A blocked application can
   leave idle tasks running, so idle monitoring does not establish application
   liveness. See [ESP-IDF watchdog semantics](https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32s3/api-reference/system/wdts.html).
3. **High integration gap: required inputs and real output confirmation are missing.**
   Turbidity remains a scaffold; flow verification/interlocks remain false and
   the PSU callback is absent. Keep START blocked. Manual pump APIs bypass the
   FSM and are only suitable for controlled calibration until an operator mode
   owns them. SDK success is not proof of physical pump stoppage or PSU state.
4. **Medium, before adding tasks: shared state is not thread-safe.**
   FSM, history, pump/sensor managers, calibration, and renderer buffers assume
   one owner task. A future UI must not directly call these APIs concurrently.
   Use copied messages and a single control owner; do not lock the whole system
   around blocking LCD calls.
5. **Medium validation gaps:** real stack headroom, response times, reset behavior,
   ADC noise, flow accuracy, and calibration remain unmeasured. The current main
   task stack is 3584 bytes. Large display buffers are static/DMA allocated,
   but this does not prove sufficient stack margin. Capture the stack high-water
   mark and worst-case timings on the board. EC temperature is manual; EC cannot
   yet produce theoretical pH. Calibration and history are not persistent.

### FreeRTOS and FSM assessment

FreeRTOS is already running: ESP-IDF calls `app_main()` from its main task,
`vTaskDelay()` yields that task, and the LCD completion ISR correctly uses
`xSemaphoreGiveFromISR()`. There are no separate application control, sensor,
or display tasks yet. Delaying after work makes the sample period longer than
1000 ms. Host simulation does not execute the RTOS kernel.

The FSM's guarded startup, explicit flow confirmation, 60-second priming timeout,
power-off-before-pump-stop order, fault latch, and explicit reset are covered by
tests. STOP remains available after a fault. RESET intentionally requires full
readiness, so incomplete hardware cannot reset through this API. STOPPING is a
synchronous transition, not a timed shutdown state; future PSU callbacks must
either confirm completion within a bounded time or use explicit completion events.

For the next RTOS implementation, retain the existing app/driver/HAL boundaries:

- A control task exclusively owns the FSM and actuator commands. Use periodic
  scheduling and bounded event processing, independent of display and sampling.
  Define the required response deadline from the hardware before choosing its period.
- A sensor task owns ADC access and calibration, and publishes copied, timestamped
  snapshots. Queue calibration requests to this owner.
- A lower-priority output task owns TFT/logging and consumes copied status.
  A slow screen must not block control; use bounded queues and explicit overflow rules.
- Supervise actual task progress with a watchdog. Validate hardware output behavior
  during reset and keep physical emergency shutdown independent of software scheduling.

This is a proposed task split, not implemented by this review. The reviewed
[ESP-IDF FreeRTOS documentation](https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32s3/api-reference/system/freertos_idf.html)
describes the task/ISR APIs; the ownership and deadline recommendations above
follow from this project's shared state and blocking call paths.

### Verification

- 50 host tests passed: 20 FSM, 11 pump, 9 sensor, 7 storage/display, 3 integration simulations.
- All 50 passed with AddressSanitizer, LeakSanitizer, and UndefinedBehaviorSanitizer.
  Leak checks required running outside the ptrace-based sandbox.
- ESP-IDF 6.0.1 N16R8 build passed without compiler warnings/errors.
- Added FSM coverage for missing output callbacks, failed power-off before START,
  clock rollback during priming, and loss of pump readiness while running.
- Reviewed pH interpolation, EC compensation, PWM scaling, ADC clipping, history
  rollover, display bounds, and DMA-buffer lifetime. No additional confirmed
  defects were found in those paths; host tests do not validate hardware behavior.

## Earlier review — 2026-10-26

## Assessment

The application → driver → HAL structure is understandable and suitable for
this firmware. Pump logic, sensor conversion, storage, and display rendering
are separated and have host tests. The main maintenance risks were duplicated
validation, incomplete pin ownership checks, and inconsistent build entry points.
These have been corrected in this review.

The required monitoring system is still incomplete: EC acquisition works, but
EC-based theoretical pH and required turbidity acquisition are not implemented.

## Findings and changes

| Priority | Finding | Result |
| --- | --- | --- |
| High | A sensor configuration could assign GPIO9/10 to ADC while the TFT used those pins; display checks inspected default sensor pins rather than the supplied map | Fixed: shared pin checks in `common/system_config.cpp`; both managers receive the same sensor/display configurations from `app.cpp` |
| High | Required turbidity has no acquisition implementation; its status always remains unavailable | Open: retained as required work, with explicit startup reporting; no placeholder measurement is returned |
| High | Required EC-based theoretical pH has no calculation or output | Open: keep this as an application calculation, separate from the EC driver and measured pH |
| Medium | Serial output accepted an Ok/NeedsCalibration reading with NaN/Inf/negative value, while the TFT rejected it | Fixed: shared `effective_status()`/`has_value()` validation; both outputs suppress invalid numbers and report an error |
| Medium | `make build`/`make flash` used the root 2 MB configuration rather than the documented N16R8 build | Fixed: all Makefile targets use `build/esp32s3` and the N16R8 defaults |
| Low | TFT font size/position constants were repeated, with no check that changes fit the strip buffer | Fixed: named layout constants, compile-time bounds checks, and expanded drawing loops |
| Low | Inconsistent include paths, indentation, and source-list grouping made related code harder to scan | Cleaned up the touched modules without reorganizing working layers |

Priority describes the impact of the finding, not whether it was observed on
hardware. Pin conflicts require a conflicting configuration; the default pin
map remains unchanged. Invalid numerical readings are a defensive boundary
case: the existing pH/EC drivers already reject their normal conversion errors.

## Required sensor and calculation responsibilities

- `driver/ec/` reads the EC board, applies calibration and temperature
  compensation, and returns EC25 in mS/cm. EC is required, including as an
  input to theoretical-pH calculation.
- `driver/turbidity/` is required and will own sensor acquisition/conversion.
  `application/turbidity_manager/` will handle its monitoring/filtering work.
  The existing sensor manager should assemble the result into the same snapshot
  and sampling loop, rather than introducing another independent sampling task.
- A future application calculation module should produce theoretical pH as a
  separate derived result. It must not overwrite the pH electrode reading or
  place chemistry calculations in the EC hardware driver.
- Derived results need their own availability/validity, explicit input units
  and temperature basis, and distinct measured/theoretical labels in storage
  and output. EC calibration/error status must propagate to that calculation.

`sensor_manager::init()` currently reports initialization of the implemented
pH/EC inputs only. Its success is not a statement that all required monitoring
features are ready. Startup telemetry now states the missing required features.

## Maintenance rules for the next changes

1. Keep device conversion in drivers, board/SDK calls in the HAL, and reactor
   decisions or derived calculations in the application layer.
2. Keep all pin ownership checks in shared configuration. Add the turbidity
   assignment there before connecting its driver; retain one configuration for
   initialization of all managers.
3. Reuse common reading validity helpers. Do not introduce a different rule for
   each output, and keep sensor-specific range checks in the sensor driver.
4. Continue using bounded storage and explicit unavailable/error values.
5. Keep current calls on the application task. Add synchronization before
   introducing concurrent sampling, calibration, or display tasks.
6. Add behavior tests when implementing the missing sensors or calculations,
   especially calibration, invalid inputs, and propagation of unavailable data.

## Remaining limits

- Required turbidity acquisition and theoretical-pH calculation are pending.
- pH/EC calibration and combined pump-flow verification remain necessary;
  individual operating duties are now measured.
- Calibration settings and sample history are RAM-only.
- The scheduler waits after work, so updates are approximately once per second.
- Display transfer/error handling is tested through a fake HAL. Real SPI/DMA,
  screen layout, sensor wiring, and flow accuracy require board testing.
- The stale-data label only updates when code redraws the display; it is not an
  independent watchdog. A failed display can retain its previous image.
- Update on 2026-10-27: the software FSM is implemented and simulated. Physical
  emergency STOP/interlocks and electrolysis control remain unimplemented;
  production START remains blocked. See [the FSM](state_machine.md).

See [development progress](progress.md) for the latest verification and next work.

## Follow-up review — 2026-10-30

A missing common sensor-validation helper and misleading READY status after STOP
were corrected. Display layout assertions were restored. The 44-test suite passes
normally and with memory/undefined-behavior checks; the N16R8 firmware builds.
See [hardware readiness](hardware_readiness.md) for findings and outstanding
electrical/interface checks. Full reactor operation remains unavailable.
