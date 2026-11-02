# Hardware readiness review — 2026-10-30

Wiring references updated on 2026-11-01 for TFT GPIO7/9/2/1, pH/EC GPIO8/10,
and reserved turbidity GPIO11. Turbidity acquisition remains disabled.

The corrected firmware is ready for a supervised monitoring-board bench test
with pump power and electrode power disconnected. Complete reactor operation
remains blocked. No board was flashed, no physical signals were measured, and
this review cannot establish that every possible fault has been eliminated.

## Bugs corrected

| Finding | Correction and verification |
| --- | --- |
| Shared `effective_status()` was missing, breaking compilation in the FSM, display, and telemetry. `has_value()` also accepted NaN/Inf/negative values marked OK. | Restored common numeric validation while preserving the explanatory comments. Existing output tests pass; expanded FSM tests reject malformed readings at START and shut down when one arrives during RUNNING. |
| STOP changed the reason to READY even when start conditions were unsatisfied. | Reproduced with a failing regression test. STOP now retains the current start blocker after successful shutdown; fault latching is unchanged. |
| Display row dimensions had lost their compile-time consistency check. | Added checks for complete panel coverage and sufficient rows for the sensor/FSM layout; the explicit ten-row setting is preserved. |

## Checks completed

- 44 host tests passed: 16 FSM, 11 pump, 8 sensor, 7 storage/display, and 2 simulations.
- All 44 also passed with GCC AddressSanitizer, LeakSanitizer, and UndefinedBehaviorSanitizer.
  Leak checks initially could not run under sandbox process tracing; the complete
  instrumented suite passed outside that sandbox. No sanitizer findings were reported.
- Normal and fault simulations ran 90 cycles each. Sensor errors recovered;
  sampling continued after TFT failure; physical-output substitutes stayed off.
- ESP-IDF 6.0.1 build passed for ESP32-S3, 16 MB flash, Octal PSRAM enabled,
  with no compiler warnings/errors. Application image: `0x40380` bytes,
  with about 75% of the application partition free.
- Reviewed pin ownership, LEDC startup/stop handling, ADC calibration and clipping,
  LCD DMA-buffer lifetime, ring-buffer rollover, and FSM output order.
- Host tests substitute hardware interfaces. They do not execute the real ADC,
  LEDC, SPI/DMA, boot sequence, or FreeRTOS scheduling on an ESP32.

## Hardware information and unresolved identification

| Device | User-reported information | Firmware setting |
| --- | --- | --- |
| Inlet pump | Grothen G328, 24 V, 3 x 5 mm; reported maximum 90 mL/min; measured 70 mL/min at 90% duty | GPIO4, 90% saved duty |
| Outlet 1 | NKP-DA-S10B, 24 V; reported maximum 75 mL/min; measured 49 mL/min at 65% duty | GPIO5, 65% saved duty |
| Outlet 2 | NKP-DA-S10B, 24 V; reported maximum 75 mL/min; measured 21 mL/min at 42% duty | GPIO6, 42% saved duty |
| Turbidity | OEM probe with analog board; exact model unknown | No assigned GPIO or acquisition yet |

