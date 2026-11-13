> Latest physical capture, 2026-11-13: all three inputs now report readings:
> pH about 5.98, EC about 19.5 mS/cm, TS-300B board voltage about 1.52 V.
> No voltage errors in the approximately 12-second capture. pH/EC calibration,
> turbidity NTU conversion, and physical pump-flow testing remain pending.

> Update 2026-11-13: EC and TS-300B dividers are confirmed installed. TS-300B
> voltage acquisition is enabled in the hardware-test image (GPIO11, gain 2).
> The preceding physical test resolved EC saturation; pH still read 0 V.
> See local test logs under build/local-docs for measured results.

> Update 2026-11-03: integrated hardware testing replaces the pump-only image.
> TS-300B raw voltage acquisition, history, and TFT formatting are implemented;
> its input remains disabled because no divider is installed. NTU is unavailable.
> Hardware-test control and monitoring use separate tasks. Normal firmware still
> uses its existing single-loop FSM adapter. Physical verification is pending.

# Development progress

Updated: 2026-11-03

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
| Three pumps | Independent PWM; zero-duty startup; 10 kHz / 10-bit; integrated native-USB timed commands | Verify all flows under plumbing load; production operator interface pending |
| pH | SEN0161-V2 acquisition, voltage conversion, calibration API, error status | Measure buffer references and verify on hardware; save calibration persistently |
| EC (required) | DFR0300 V2 K=1 acquisition, calibration API, normalization to 25 C using manual temperature | Calibrate with standards and verify divider/readings; save calibration persistently; supply theoretical-pH calculation |
| Turbidity (required) | TS-300B raw voltage driver, ADC2 GPIO11, storage and TFT support; disabled by default | Install/verify divider, enable acquisition, calibrate NTU |
| Theoretical pH | Not implemented | Add a separate EC-based application calculation and a distinct derived value in storage/output |
| Data storage | Latest 60 snapshots in RAM, with uptime, sequence number, and sensor status | Persistent data logging/export if required; RAM clears at restart |
| TFT | GMT130/ST7789 240x240 renderer, values, units, calibration/error labels, startup screen | Verify wiring, alignment, colors, and refresh on DevKitC-1 |
| Serial output | Monitoring telemetry; separate native-USB hardware-test commands | Production operator input and serial calibration commands |
| Reactor sequence | INIT/IDLE/PRIME/RUNNING/STOPPING/FAULT, readiness guards, priming timeout, latched faults/reset, TFT/serial state | Operator input, verified flow readiness, interlock and PSU backends; real START remains blocked |
| Safety | Zero-duty startup, software shutdown order, readiness-loss faults, explicit reset; hardware-test task watchdog | Physical interlocks/emergency STOP, hardware validation, fast control task and watchdog management |
| Electrolysis | No power-control implementation | Define power interface and actual current/voltage feedback before implementing regulation |
| Manuscript calculations | Method reviewed previously | Implement monitoring metrics after required measurements and equation/unit questions are resolved |

EC is a required monitoring and calculation input. The current code reports
EC25; it does not yet calculate theoretical pH. Turbidity NTU and derived results
remain unavailable; raw turbidity voltage can be enabled after divider verification.

## Confirmed configuration

| Connection | GPIO | Setting / status |
| --- | --- | --- |
| Inlet pump | 4 | Grothen G328, 24 V, 3 x 5 mm; measured 70 mL/min at 90% duty |
| Outlet 1 pump | 5 | NKP-DA-S10B, 24 V; measured 49 mL/min at 65% duty |
| Outlet 2 pump | 6 | NKP-DA-S10B, 24 V; measured 21 mL/min at 42% duty |
| pH analog input | 8 | Direct sensor-board input |
| EC analog input | 10 | 10k/20k divider; restore board voltage with gain 1.5 |
| TFT SCLK / MOSI | 7 / 9 | SPI2, 20 MHz, mode 3 |
| TFT DC / RESET | 2 / 1 | Seven-pin module, no exposed CS |
| Turbidity input | 11 | ADC2 channel 0 reserved; acquisition remains disabled |

