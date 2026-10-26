# Code review — 2026-10-26

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
- pH/EC calibration and outlet pump duty measurements remain necessary.
- Calibration settings and sample history are RAM-only.
- The scheduler waits after work, so updates are approximately once per second.
- Display transfer/error handling is tested through a fake HAL. Real SPI/DMA,
  screen layout, sensor wiring, and flow accuracy require board testing.
- The stale-data label only updates when code redraws the display; it is not an
  independent watchdog. A failed display can retain its previous image.
- The state machine, emergency STOP/interlocks, and electrolysis control remain
  unimplemented; the main loop does not start pumps.

See [development progress](progress.md) for the latest verification and next work.
