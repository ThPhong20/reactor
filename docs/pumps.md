# Three-pump PWM control

| Pump | GPIO / LEDC channel | Target flow | Saved operating duty |
| --- | --- | --- | --- |
| Inlet | 4 / 0 | 70 mL/min | 90% (measured) |
| Outlet 1 | 5 / 1 | 49 mL/min | 0% (awaiting calibration) |
| Outlet 2 | 6 / 2 | 21 mL/min | 0% (awaiting calibration) |

The outlet targets add up to the inlet target. The user measured the inlet at
70 mL/min with 90% duty using 10 kHz, 10-bit PWM. Outlet targets still need their
own measurements; they are not PWM percentages or guaranteed pump capacities.

`system_config::default_pump_config()` in `src/common/system_config.cpp` stores
the pin mapping, targets, and each pump's adjustable `duty_percent`. The inlet
defaults to 90% and the outlets to 0%. All physical outputs initialize at 0%,
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

## Startup and control

`app::init()` initializes the pump manager and leaves all outputs off. The
application loop does not start pumps yet; START/STOP handling and the reactor
state machine are still pending. A nonzero stored duty does not enable a pump
at boot.

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
