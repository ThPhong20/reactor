#include "driver/ph/ph.h"

#include "hal/adc/adc.h"

#include <cmath>

namespace drivers::ph {

namespace {
system_config::PhConfig settings{};
bool ready = false;

bool valid_calibration(const system_config::PhCalibration& calibration)
{
    // SEN0161-V2 pH 7.00 and 4.00 recognition windows from the bench test.
    // Separate windows also guarantee a nonzero, correctly signed slope.
    return std::isfinite(calibration.neutral_mv) && std::isfinite(calibration.acid_mv) &&
           calibration.neutral_mv > 1322.0f && calibration.neutral_mv < 1678.0f &&
           calibration.acid_mv > 1854.0f && calibration.acid_mv < 2210.0f;
}
} // namespace

bool init(const system_config::PhConfig& config)
{
    ready = false;
    if (!valid_calibration(config.calibration) || !hal::adc::configure(config.gpio)) {
        return false;
    }
    settings = config;
    ready = true;
    return true;
}

bool set_calibration(const system_config::PhCalibration& calibration)
{
    if (!ready || !valid_calibration(calibration)) {
        return false;
    }
    settings.calibration = calibration;
    return true;
}

sensor_data::Reading read()
{
    sensor_data::Reading result; // Fresh result: a failed read cannot reuse old pH.
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
    result.board_mv = sample.millivolts; // Direct connection, no divider on pH.
    if (sample.clipped || !std::isfinite(result.board_mv) ||
        result.board_mv <= 0.0f || result.board_mv > 3000.0f) {
        result.status = sensor_data::Status::VoltageOutOfRange;
        return result;
    }
    const auto& cal = settings.calibration;
    // Line through (neutral_mv, pH 7) and (acid_mv, pH 4), matching DFRobot_PH.
    // No pH temperature compensation; calibrate and measure at similar temperature.
    const float value = 7.0f + 3.0f * (result.board_mv - cal.neutral_mv) /
                                    (cal.neutral_mv - cal.acid_mv);
    if (!std::isfinite(value) || value < 0.0f || value > 14.0f) {
        result.status = sensor_data::Status::ValueOutOfRange;
        return result;
    }
    result.value = value;
    result.status = cal.calibrated ? sensor_data::Status::Ok : sensor_data::Status::NeedsCalibration;
    return result;
}

} // namespace drivers::ph
