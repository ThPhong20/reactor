> Update 2026-11-13: the separate TS-300B 10k/10k divider is confirmed installed.
> The hardware-test image now enables GPIO11 voltage acquisition with gain 2.
> Normal monitoring retains its disabled default. The installation-pending notes
> below describe the original bring-up configuration; NTU calibration is still pending.

# Integrated hardware test — ESP32-S3 N16R8

Updated 2026-11-03. This replaces the pump-only test. The test image runs pH,
EC, TFT, a 60-record RAM history, and supervised timed pump commands together.
TS-300B raw-voltage support is included but disabled: the user confirmed no
voltage divider is installed. No hardware has been flashed or verified here.
Electrode power must remain physically disconnected; PSU control is not implemented.

## Wiring reference

Use GPIO numbers printed on the DevKitC-1, not XIAO D-pin names.

| Device signal | GPIO | Notes |
| --- | --- | --- |
| Inlet HW-517 TRIG/PWM | 4 | Grothen G328, 24 V, 3 x 5 mm tubing |
| Outlet 1 HW-517 TRIG/PWM | 5 | NKP-DA-S10B, 24 V |
| Outlet 2 HW-517 TRIG/PWM | 6 | NKP-DA-S10B, 24 V |
| pH analog | 8 | SEN0161-V2; verify output before connection |
| EC analog | 10 | Output → 10 kΩ → GPIO10 → 20 kΩ → GND |
| Turbidity analog, reserved | 11 | TS-300B: AO must stay disconnected until a divider is installed |
| TFT SCL / SDA | 7 / 9 | SPI clock / MOSI |
| TFT RES / DC | 1 / 2 | ST7789 240 x 240, no exposed CS |
| Native USB | 19 / 20 | Reserved for the board's native USB connection |

