# Three-pump PWM control

| Pump | Confirmed model | GPIO / LEDC channel | Target flow | Saved operating duty |
| --- | --- | --- | --- | --- |
| Inlet | Grothen G328, 24 V, 3 x 5 mm tube | 4 / 0 | 70 mL/min | 90% (measured) |
| Outlet 1 | Kamoer NKP-DA-S10B, 24 V | 5 / 1 | 49 mL/min | 65% (measured) |
| Outlet 2 | Kamoer NKP-DA-S10B, 24 V | 6 / 2 | 21 mL/min | 42% (measured) |

The outlet targets add up to the inlet target. The user measured the inlet at
70 mL/min with 90% duty using 10 kHz, 10-bit PWM. Outlet 1 was measured at 49 mL/min with 65% duty. Outlet 2 was measured at 21 mL/min with 42% duty; flow targets are not PWM percentages or guaranteed pump capacities.

`system_config::default_pump_config()` in `src/common/system_config.cpp` stores
the pin mapping, targets, and each pump's adjustable `duty_percent`. The inlet
defaults to 90%, outlet 1 to 65%, and outlet 2 to 42%. All physical outputs initialize at 0%,
regardless of saved duty. The PWM signal uses active-high pulses: 0% holds the pin
low and 100% holds it high. These GPIOs are control signals for pump drivers.

The frequency is 10 kHz with 10-bit duty resolution, matching the inlet calibration.
This supersedes the older 5 kHz, 8-bit setup in `../benchtest/pump_pwm`.
The frequency is set by
`system_config::pump_pwm_frequency_hz`; resolution is set in the PWM HAL. Verify
these settings against the actual pump driver. LEDC low-speed timer 0 and
channels 0, 1, and 2 are reserved for the pumps. ESP-IDF behavior is documented in
the [LEDC guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/ledc.html).

The HAL converts duty using `round(percent * 1024 / 100)`. At 90%, the raw duty
is 922, or approximately 90.04% after rounding. A 10 kHz cycle is 100 microseconds.
This calibration point does not establish a linear flow-to-duty curve for the
other pumps.

## Identified pump hardware