Pump models are now confirmed by the user. Pump PWM remains 10 kHz, 10-bit,
active high. The user's affirmative reply was interpreted as LOW = stopped;
verify that behavior at the actual external driver input. The user identified
the driver as HW-517. Its common dual-MOSFET design controls one output, so the
three independent pumps require three modules. Installed quantity and physical
electrical/protection checks remain pending; see [HW-517 wiring](pumps.md#hw-517-driver-wiring).

The [Grothen G328 data sheet](https://ae01.alicdn.com/kf/Sfb54a59c481d4a0f97dff55d549263371.pdf)
lists both 12 V and 24 V versions with 3 x 5 mm tubing; the user has now
confirmed the inlet is the 24 V version. The
[Kamoer NKP manual](https://m.media-amazon.com/images/I/81o9jakHMZL.pdf)
defines DA as a 24 V brushed motor. Its older tube table does not cover S10;
retain the user's outlet flow measurements rather than importing another
variant's flow curve. See [pump hardware notes](pumps.md#identified-pump-hardware).

The earlier KPHM100 and KXF search candidates are superseded by the confirmed
models. In particular, the KPHM100 brushless LOW = full-speed warning does not
apply as a specification for these pumps. The external driver still determines
how ESP32 PWM controls motor power.

Turbidity remains an identification lead, not a confirmed installed part:

- **TS-300B**: a possible OEM analog turbidity module. Elecrow's own listing gives
  a 5 V supply and 0–4.5 V analog output. Board appearance alone cannot establish
  the model or calibration. If this is the installed version, its output needs
  conditioning before connection to the ESP32 ADC; do not connect it directly.
  [Elecrow TS-300B](https://www.elecrow.com/ts-300b-turbidity-sensor-water-turbidity-module-turbidity-detection-module.html).

The default pH/EC/pump/TFT pins do not overlap. GPIO8/10 are ADC1 inputs on the
DevKitC-1; avoid repurposing memory pins for additional peripherals, particularly
GPIO35–37 on Octal-memory variants.
[Espressif board guide](https://documentation.espressif.com/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.0.html).

## Conditions before powered reactor testing

| Open item | Why it matters / required next check |
| --- | --- |
| Pump driver electrical interface | HW-517 identified; 24 V power and 3.3 V / 10 kHz PWM match the common D4184 module specification. Confirm three separate modules, actual component ratings, flyback diodes, startup current/temperature, and LOW/off behavior through reset. Firmware only requests LOW after it starts. |
| Emergency stop and electrode power | No physical interlock inputs or PSU backend exist. Firmware cannot turn off an independently powered electrode supply. Keep electrode power disconnected for monitoring tests. |
| Turbidity | Confirm label, connector pinout, supply and maximum output voltage, then select input conditioning and calibration. Do not substitute an unrelated voltage-to-NTU equation. |
| pH/EC calibration | Both calibration flags remain false. Verify standards and manual liquid temperature before treating readings as calibrated. Check for interference when sensors and pumps operate together. |
| Loop timing and watchdog | FSM, sampling, TFT, and serial share one task. SDK idle-task watchdog checks do not supervise successful completion of this application loop. There is no dedicated fast shutdown task or application watchdog. |
| Manual pump APIs | `set_manual_duty()` and direct `start_all()` bypass FSM readiness checks. They are supervised bench APIs, not an operator safety interface; no serial command parser exists. |
| Persistent records | Calibration changes made through APIs and the 60-record RAM history are lost on reboot. |
| Flash configuration | Use `make build`, `make flash`, and `make monitor` from an activated ESP-IDF environment. Plain root `idf.py` commands select the older 2 MB/no-PSRAM configuration. |

## First bench checks

1. Confirm board and device labels, ground/driver interface, and supply polarity
   with power disconnected. Keep motor power and electrode power disconnected.
2. Build/flash the N16R8 image using the Makefile. Inspect serial at 115200 baud:
   expect successful pump initialization, stopped outputs, and
   `FSM: IDLE | VERIFY PUMP FLOWS`. A missing TFT does not necessarily produce
   an SDK error because the SPI module provides no acknowledgement.
3. Verify GPIO4/5/6 remain LOW after boot and through reset using an instrument.
   Validate the external driver's off behavior before connecting powered pumps.
4. Check sensor voltages before connecting ADC inputs. pH uses GPIO8 directly;
   EC uses a 10 kOhm series / 20 kOhm-to-ground divider into GPIO10, gain 1.5.
   The specified board outputs are up to 3.0 V for
   [SEN0161-V2](https://wiki.dfrobot.com/sen0161-v2) and 3.4 V for
   [DFR0300 V2](https://wiki.dfrobot.com/dfr0300).
5. Confirm pH/EC responses with reference solutions and verify the TFT's seconds
   value advances. An in-range disconnected analog input can look plausible;
   software voltage checks do not prove a probe is connected.
6. After driver compatibility is established, use a supervised pump bench
   procedure to check 10 kHz PWM and measured flows at 90%, 65%, and 42%.
   Check stop/reset behavior and then all three flows together with the intended
   tubing. Current monitoring firmware will not start them automatically.
7. Leave automatic reactor START blocked until required sensors, actual flow
   readiness, interlocks, operator events, and power-control hardware are integrated
   and tested. Passing host tests do not establish physical readiness.

The existing operation introduction stays local under `build/local-docs/` and
is still ignored by Git. This file records review findings and bench acceptance
checks; it does not certify physical hardware operation.
