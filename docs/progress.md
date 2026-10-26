# Development progress

Updated: 2026-10-26

## Current stage

The Gen2 monitoring software is partially implemented for the ESP32-S3 DevKitC-1 N16R8.
It reads pH and EC, stores recent samples, and sends readings to the TFT and
serial monitor. Pump control APIs are implemented, but the main loop keeps
pumps off. Automatic reactor operation is not implemented yet.

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
| Three pumps | Independent PWM, adjustable duties, coordinated start/stop, startup at zero duty; 10 kHz / 10-bit | Measure both outlet duties; verify flow under actual plumbing conditions; add operator controls |
| pH | SEN0161-V2 acquisition, voltage conversion, calibration API, error status | Measure buffer references and verify on hardware; save calibration persistently |
| EC (required) | DFR0300 V2 K=1 acquisition, calibration API, normalization to 25 C using manual temperature | Calibrate with standards and verify divider/readings; save calibration persistently; supply theoretical-pH calculation |
| Turbidity (required) | Driver/application folders and data/display fields | Confirm model, interface, pin, and calibration; implement acquisition |
| Theoretical pH | Not implemented | Add a separate EC-based application calculation and a distinct derived value in storage/output |
| Data storage | Latest 60 snapshots in RAM, with uptime, sequence number, and sensor status | Persistent data logging/export if required; RAM clears at restart |
| TFT | GMT130/ST7789 240x240 renderer, values, units, calibration/error labels, startup screen | Verify wiring, alignment, colors, and refresh on DevKitC-1 |
| Serial output | Sensor readings/status and startup/display messages | Operator input and serial calibration commands |
| Reactor sequence | INIT/IDLE/PRIME/RUNNING/FAULT enum | Events, transitions, priming, operator confirmation, and RUNNING behavior |
| Safety | Pump-driver error handling and zero-duty startup | Reactor interlocks, emergency STOP behavior, fault recovery, watchdog management |
| Electrolysis | No power-control implementation | Define power interface and actual current/voltage feedback before implementing regulation |
| Manuscript calculations | Method reviewed previously | Implement monitoring metrics after required measurements and equation/unit questions are resolved |

EC is a required monitoring and calculation input. The current code reports
EC25; it does not yet calculate theoretical pH. Turbidity and derived results
remain unavailable until implemented.

## Confirmed configuration

| Connection | GPIO | Setting / status |
| --- | --- | --- |
| Inlet pump | 4 | Target 70 mL/min; user measured 70 mL/min at 90% duty |
| Outlet 1 pump | 5 | Target 49 mL/min; saved duty 0% until calibrated |
| Outlet 2 pump | 6 | Target 21 mL/min; saved duty 0% until calibrated |
| pH analog input | 1 | Direct sensor-board input |
| EC analog input | 2 | 10k/20k divider; restore board voltage with gain 1.5 |
| TFT SCLK / MOSI | 12 / 11 | SPI2, 20 MHz, mode 3 |
| TFT DC / RESET | 10 / 9 | Seven-pin module, no exposed CS |
| Turbidity input | Unassigned | Awaiting sensor details |

The outlet numbers are target flows, not measured flows. The inlet measurement
is one calibration point, not a general linear PWM-to-flow formula. All three
pumps remain off at boot, including the inlet with its saved 90% duty.

## Current runtime workflow

1. Initialize pump outputs at zero duty.
2. Initialize pH/EC acquisition, clear RAM history, and initialize the TFT.
3. Sample pH and EC; retain turbidity as unavailable.
4. Save the snapshot and time since boot in the 60-record RAM history.
5. Show the latest stored snapshot on the TFT and print it over serial.
6. Wait 1000 ms, then repeat. Sampling/display/logging add to that interval.

Readings carry independent error/calibration status. Failed readings do not
reuse an older good value. Default pH/EC coefficients are marked as estimates;
manual EC temperature starts at 25 C. A display failure disables further
display updates until reboot while acquisition and storage continue.

## Verification completed

Latest verification after the consistency/maintenance review:

- 26 host tests passed: 11 pump, 8 sensor, 7 storage/display scenarios.
- ESP-IDF firmware build passed for ESP32-S3, 16 MB flash, Octal PSRAM enabled.
- Final firmware build had no reported compiler warnings or errors.
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
- [ ] Calibrate pH and EC; measure the outlet duties needed for 49 and 21 mL/min.
- [ ] Confirm turbidity hardware details, then implement and test its acquisition.
- [ ] Implement EC-based theoretical pH separately from electrode-measured pH.
- [ ] Add persistent calibration and operator controls.
- [ ] Define and implement safety handling and the reactor state sequence.
- [ ] Add electrolysis current/voltage measurement and power control.
- [ ] Implement the agreed manuscript monitoring calculations using the needed measurements.
- [ ] Add persistent logs/export if required for experiments.

See [architecture](architecture.md), [pumps](pumps.md), [sensors](sensors.md),
and [storage/TFT](display.md) for implementation and wiring details.
