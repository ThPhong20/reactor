#include "application/sensor_manager/sensor_manager.h"

#include "driver/ph/ph.h"
#include "driver/ec/ec.h"

namespace app::sensor_manager {

namespace {
bool ph_ready = false;
bool ec_ready = false;
} // namespace

// Check pin ownership and initialize pH and EC independently.
bool init(const system_config::SensorConfig& config, const system_config::DisplayConfig& display)
{
    ph_ready = false;
    ec_ready = false;
    // Validate before configuring ADC pins, including pins owned by the TFT.
    if (!system_config::valid_pin_assignments(config, display)) {
        return false;
    }
    ph_ready = drivers::ph::init(config.ph);
    ec_ready = drivers::ec::init(config.ec);
    return ph_ready && ec_ready;
}

// Read available sensors into a fresh snapshot, preserving error status.
sensor_data::Snapshot sample()
{
    // Read both even if one fails. Each reading carries its own error/calibration
    // status; no failed sample is substituted with an older successful value.
    sensor_data::Snapshot readings;
    readings.ph = ph_ready ? drivers::ph::read() : sensor_data::Reading{};
    readings.ec = ec_ready ? drivers::ec::read() : sensor_data::Reading{};
    readings.ec_temperature_c = ec_ready ? drivers::ec::temperature()
                                       : std::numeric_limits<float>::quiet_NaN();
    // Turbidity stays NotInitialized/NaN until its driver is implemented.
    return readings;
}

// Apply pH calibration in RAM when the pH driver is ready.
bool set_ph_calibration(const system_config::PhCalibration& calibration)
{
    return ph_ready && drivers::ph::set_calibration(calibration);
}

// Apply EC calibration in RAM when the EC driver is ready.
bool set_ec_calibration(const system_config::EcCalibration& calibration)
{
    return ec_ready && drivers::ec::set_calibration(calibration);
}

// Update the manual EC compensation temperature when ready.
bool set_ec_temperature(float temperature_c)
{
    return ec_ready && drivers::ec::set_temperature(temperature_c);
}

} // namespace app::sensor_manager
