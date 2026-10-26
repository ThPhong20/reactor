#include "common/system_config.h"

namespace system_config {

DisplayConfig default_display_config()
{
    return {};
}

RuntimeConfig default_config()
{
    // Baseline runtime configuration values.
    return RuntimeConfig{1000, true};
}

PumpConfigs default_pump_config()
{
    // Target flow balance: inlet 70 = outlet 49 + outlet 21 mL/min.
    // User measurement: inlet delivered 70 mL/min at 90% duty, 10 kHz, 10-bit PWM.
    // This is one measured point, not a linear flow-to-duty formula. Measure each
    // outlet independently; do not assume 49 and 21 mL/min mean 49% and 21% duty.
    // Saved duties are used by start_all(); initialization always sets outputs off.
    // Fields: GPIO, target flow in mL/min, operating duty in percent.
    return {{{4, 70.0f, 90.0f},  // Inlet: measured operating duty.
             {5, 49.0f, 0.0f},   // Outlet 1: duty awaiting calibration.
             {6, 21.0f, 0.0f}}}; // Outlet 2: duty awaiting calibration.
}

SensorConfig default_sensor_config()
{
    // ESP32-S3 DevKitC-1 N16R8: use GPIO numbers, not the XIAO D0 label.
    // pH SEN0161-V2: analog output directly to GPIO1 (ADC1 channel 0).
    // EC DFR0300 V2 K=1: output -> 10k -> GPIO2 -> 20k -> GND.
    // Restore EC board voltage by multiplying the ADC pin voltage by 1.5.
    // GPIO4/5/6 remain reserved for pumps. Temperature defaults to MANUAL 25 C.
    return {{1, {}}, {2, 1.5f, 25.0f, {}}};
}

bool valid_display_config(const DisplayConfig& display)
{
    if (!display.enabled) {
        return true; // Disabled hardware reserves no pins.
    }
    // DevKitC-1 N16R8 policy: keep the TFT away from USB, strapping,
    // flash/PSRAM pins, and the default analog/pump pins.
    const std::array pins{display.sclk, display.mosi, display.dc, display.reset};
    for (std::size_t i = 0; i < pins.size(); ++i) {
        if (pins[i] < 7 || pins[i] > 18) {
            return false;
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (pins[i] == pins[j]) {
                return false;
            }
        }
    }
    return display.y_offset >= 0 && display.y_offset <= 80;
}

bool valid_pin_assignments(const SensorConfig& sensors, const DisplayConfig& display)
{
    // Check sensor ownership even if a display setting is invalid: its selected
    // pins must never be taken by an ADC input while the display is enabled.
    // Analog input support itself is checked by the ADC HAL.
    const std::array sensor_pins{sensors.ph.gpio, sensors.ec.gpio};
    const std::array display_pins{display.sclk, display.mosi, display.dc, display.reset};
    if (sensors.ph.gpio == sensors.ec.gpio) {
        return false;
    }
    for (int pin : sensor_pins) {
        if (pin < 0) {
            return false;
        }
        for (const auto& pump : default_pump_config()) {
            if (pin == pump.gpio) {
                return false;
            }
        }
        if (display.enabled) {
            for (int display_pin : display_pins) {
                if (pin == display_pin) {
                    return false;
                }
            }
        }
    }
    if (display.enabled) {
        for (int pin : display_pins) {
            for (const auto& pump : default_pump_config()) {
                if (pin == pump.gpio) {
                    return false;
                }
            }
        }
    }
    return true;
}

} // namespace system_config
