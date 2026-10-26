#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

namespace sensor_data {

enum class Status { NotInitialized, AdcError, VoltageOutOfRange, ValueOutOfRange,
                    NeedsCalibration, Ok };

struct Reading {
    Status status = Status::NotInitialized;
    int raw = 0; // Averaged 12-bit ADC count, useful for wiring diagnostics.
    float pin_mv = std::numeric_limits<float>::quiet_NaN();
    float board_mv = std::numeric_limits<float>::quiet_NaN();
    float value = std::numeric_limits<float>::quiet_NaN(); // pH, EC25 mS/cm, or turbidity NTU.
};

// One validity rule for storage consumers, serial output, and the TFT.
// Drivers enforce sensor-specific ranges; all current sensor values are >= 0.
inline Status effective_status(const Reading& reading)
{
    const bool reports_value = reading.status == Status::Ok || reading.status == Status::NeedsCalibration;
    if (reports_value && (!std::isfinite(reading.value) || reading.value < 0.0f)) {
        return Status::ValueOutOfRange;
    }
    return reading.status;
}

inline bool has_value(const Reading& reading)
{
    const auto status = effective_status(reading);
    return status == Status::Ok || status == Status::NeedsCalibration;
}

struct Snapshot {
    Reading ph;
    Reading ec; // Required EC25 input for the planned theoretical-pH calculation.
    float ec_temperature_c = 25.0f; // Manual compensation input.
    Reading turbidity; // Required sensor; unavailable until acquisition is implemented.
};

struct Record {
    Snapshot sensors;
    std::uint64_t sampled_at_ms = 0; // Monotonic uptime, not wall-clock date/time.
    std::uint64_t sequence = 0;      // Starts at 1; zero means no sample stored.
};

} // namespace sensor_data
