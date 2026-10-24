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

} // namespace system_config
