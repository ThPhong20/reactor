#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace system_config {

struct RuntimeConfig {
    int sample_interval_ms;
    bool logging_enabled;
};

RuntimeConfig default_config();

// Enum order matches the configuration array and LEDC channels 0, 1, and 2.
enum class Pump : std::size_t {
    Inlet,
    Outlet1,
    Outlet2
};
inline constexpr std::size_t pump_count = 3;
// All pumps share this frequency. The 90% inlet measurement used 10 kHz;
// changing frequency or resolution requires checking the measured flow again.
inline constexpr std::uint32_t pump_pwm_frequency_hz = 10000;

struct PumpConfig {
    int gpio;
    float target_flow_ml_min; // Desired flow, not a measured value or duty percentage.
    float duty_percent;       // Saved operating duty (0..100%), not the startup output.
};

using PumpConfigs = std::array<PumpConfig, pump_count>;
PumpConfigs default_pump_config();

// Sensor calibration is separate from the ESP32 ADC's voltage calibration.
// Defaults are estimates until BOTH reference solutions have been measured.
struct PhCalibration {
    float neutral_mv = 1500.0f; // Board voltage in pH 7.00 buffer.
    float acid_mv = 2032.44f;   // Board voltage in pH 4.00 buffer.
    bool calibrated = false;
};

struct EcCalibration {
    float low_k = 1.0f;  // Correction measured with 1.413 mS/cm standard.
    float high_k = 1.0f; // Correction measured with 12.88 mS/cm standard.
    bool calibrated = false;
};

struct PhConfig {
    int gpio;
    PhCalibration calibration;
};

struct EcConfig {
    int gpio;
    float divider_gain;
    float temperature_c; // Manually supplied liquid temperature, not a sensor reading.
    EcCalibration calibration;
};

struct SensorConfig {
    PhConfig ph;
    EcConfig ec;
};

SensorConfig default_sensor_config();

// GMT130 / ST7789 240x240, SPI2. These are DevKitC-1 GPIO numbers,
// not Seeed XIAO D-pin labels. No CS/backlight GPIO on the seven-pin module.
struct DisplayConfig {
    bool enabled = true;
    int sclk = 12;
    int mosi = 11;
    int dc = 10;
    int reset = 9;
    int y_offset = 0; // Set to 80 only if this panel needs that vertical offset.
    bool invert_colors = true;
};
DisplayConfig default_display_config();

// Shared board checks used before either sensor or display initialization.
// Pump pins come from default_pump_config(), as used by the pump manager.
bool valid_display_config(const DisplayConfig& display);
bool valid_pin_assignments(const SensorConfig& sensors, const DisplayConfig& display);

} // namespace system_config