For each HW-517: 24 V supply to DC+, supply ground to DC−, motor to OUT+/OUT−,
ESP32 PWM to TRIG/PWM, and shared signal ground. OUT− is switched: do not connect
it directly to ground. Never connect 24 V to an ESP32 power or signal pin.
Review [driver protection and wiring](pumps.md#hw-517-driver-wiring), including
flyback protection and keeping the driver input off during reset.


## TS-300B wiring prerequisite

[The supplier specification](https://www.elecrow.com/ts-300b-turbidity-sensor-water-turbidity-module-turbidity-detection-module.html)
lists a 5 V supply and 0–4.5 V analog output. Do not connect AO directly to GPIO11.
Disabling acquisition in software does not protect a physically connected pin.
Proposed wiring: AO → 10 kΩ → GPIO11 → 10 kΩ → GND, with common ground.
This gives a divider gain of 2 and at most 2.25 V at the ADC for a 4.5 V output.
Measure the actual output and divided voltage before connecting GPIO11.

After installing and verifying the divider, set `SensorConfig::turbidity_enabled`
to `true` in `src/common/system_config.h`, then rebuild and flash. Its default
`false` intentionally leaves this ADC input untouched. Adjust
`turbidity_divider_gain` if using a different verified divider.
The TFT then shows reconstructed board voltage and `VOLTAGE ONLY`; no NTU
conversion is invented. Raw voltage does not satisfy the production FSM's
calibrated turbidity requirement. An enabled turbidity channel must return valid
voltage before hardware-test pump starts; losing it stops and latches the test.

## Build and flash

Activate ESP-IDF in the same terminal. On this machine:

```bash
source /home/jaytp/.espressif/tools/activate_idf_v6.0.1.sh
make check
make hardware-build
make hardware-flash PORT=/dev/ttyACM0
make hardware-monitor PORT=/dev/ttyACM0
```

The Makefile uses exported `IDF_PATH` and `IDF_PYTHON_ENV_PATH`, including when
EIM exposes `idf.py` only as a shell function. An `idf.py: not found` error occurs
before USB access: activate the SDK first. Other machines need their own SDK path.
Close other serial programs first. If needed, hold BOOT, press/release RESET,
release BOOT, and retry flashing. Confirm the port after USB re-enumeration.
The native-USB configuration is isolated in `build/hardware-test`, using
`sdkconfig.defaults.esp32s3` and `sdkconfig.defaults.hardware-test`.

## Physical test procedure

1. Disconnect motor and electrode power. Check wiring, polarity, grounding,
   driver protection, and reset-time pull-downs. Keep electrode power disconnected.
2. Boot using native USB. Expect `INTEGRATED HARDWARE TEST`. Send `status` if
   the banner was missed. Commands are not echoed; press Enter to send them.
3. Check pH/EC serial readings and the TFT update approximately once per second.
   Uncalibrated estimates are accepted for this water test. Turbidity shows
   `NOT READY` while disabled. RAM history stores the latest 60 snapshots and
   is lost on reboot; there is no history export command.
4. Confirm GPIO4/5/6 are low, then connect motor power. All pumps must stay off.
   Check reset and power-up behavior separately: firmware cannot control pins
   before startup. Use the motor-power disconnect for unexpected operation.
5. Use clean water and collection containers. Test each pump briefly:

```text
status
run inlet 90 10
run outlet1 65 10
run outlet2 42 10
```

Wait for each run to finish before issuing the next command. Watch direction,
leaks, and the other two pumps remaining stopped. Avoid dry running.

6. Run the combined system with `run all 10`. Saved duties are 90%, 65%, and 42%
   at 10 kHz / 10-bit. Confirm pH/EC readings and the screen continue updating
   while pumps run; check for electrical noise, resets, and lost ADC readings.
7. Send `stop` + Enter, or `!` without Enter, during a run. Verify all motors
   physically stop. Repeat with a short timed run to check automatic stop.
   Test USB disconnection only with the independent power disconnect within reach.
8. After short tests succeed, use `run all 60` and collect each stream separately.
   Expected volumes at the previously measured operating points are 70, 49, and
   21 mL in 60 seconds. These are targets, not new measurements or acceptance
   tolerances. Record actual volumes under the installed tubing/head conditions.
9. Once the turbidity divider is installed and verified, enable the channel and
   repeat simultaneous tests. Compare clear and cloudy samples by voltage only.
   Do not claim NTU accuracy until a valid calibration model is established.

## Command behavior and task ownership

- `run inlet|outlet1|outlet2 DUTY SECONDS`: duty greater than 0 and at most 100%;
  whole duration 1–60 seconds.
- `run all SECONDS`: use saved duties on all three pumps.
- `stop` or `!`: command all pumps off; `!` needs no Enter.
- `status`, `help`: inspect logical status or command syntax.

New runs are rejected while a run is active. No command extends the deadline.
The priority-3 control task owns all pump commands, polls every 10 ms, and is
subscribed to a 2-second panic-enabled watchdog. The priority-1 monitoring task
owns sensors, TFT, and history. One-element queues copy records and pump state
between tasks; monitoring never writes pump outputs.

Starts require fresh usable pH/EC (and raw turbidity voltage if enabled).
An invalid record or a record older than 3 seconds stops pumps and latches a
fault until reboot. A stuck sensor/display operation therefore cannot keep
refreshing monitoring readiness. A returned display error disables rendering
while sampling continues; it does not independently stop an otherwise healthy
water test. The screen must still pass visual inspection for full integration.

USB bus disconnection stops a run, but closing the terminal may leave the bus
connected. Serial writes are non-waiting and may drop output under load.
Commands and timestamps are checked in bounded batches; this is not a certified
stop deadline. A watchdog reset cannot guarantee motor shutdown without correct
electrical off-state behavior. Always retain the physical disconnect.

The test uses its own timed controller, not the production reactor FSM. It does
not verify electrode control, physical interlocks, measured flow feedback, or
chemical performance. Normal firmware START guards remain unchanged.

## Restore normal monitoring

Stop testing and disconnect motor power before switching images:

```bash
make build
make flash PORT=/dev/ttyACM0
make monitor PORT=/dev/ttyACM0
```

The normal image uses `build/esp32s3`, has no hardware-test command parser, and
still rejects reactor START until all production prerequisites are implemented.
Its console configuration can differ from the native-USB test configuration.

## Physical acceptance record

| Check | Result |
| --- | --- |
| Off at boot/reset; physical disconnect | Not tested |
| pH and EC live values and reference calibration | Not tested |
| TFT refresh during simultaneous pump operation | Not tested |
| Individual and combined timed pump runs | Not tested |
| STOP, timeout, USB loss, sensor-staleness shutdown | Not tested |
| Collected inlet/outlet volumes | Not measured |
| TS-300B divider installation and measured ADC voltage | Pending; no divider installed |
| TS-300B voltage response and NTU calibration | Not tested / calibration model missing |

Host tests exercise fake peripherals and controller logic. They do not emulate
FreeRTOS scheduling or replace the physical checks above.

Software verification on 2026-11-03: all 71 host tests passed normally and with
address, leak, and undefined-behavior sanitizers. Both normal and hardware-test
ESP32-S3 N16R8 firmware builds succeeded without compiler warnings/errors.
Linked-symbol checks confirmed test control is absent from the normal image.
No board was flashed and no physical acceptance result is claimed.

## Explicit pump-only command — 2026-11-13

`pump-test 5` runs all pumps at saved duties for five seconds even when sensor
readings are unavailable. This command is for supervised water testing with
independent motor power disconnection available. Duration is 1–60 whole seconds.
Sensor failures/staleness do not stop this explicit test; timeout, STOP, USB loss,
watchdog, and latched pump hardware faults still apply. Ordinary `run` commands
continue to require valid fresh sensors. No electrode power is controlled.
