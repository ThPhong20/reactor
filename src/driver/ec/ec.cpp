#include "driver/ec/ec.h"

#include "hal/adc/adc.h"

#include <cmath>

namespace drivers::ec {

namespace {
system_config::EcConfig settings{};
bool ready = false;
bool high_range = false;

bool valid_temperature(float value)
{
    return std::isfinite(value) && value >= 0.0f && value <= 40.0f;
}

bool valid_calibration(const system_config::EcCalibration& calibration)
{
    return std::isfinite(calibration.low_k) && std::isfinite(calibration.high_k) &&
           calibration.low_k > 0.5f && calibration.low_k < 1.5f &&
           calibration.high_k > 0.5f && calibration.high_k < 1.5f;
}
} // namespace

bool init(const system_config::EcConfig& config)
{
    ready = false;
    high_range = false;
    if (!std::isfinite(config.divider_gain) || config.divider_gain < 1.0f ||
        !valid_temperature(config.temperature_c) || !valid_calibration(config.calibration) ||
        !hal::adc::configure(config.gpio)) {
        return false;
    }
    settings = config;
    ready = true;
    return true;
}

bool set_temperature(float temperature_c)
{
    if (!ready || !valid_temperature(temperature_c)) {
        return false;
    }
    settings.temperature_c = temperature_c;
    return true;
}

float temperature()
{
    return ready ? settings.temperature_c : std::numeric_limits<float>::quiet_NaN();
}

bool set_calibration(const system_config::EcCalibration& calibration)
{
    if (!ready || !valid_calibration(calibration)) {
        return false;
    }
    settings.calibration = calibration;
    high_range = false;
    return true;
}

sensor_data::Reading read()
{
    sensor_data::Reading result;
    if (!ready) {
        return result;
    }
    hal::adc::Sample sample;
    if (!hal::adc::read(settings.gpio, sample)) {
        result.status = sensor_data::Status::AdcError;
        return result;
    }
    result.raw = sample.raw;
    result.pin_mv = sample.millivolts;
    result.board_mv = sample.millivolts * settings.divider_gain;
    // The divider reduces the EC board's 0..3.4 V output for the ADC input.
    if (sample.clipped || !std::isfinite(result.pin_mv) || !std::isfinite(result.board_mv) ||
        result.pin_mv <= 0.0f || result.pin_mv > 3100.0f || result.board_mv > 3400.0f) {
        result.status = sensor_data::Status::VoltageOutOfRange;
        return result;
    }
    // DFR0300 V2 K=1 conversion, matching the bench test / DFRobot_EC formula:
    // raw EC = board_mV / 164; EC25 = raw EC * K / (1 + 0.0185 * (T - 25)).
    const float raw_ec = result.board_mv / 164.0f;
    const auto& cal = settings.calibration;
    const float previous = raw_ec * (high_range ? cal.high_k : cal.low_k);
    // Hysteresis prevents repeatedly switching factors around 2 mS/cm.
    bool next_high = high_range;
    if (previous > 2.5f) next_high = true;
    else if (previous < 2.0f) next_high = false;
    const float factor = next_high ? cal.high_k : cal.low_k;
    const float value = raw_ec * factor / (1.0f + 0.0185f * (settings.temperature_c - 25.0f));
    if (!std::isfinite(value) || value < 0.0f || value > 20.0f) {
        result.status = sensor_data::Status::ValueOutOfRange;
        return result;
    }
    high_range = next_high;
    result.value = value;
    result.status = cal.calibrated ? sensor_data::Status::Ok : sensor_data::Status::NeedsCalibration;
    return result;
}

} // namespace drivers::ec