The user reports maximum pump capacities of 90 mL/min for the inlet and
75 mL/min for each outlet. All motors are confirmed as 24 V, with HW-517 PWM
driver modules. Three separate modules and an accessible motor-power disconnect
were confirmed on 2026-11-03. Flyback protection, startup current, and reset
behavior still need bench checks.
See [driver wiring](pumps.md#hw-517-driver-wiring).
All three pumps have individually measured operating points.
These measurements do not establish a general linear PWM-to-flow formula. All three
pumps remain off at boot, including the inlet with its saved 90% duty.

The TFT wiring was updated on 2026-11-01; pH moved to GPIO8 and EC to GPIO10
to avoid conflicts. GPIO7 is now the TFT clock, so the earlier proposed turbidity
pin is unavailable. Turbidity is now reserved on GPIO11 (ADC2 channel 0).
The ADC HAL supports both units; TS-300B voltage sampling is implemented but disabled pending divider installation
and voltage verification. NTU calibration is still missing.
The updated pin map passes all 50 host tests and the N16R8 firmware build;
it has not been flashed or verified on the board.

## Hardware bring-up preparation

A separate `make hardware-build` image runs pH/EC/TFT/history alongside native-USB timed pump tests,
including `run inlet 90 10`, `run all 10`, `stop`, and immediate `!`. Runs are
limited to 1..60 seconds. It has a subscribed task watchdog, bounded input
processing, and non-waiting replies. The normal monitoring image still rejects
reactor START. Both modes boot with pumps off; neither controls electrode power.
See [hardware bring-up](hardware_bringup.md) for wiring, flashing, measurements,
and the unfilled physical acceptance record. No board has been flashed by this work.

## Current runtime workflow

1. Initialize pump outputs at zero duty.
2. Initialize pH/EC acquisition, clear RAM history, and initialize the TFT.
3. Sample pH and EC; sample TS-300B voltage only if explicitly enabled after divider verification.
4. Save the snapshot and time since boot in the 60-record RAM history.
5. Evaluate the FSM, then show the latest stored snapshot, state, and reason on
   the TFT; report readings and state changes over serial.
6. Wait 1000 ms, then repeat. Sampling/display/logging add to that interval.

Readings carry independent error/calibration status. Failed readings do not
reuse an older good value. Default pH/EC coefficients are marked as estimates;
manual EC temperature starts at 25 C. A display failure disables further
display updates until reboot while acquisition and storage continue.

## Verification completed

Latest host verification: 2026-11-03.

- 71 host tests passed: 20 FSM, 11 pump, 11 timed-pump, 6 hardware-session, 4 turbidity, 9 sensor, 7 storage/display, 3 system simulations.
- Normal and fault-injected simulations each ran the real application loop for
  90 cycles with simulated peripherals; see [simulation results](simulation.md).
- Normal simulation: 90 reads per sensor, 91 TFT frames, maximum report interval 1,028 ms.
- Fault simulation: five injected failures per sensor recovered; TFT timeout
  disabled display updates while sampling continued for all 90 cycles.
  Maximum report interval was 1,258 ms. Timing is virtual, not measured hardware timing.
- Saved pump duties are 90%, 65%, and 42%. All three simulations confirmed zero
  startup output and rejected START with `IDLE / VERIFY PUMP FLOWS`.
- All 71 tests also passed with address, leak, and undefined-behavior sanitizers.
- Both normal and integrated hardware-test firmware built for ESP32-S3, 16 MB flash,
  Octal PSRAM enabled, without compiler warnings/errors; no board was flashed.
- Checked linked symbols: the normal image does not contain the hardware-test controller.
- Corrected a missing shared sensor-validation helper and misleading READY status
  after STOP. Restored display layout checks. See [hardware readiness](hardware_readiness.md).
- Corrected EC range oscillation and acquisition timestamps; a 4-second slow-sensor
  simulation now reports stale data. FSM and display share one freshness rule.
- Reviewed FreeRTOS ownership, watchdog gaps, and unbounded SDK SPI waits; see
  [the current code review](code_review.md#current-review--2026-10-31).
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
- [ ] Install and verify the TS-300B divider, enable raw-voltage acquisition, then calibrate NTU.
- [ ] Implement EC-based theoretical pH separately from electrode-measured pH.
- [ ] Add persistent calibration and operator controls.
- [x] Implement and simulate the guarded reactor state sequence.
- [ ] Connect physical interlocks/operator events and verify shutdown on hardware.
- [ ] Add electrolysis current/voltage measurement and power control.
- [ ] Implement the agreed manuscript monitoring calculations using the needed measurements.
- [ ] Add persistent logs/export if required for experiments.

See [architecture](architecture.md), [pumps](pumps.md), [sensors](sensors.md),
and [storage/TFT](display.md) for implementation and wiring details.

### Pump-only physical test — 2026-11-13

Added `pump-test SECONDS` to the integrated test console for supervised pump tests
while sensors are unavailable. The 1–60 second limit, STOP, USB-disconnection
shutdown, watchdog, and pump fault checks remain. Normal `run` commands retain
sensor readiness checks. All 72 host tests passed and the firmware built/flashed.
A five-second run at 90%/65%/42% was acknowledged and ended automatically; follow-up
STOP confirmed outputs commanded off. Physical motor motion and flow still need
operator confirmation. pH remains invalid; EC and turbidity telemetry continued.
