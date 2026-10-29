# Host system simulation

Run date: 2026-10-29. This is a software integration simulation of the current
single-loop firmware, not an ESP32 emulator or FreeRTOS scheduler simulation.
No hardware is flashed or operated.

## What runs

`src/test/system_simulation.cpp` links the actual `app.cpp`, scheduler,
pump/sensor managers and drivers, RAM store, TFT formatter/renderer, and serial
telemetry. Simulated ADC, PWM, LCD, clock/delay, and serial boundaries replace
hardware. Each scenario runs the real application loop for 90 cycles with a
virtual clock. A host-only exception ends the infinite loop at its delay boundary.

The inputs are synthetic voltages around the default pH and EC calibration
points. They are not predictions of the reactor chemistry. Turbidity remains
unavailable and theoretical pH is not calculated. Pumps remain at zero duty,
matching current application behavior. Pump actuation/failure scenarios are
covered separately by the existing pump host tests.

This rerun uses saved pump duties of 90% (70 mL/min inlet), 65% (49 mL/min
outlet 1), and 42% (21 mL/min outlet 2). Those flows are user measurements,
not simulation results. The application starts in `IDLE / VERIFY PUMP FLOWS`
and rejects START; saved duties do not turn outputs on at boot.

## Run again

```bash
cmake -S src/test -B build/host-tests
cmake --build build/host-tests
ctest --test-dir build/host-tests --output-on-failure

# Run only the two simulations and print their summaries:
ctest --test-dir build/host-tests -R '^simulation_' -V
```

Outputs are under `build/host-tests/simulation/normal/` and `faults/`:

- `summary.txt`: result and counters.
- `trace.csv`: report timestamps, sensor/display states, and pump duties.
- `serial.log`: actual application telemetry generated during the run.
- `tft-normal.svg`: actual TFT renderer pixels at cycle 5.
- `tft-ph-check.svg`: actual TFT renderer pixels at cycle 22, including the
  injected pH error in the faults scenario. These are framebuffer captures;
  they do not validate physical SPI transfer, panel colors, or orientation.

Generated outputs are ignored build artifacts and are regenerated on each run.

## Scenarios and results

| Scenario | Injected behavior | Result |
| --- | --- | --- |
| Normal | Changing pH/EC voltages over 90 cycles | Passed; 90 reads per sensor, 91 complete TFT frames including startup |
| Slow TFT | Cycles 10–19: 20 ms per strip instead of 2 ms | Passed; monitoring continues, report spacing grows |
| pH failure | Cycles 20–24: ADC read fails | Passed; pH unavailable, EC continues; pH recovers afterward |
| EC failure | Cycles 40–44: ADC read fails | Passed; EC unavailable, pH continues; EC recovers afterward |
| TFT timeout | Cycle 60: simulated 250 ms failure response | Passed; failure reported, further TFT writes stop, sensor sampling continues through cycle 89 |
| Pump startup | All cycles | Passed; all simulated outputs remain zero |

Normal run: 92,540 ms of virtual time, 90 pH reads, 90 EC reads, and 91 completed
TFT frames. Fault run: 93,990 ms of virtual time, 90 reads per sensor, five
injected failures per sensor, and 61 completed TFT frames before display shutdown.
Sensor readings recover after their injected failures; acquisition continues
after the TFT timeout. No new failures were found in this rerun.

All 42 host tests pass, including 14 FSM scenarios and these two integration
runs. The system simulations additionally verify IDLE startup and a blocked
START in the real production adapter. The existing storage test checks rollover and
preservation of failed samples. The simulation executes the application's
private history store but does not inspect it directly.

## Timing interpretation

Timing assumptions are 4 ms per averaged sensor read, 2 ms per normal TFT strip,
20 ms per slow strip, and 250 ms for the failed transfer. Serial output and CPU
calculation time are assigned zero virtual duration. These are controlled test
inputs, not measured ESP32 timings or worst-case bounds.

| Condition | Simulated interval between serial reports |
| --- | --- |
| Normal | 1,028 ms |
| Slow TFT | 1,208 ms |
| TFT timeout cycle | 1,258 ms |
| After TFT is disabled | 1,008 ms |

This demonstrates the coupling in the current sequential loop. It supports
separating sensor acquisition from display work with queues and periodic task
scheduling. It does not establish the performance of a future two-task design.
RTOS scheduling, races, stack use, watchdog response, ADC calibration, electrical
signals, real pump flow, and chemical recovery still require other validation.

The simulation target itself does not alter production behavior. The FSM is
now integrated into the production application; see [its documentation](state_machine.md).