Models were confirmed by the user on 2026-10-30. The
[Grothen G328 data sheet](https://ae01.alicdn.com/kf/Sfb54a59c481d4a0f97dff55d549263371.pdf)
lists separate 12 V and 24 V variants with 3 x 5 mm tubing. The user confirmed
the installed inlet is 24 V. The sheet lists reference flow above 90 mL/min,
not a guaranteed maximum or a linear duty-to-flow curve.

The [Kamoer NKP manual](https://m.media-amazon.com/images/I/81o9jakHMZL.pdf)
defines DA as a 24 V DC brushed motor. This older manual does not list the S10
tube option, so the outlets' 75 mL/min capacity remains user-reported rather
than independently verified from that document. The measured operating flows
and stored duties above remain the calibration points used by this project.

The user confirmed an HW-517 dual-MOSFET PWM module as the motor driver.
All three motors use a 24 V supply; the ESP32 supplies only the control signal.
The earlier KPHM100 brushless-pump polarity concern does not describe these
identified pumps. No PWM polarity or frequency was changed based on that candidate.

## HW-517 driver wiring

The common D4184 dual-MOSFET module has **one independently controlled output**:
the two MOSFETs work in parallel. Three pumps at different duties therefore
require **three modules**, one per pump. This is the required arrangement;
the number of installed modules has not been confirmed.

A [supplier data sheet for this D4184 module design](https://www.mantech.co.za/Datasheets/Products/DC-SSR_200912.pdf)
specifies a 5–36 V load supply, control input starting at 3.3 V, and PWM up to
20 kHz. Thus 24 V motor power and the current 3.3 V / 10 kHz PWM fit the published
ranges for that design. HW-517 boards may use different components: these
specifications do not verify the actual board's current capacity or temperature.
Check its markings and switching behavior under the pump's startup load.

Wire by terminal labels, not by assumed terminal order:

| Connection | Each HW-517 module |
| --- | --- |
| Fused +24 V motor supply | DC+ |
| Motor supply negative / 0 V | DC- |
| Pump positive | OUT+ |
| Pump negative | OUT- |
| ESP32 GND | Signal GND, referenced to supply 0 V |
| GPIO4 / GPIO5 / GPIO6 | TRIG/PWM on inlet / outlet 1 / outlet 2 module respectively |

Keep motor return current in the power wiring rather than routing it through
ESP32 ground leads. OUT- is the switched motor return, not a ground connection
for the MCU; grounding OUT- bypasses the switch. Do not connect 24 V to an ESP32
power or GPIO pin. Power the board through its normal supported supply path.

Use a flyback diode across each motor unless an equivalent motor recirculation
path is already verified: cathode (band) to motor positive / OUT+, anode to
motor negative / OUT-. Select a fast or Schottky diode with reverse-voltage
margin above the 24 V rail and current capability for the motor current at
turn-off, including startup. A MOSFET body diode alone is not that path in a
single low-side switch. See [Diodes Incorporated's motor-drive explanation](https://www.diodes.com/design/support/technical-articles/driving-brushed-dc-motors).

Verify an effective TRIG-to-GND pull-down so reset/disconnected control leaves
pumps off; add an appropriate external pull-down if absent. Check LOW = off and
HIGH = on before wet tests. For boards using AOD4184A, the component data sheet
specifies on-resistance at 4.5 V and 10 V gate drive, not at 3.3 V. The module's
3.3 V trigger claim is therefore not a guarantee of its advertised high-current
performance; inspect voltage drop and heating with the actual pumps.
[AOD4184A data sheet](https://www.aosmd.com/res/data_sheets/AOD4184A.pdf).

The 24 V supply and branch protection must accommodate all three motor startup
currents. No supply-current rating is selected from flow rate alone. Firmware
settings remain 90%, 65%, and 42%, and all outputs still initialize off.

## Startup and control

`app::init()` initializes the pump manager and leaves all outputs off. The
application loop does not start pumps automatically. The FSM owns guarded
START/STOP events, but the production adapter blocks START until required
readiness and hardware backends exist. There is no serial/button command parser.
A nonzero stored duty does not enable a pump at boot. See [the FSM](state_machine.md).

Use the pump manager from the application task:

- `init()` configures all three channels at zero duty.
- `set_manual_duty(Pump::Inlet, percent)` immediately adjusts one pump for
  calibration. `Pump::Outlet1` and `Pump::Outlet2` select the other pumps.
- `set_operating_duty(pump, percent)` saves a duty in RAM for the next
  `start_all()` call; it does not change a running output.
- `start_all()` applies all three stored duties. Every duty must be above zero;
  the function validates the complete set before starting any pump.
- `stop_all()` attempts to stop every configured channel, even if one fails.

All operations return `bool`; check the result. Invalid pump IDs, non-finite
duties, and percentages outside 0..100 are rejected without changing outputs.
PWM duties are rounded to the nearest hardware step. Manual duty changes do not
overwrite stored operating duties. Runtime settings are lost on reboot; put
measured operating duties into `default_pump_config()` to retain them in firmware.

If an output write fails, the driver attempts to stop all pumps and blocks
further starts until reboot. Stop attempts remain available. A failed stop is
reported as failure, not as a confirmed physical shutdown. Calling `init()`
again stops outputs and reloads default duties; it does not clear a hardware
fault. Calls are serialized by the application task; these APIs are not for
interrupt handlers or concurrent tasks.

## Manual calibration

1. Run one pump at an explicitly chosen duty using `set_manual_duty()`.
2. Collect and measure liquid over a known time with the intended tubing and
   operating load. Flow in mL/min is `collected_mL * 60 / elapsed_seconds`.
3. Adjust the duty and repeat until that pump reaches its target.
4. Stop the pump with 0% duty or `stop_all()`. Record its measured operating duty.
5. Repeat for the other pumps, then check flows with all three operating together.

There is no automatic conversion from mL/min to duty and no flow feedback yet.
For example, 70 mL/min does not imply 70% duty. The manual APIs are C++ calls;
serial command parsing has not been added.

## Verification

Host tests exercise the real manager and driver against a fake PWM HAL, covering
startup, pin mapping, independent duties, invalid input, start preflight, partial
initialization, output errors, stop errors, and restart behavior:

```bash
cmake -S src/test -B build/host-tests
cmake --build build/host-tests --target pump_control_test
ctest --test-dir build/host-tests -R '^pump_' --output-on-failure
```

The normal ESP-IDF firmware build compiles the actual LEDC HAL. Host tests do
not verify electrical signals or physical flow rates; those require bench work.
