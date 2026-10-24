#include "application/pump_manager/pump_manager.h"

#include "driver/pump_pwm/pump_pwm.h"

#include <cmath>

namespace app::pump_manager {

namespace {
system_config::PumpConfigs pumps{};
bool initialized = false;

bool valid_duty(float percent)
{
    return std::isfinite(percent) && percent >= 0.0f && percent <= 100.0f;
}
} // namespace

bool init()
{
    // Loading the measured 90% inlet duty only stores a setting. The driver
    // initializes each physical output at zero; starting requires an explicit call.
    pumps = system_config::default_pump_config();
    initialized = drivers::pump_pwm::init(pumps);
    for (const auto& pump : pumps) {
        if (!valid_duty(pump.duty_percent)) {
            stop_all();
            initialized = false;
            return false;
        }
    }
    return initialized;
}

bool set_operating_duty(Pump pump, float percent)
{
    const auto index = static_cast<std::size_t>(pump);
    if (!initialized || index >= pumps.size() || !valid_duty(percent)) {
        return false;
    }
    pumps[index].duty_percent = percent;
    return true;
}

bool start_all()
{
    if (!initialized) {
        return false;
    }
    // Validate the entire set before changing any output. The two uncalibrated
    // outlet defaults are zero, so the inlet cannot start alone through this API.
    for (const auto& pump : pumps) {
        if (!valid_duty(pump.duty_percent) || pump.duty_percent == 0.0f) {
            return false;
        }
    }
    // Channels update in order, not simultaneously. If a write fails, the driver
    // attempts to stop all three, including any channels already started here.
    for (std::size_t i = 0; i < pumps.size(); ++i) {
        if (!set_manual_duty(static_cast<Pump>(i), pumps[i].duty_percent)) {
            return false;
        }
    }
    return true;
}

bool set_manual_duty(Pump pump, float percent)
{
    if (!initialized || static_cast<std::size_t>(pump) >= pumps.size() || !valid_duty(percent)) {
        return false;
    }
    if (!drivers::pump_pwm::set_duty(pump, percent)) {
        // The driver has already attempted shutdown; block further manager starts.
        initialized = false;
        return false;
    }
    return true;
}

bool stop_all()
{
    // Keep stop available even when initialization failed or a fault was latched.
    const bool stopped = drivers::pump_pwm::stop_all();
    if (!stopped) {
        initialized = false;
    }
    return stopped;
}

} // namespace app::pump_manager
