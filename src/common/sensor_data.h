#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

namespace sensor_data {

// One freshness limit for the FSM and the operator display.
inline constexpr std::uint64_t max_sample_age_ms = 3000;

enum class Status { NotInitialized, AdcError, VoltageOutOfRange, ValueOutOfRange,
                    NeedsCalibration, Ok };


// A single sensor reading, including raw ADC counts, voltages, and the final value.
struct Reading {
    Status status = Status::NotInitialized;
    int raw = 0; // Averaged 12-bit ADC count, useful for wiring diagnostics.
    float pin_mv = std::numeric_limits<float>::quiet_NaN();
    float board_mv = std::numeric_limits<float>::quiet_NaN();
    float value = std::numeric_limits<float>::quiet_NaN(); // pH, EC25 mS/cm, or turbidity NTU.
};


// Reject invalid numbers even when a driver reports a successful reading.
inline Status effective_status(const Reading& reading)
{
    const bool reports_value = reading.status == Status::Ok || reading.status == Status::NeedsCalibration;
    if (reports_value && (!std::isfinite(reading.value) || reading.value < 0.0f)) {
        return Status::ValueOutOfRange;
    }
    return reading.status;
}

// Returns true if the reading has a valid value, even if calibration is needed.
inline bool has_value(const Reading& reading)
{
    const auto status = effective_status(reading);
    return status == Status::Ok || status == Status::NeedsCalibration;
}


// A snapshot of all sensors at a single point in time, including the manually supplied temperature.
struct Snapshot {
    Reading ph;
    Reading ec;
    float ec_temperature_c = 25.0f; // Manual compensation input.
    Reading turbidity; // Unavailable until acquisition/calibration is implemented.
};


// A stored record of sensor readings, including the time and sequence number.
struct Record {
    Snapshot sensors;
    std::uint64_t sampled_at_ms = 0; // Acquisition start, in monotonic milliseconds.
    std::uint64_t sequence = 0;      // Starts at 1; zero means no sample stored.
};

// Check age without unsigned underflow; sequence zero means no stored sample.
inline bool is_fresh(const Record& record, std::uint64_t now_ms)
{
    return record.sequence != 0 && now_ms >= record.sampled_at_ms &&
           now_ms - record.sampled_at_ms <= max_sample_age_ms;
}

} // namespace sensor_data
