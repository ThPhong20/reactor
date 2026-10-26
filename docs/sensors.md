# pH and EC acquisition

This firmware targets the ESP32-S3 DevKitC-1 N16R8. It integrates the SEN0161-V2
pH and DFR0300 V2 EC (K=1) conversions from the local bench projects into the
application, device-driver, and ADC HAL layers.

## Wiring and configuration

| Sensor | Supply | Signal connection | ADC input |
| --- | --- | --- | --- |
| SEN0161-V2 pH | 3V3 and common GND | Board A directly to GPIO1 | ADC1 channel 0 |
| DFR0300 V2 EC, K=1 | 5V and common GND | Board A through the divider below to GPIO2 | ADC1 channel 1 |

Use the GPIO numbers printed on the DevKitC-1. The older standalone tests used
XIAO D0/GPIO1 for each sensor separately; the combined firmware uses two inputs.
GPIO4/5/6 remain assigned to pumps. The sensor manager rejects shared sensor pins
and conflicts with the configured pump pins before configuring ADC inputs.

EC divider, matching the bench test:

```text
EC A ---- 10 kOhm ----+---- GPIO2
                     |
                  20 kOhm
                     |
                    GND
```

The bench setup also uses 100 nF from the ADC pin to ground. The voltage at the
EC board is `pin_mV * (10 + 20) / 20`, so the configured gain is 1.5. The pH
signal connects directly and does not use this gain. The respective board output
ranges are 0..3.0 V for [SEN0161-V2](https://wiki.dfrobot.com/sen0161-v2) and
0..3.4 V for [DFR0300 V2](https://wiki.dfrobot.com/dfr0300).

`default_sensor_config()` in `src/common/system_config.cpp` contains the pin
mapping, EC divider gain, manual temperature, and calibration settings. Both
sensor calibration flags start false; default coefficients are estimates.

## Runtime workflow

1. `app::init()` leaves pump outputs off, then initializes both sensors.
2. The ADC HAL creates one shared ADC1 unit and a calibrated channel for each
   input, using 12-bit conversion and 12 dB attenuation.
3. Each loop samples pH and EC. The HAL averages 32 ADC samples and converts the
   mean to millivolts using ESP-IDF curve fitting.
4. Each driver converts its board voltage to a sensor value and supplies a status.
5. The application stores a timestamped snapshot in its 60-record RAM history,
   updates the TFT, and prints serial telemetry. Turbidity has a reserved field
   and displays unavailable until its acquisition is implemented.
6. The scheduler waits 1000 ms. The actual interval is this delay plus the time
   taken to sample, update the display, and log. See [storage and TFT](display.md).

Example output using default coefficients:

```text
pH=7.000 pH | pin=1500.0 mV board=1500.0 mV | estimate; needs calibration
EC25 (manual T=25.00 C)=1.413 mS/cm | pin=154.5 mV board=231.7 mV | estimate; needs calibration
```

If one input fails, the other is still sampled. Failures return a fresh status
and a NaN measurement, and telemetry prints `unavailable`. It does not repeat a
previous value as if it were fresh. Statuses distinguish initialization errors,
ADC errors, unusable voltage, out-of-range values, estimates, and calibrated
readings. `sensor_data::has_value()` permits both estimates and calibrated values;
future control decisions should explicitly require the appropriate status.

An individual raw sample at/below 0 or at/above 4090 marks the entire average as
clipped. Voltage checks and conversion range checks then apply. These checks do
not prove that a probe is connected or healthy; an in-range floating voltage may
still pass. Sampling currently reports measurements only and does not drive pumps
or implement reactor safety interlocks.

## Conversion and calibration

The pH driver interpolates between measured voltages in pH 7.00 and 4.00 buffers:

```text
pH = 7 + 3 * (board_mV - neutral_mV) / (neutral_mV - acid_mV)
```

Default references are 1500 and 2032.44 mV, following
[DFRobot_PH](https://github.com/DFRobot/DFRobot_PH). Record stable voltages with
both buffers, then put the measured pair and `calibrated = true` in the shared
configuration. The accepted reference windows are 1322..1678 mV for pH 7 and
1854..2210 mV for pH 4, with endpoints excluded. Invalid updates are rejected.
No pH temperature compensation is applied; use similar calibration and measurement
temperatures. Values outside pH 0..14 are unavailable.

The EC driver reports conductivity compensated to 25 C in mS/cm:

```text
raw_EC = board_mV / 164
EC25 = raw_EC * K / (1 + 0.0185 * (temperature_C - 25))
```

This follows [DFRobot_EC](https://github.com/DFRobot/DFRobot_EC). Separate low/high
K factors use hysteresis: switch high above 2.5 and low below 2.0 mS/cm, evaluated
using the previous factor before temperature compensation. Values above the
supported 20 mS/cm limit are unavailable.

Use the [manufacturer's two EC standards](https://wiki.dfrobot.com/dfr0300/docs/20349):
1.413 mS/cm and 12.88 mS/cm, specified at 25 C. For each stable measurement:

```text
K = standard_mS_cm * (1 + 0.0185 * (actual_temperature_C - 25)) / (board_mV / 164)
```

Record the low and high factors in the configuration and mark them calibrated
only after measuring both standards. Each factor must be finite and strictly
between 0.5 and 1.5. The EC temperature input is manual, defaults to 25 C, and
must be within 0..40 C. It is not a measured temperature or an automatic probe.

The application also exposes `set_ph_calibration()`, `set_ec_calibration()`, and
`set_ec_temperature()` in `sensor_manager.h` for runtime configuration. These are
C++ APIs; runtime changes are lost on reboot. NVS persistence and the bench
projects' serial calibration commands (`enterph`, `calph`, `enterec`, etc.) have
not been integrated. Defaults edited in shared configuration persist in firmware.

## Files and verification

| Layer | Files |
| --- | --- |
| Application | `src/application/sensor_manager/sensor_manager.{h,cpp}` |
| pH driver | `src/driver/ph/ph.{h,cpp}` |
| EC driver | `src/driver/ec/ec.{h,cpp}` |
| ADC HAL | `src/hal/adc/adc.{h,cpp}` |
| Data/status types | `src/common/sensor_data.h` |
| Configuration | `src/common/system_config.{h,cpp}` |
| Output | `src/application/telemetry/telemetry.{h,cpp}` |

Run host tests using a fake ADC and real sensor drivers, manager, and telemetry:

```bash
cmake -S src/test -B build/host-tests
cmake --build build/host-tests
ctest --test-dir build/host-tests --output-on-failure
```

Use the N16R8 build command in the project README to compile the actual ADC HAL.
Physical voltages and solution measurements require bench verification on the
connected DevKitC-1; host tests do not establish sensor accuracy.
