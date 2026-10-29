# Development progress

Updated: 2026-10-29

## Current stage

The Gen2 monitoring software is partially implemented for the ESP32-S3 DevKitC-1 N16R8.
It reads pH and EC, stores recent samples, and sends readings to the TFT and
serial monitor. Pump control APIs are implemented, but the main loop keeps
pumps off. The FSM sequence is implemented and tested in simulation; real
operation is blocked by missing readiness and hardware-control integration.

Overall completion is estimated at about 65%. This is an engineering estimate,
not a measured acceptance score. Repeating the simulation confirms existing
behavior; it does not complete the remaining hardware or sensor work.

EC and turbidity are required sensors. EC also supplies the planned theoretical-pH
calculation. Required turbidity acquisition and theoretical pH are still pending;
the existing pH reading is measured by the pH electrode.

Software build/test success does not mean the complete system has been tested
on hardware. The earlier TFT bench test used a Seeed XIAO ESP32-S3; the combined
DevKitC-1 firmware still needs a physical test.

## Module status

| Part | Implemented now | Remaining work |
| --- | --- | --- |
| Project structure | Application, driver, HAL, shared data/configuration, host tests; unused scaffolding cleaned up | Add future modules as needed |
| Three pumps | Independent PWM, adjustable duties, coordinated start/stop, startup at zero duty; 10 kHz / 10-bit | Verify all three flows together under actual plumbing conditions; add operator controls |
| pH | SEN0161-V2 acquisition, voltage conversion, calibration API, error status | Measure buffer references and verify on hardware; save calibration persistently |
| EC (required) | DFR0300 V2 K=1 acquisition, calibration API, normalization to 25 C using manual temperature | Calibrate with standards and verify divider/readings; save calibration persistently; supply theoretical-pH calculation |
| Turbidity (required) | Driver/application folders and data/display fields | Confirm model, interface, pin, and calibration; implement acquisition |
| Theoretical pH | Not implemented | Add a separate EC-based application calculation and a distinct derived value in storage/output |
| Data storage | Latest 60 snapshots in RAM, with uptime, sequence number, and sensor status | Persistent data logging/export if required; RAM clears at restart |
| TFT | GMT130/ST7789 240x240 renderer, values, units, calibration/error labels, startup screen | Verify wiring, alignment, colors, and refresh on DevKitC-1 |
| Serial output | Sensor readings/status and startup/display messages | Operator input and serial calibration commands |
| Reactor sequence | INIT/IDLE/PRIME/RUNNING/STOPPING/FAULT, readiness guards, priming timeout, latched faults/reset, TFT/serial state | Operator input, verified flow readiness, interlock and PSU backends; real START remains blocked |
| Safety | Zero-duty startup, software shutdown order, readiness-loss faults, explicit reset | Physical interlocks/emergency STOP, hardware validation, fast control task and watchdog management |
| Electrolysis | No power-control implementation | Define power interface and actual current/voltage feedback before implementing regulation |
| Manuscript calculations | Method reviewed previously | Implement monitoring metrics after required measurements and equation/unit questions are resolved |

EC is a required monitoring and calculation input. The current code reports
EC25; it does not yet calculate theoretical pH. Turbidity and derived results
remain unavailable until implemented.

## Confirmed configuration

| Connection | GPIO | Setting / status |
| --- | --- | --- |
| Inlet pump | 4 | Target 70 mL/min; user measured 70 mL/min at 90% duty |
| Outlet 1 pump | 5 | Measured 49 mL/min at 65% duty |
| Outlet 2 pump | 6 | Measured 21 mL/min at 42% duty |
| pH analog input | 1 | Direct sensor-board input |
| EC analog input | 2 | 10k/20k divider; restore board voltage with gain 1.5 |
| TFT SCLK / MOSI | 12 / 11 | SPI2, 20 MHz, mode 3 |
| TFT DC / RESET | 10 / 9 | Seven-pin module, no exposed CS |
| Turbidity input | Unassigned | Awaiting sensor details |

All three pumps have individually measured operating points.
These measurements do not establish a general linear PWM-to-flow formula. All three
pumps remain off at boot, including the inlet with its saved 90% duty.

## Current runtime workflow

1. Initialize pump outputs at zero duty.
2. Initialize pH/EC acquisition, clear RAM history, and initialize the TFT.
3. Sample pH and EC; retain turbidity as unavailable.
4. Save the snapshot and time since boot in the 60-record RAM history.
5. Evaluate the FSM, then show the latest stored snapshot, state, and reason on
   the TFT; report readings and state changes over serial.
6. Wait 1000 ms, then repeat. Sampling/display/logging add to that interval.

Readings carry independent error/calibration status. Failed readings do not
reuse an older good value. Default pH/EC coefficients are marked as estimates;
manual EC temperature starts at 25 C. A display failure disables further
display updates until reboot while acquisition and storage continue.

## Verification completed

Latest host verification: 2026-10-29.

- 42 host tests passed: 14 FSM, 11 pump, 8 sensor, 7 storage/display, 2 system simulations.
- Normal and fault-injected simulations each ran the real application loop for
  90 cycles with simulated peripherals; see [simulation results](simulation.md).
- Normal simulation: 90 reads per sensor, 91 TFT frames, maximum report interval 1,028 ms.
- Fault simulation: five injected failures per sensor recovered; TFT timeout
  disabled display updates while sampling continued for all 90 cycles.
  Maximum report interval was 1,258 ms. Timing is virtual, not measured hardware timing.
- Saved pump duties are 90%, 65%, and 42%. Both simulations confirmed zero
  startup output and rejected START with `IDLE / VERIFY PUMP FLOWS`.
- The preceding firmware review successfully built for ESP32-S3, 16 MB flash,
  Octal PSRAM enabled, with no compiler warnings or errors. This rerun changed
  documentation only; it did not rebuild or flash firmware.
- Complete DevKitC-1 + pumps + sensors + TFT hardware operation is not verified.

Host tests use fake peripheral interfaces. They validate software behavior,
not electrical wiring, flow accuracy, sensor calibration, or screen appearance.
`make build`, `make flash`, and `make monitor` now use the isolated
`build/esp32s3` N16R8 configuration. A plain `idf.py build` still uses the root
`sdkconfig`, which selects 2 MB flash with PSRAM disabled.

The [code review](code_review.md) records corrected pin ownership checks,
consistent reading validation, build configuration, and TFT layout checks.

## Next development steps

- [ ] Test the combined monitoring firmware on the DevKitC-1 and TFT.
- [ ] Calibrate pH and EC; verify all three flows together.
- [ ] Confirm turbidity hardware details, then implement and test its acquisition.
- [ ] Implement EC-based theoretical pH separately from electrode-measured pH.
- [ ] Add persistent calibration and operator controls.
- [x] Implement and simulate the guarded reactor state sequence.
- [ ] Connect physical interlocks/operator events and verify shutdown on hardware.
- [ ] Add electrolysis current/voltage measurement and power control.
- [ ] Implement the agreed manuscript monitoring calculations using the needed measurements.
- [ ] Add persistent logs/export if required for experiments.

See [architecture](architecture.md), [pumps](pumps.md), [sensors](sensors.md),
and [storage/TFT](display.md) for implementation and wiring details.
