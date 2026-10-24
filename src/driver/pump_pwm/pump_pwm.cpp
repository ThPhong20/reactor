#include "driver/pump_pwm/pump_pwm.h"

namespace drivers::pump_pwm {

namespace {
// Track completed channel setup so cleanup also works after partial init.
unsigned configured_channels = 0;
bool initialized = false;
bool init_attempted = false;
} // namespace

bool init(const system_config::PumpConfigs& config)
{
    // A second init stops existing outputs; hardware errors require a reboot.
    if (init_attempted) {
        const bool stopped = stop_all();
        return initialized && stopped;
    }
    init_attempted = true;
    // Two PWM channels must not compete for the same physical output pin.
    for (std::size_t i = 0; i < config.size(); ++i) {
        for (std::size_t j = 0; j < i; ++j) {
            if (config[i].gpio == config[j].gpio) {
                return false;
            }
        }
    }
    if (!hal::pwm::init(system_config::pump_pwm_frequency_hz)) {
        return false;
    }
    for (unsigned channel = 0; channel < config.size(); ++channel) {
        if (!hal::pwm::configure_channel(channel, config[channel].gpio)) {
            stop_all();
            return false;
        }
        ++configured_channels;
    }
    initialized = true;
    return true;
}

bool set_duty(system_config::Pump pump, float percent)
{
    const auto channel = static_cast<unsigned>(pump);
    if (!initialized || static_cast<std::size_t>(pump) >= system_config::pump_count ||
        !std::isfinite(percent) || percent < 0.0f || percent > 100.0f) {
        return false;
    }
    if (!hal::pwm::set_duty(channel, percent)) {
        // A failed write may have partially changed an output. Attempt to stop
        // every configured channel and latch the driver unavailable until reboot.
        initialized = false;
        stop_all();
        return false;
    }
    return true;
}

bool stop_all()
{
    // Do not return early: a failure on one pump must not skip the other pumps.
    // false means at least one stop could not be confirmed by the SDK.
    bool success = true;
    for (unsigned channel = 0; channel < configured_channels; ++channel) {
        if (!hal::pwm::stop(channel)) {
            success = false;
        }
    }
    if (!success) {
        initialized = false;
    }
    return success;
}

} // namespace drivers::pump_pwm
