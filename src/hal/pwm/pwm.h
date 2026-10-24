#pragma once

#include <cstdint>

// Owns low-speed LEDC timer 0 with 10-bit resolution. Call from one task.
namespace hal::pwm {

bool init(std::uint32_t frequency_hz);
// Routes a channel to its GPIO at zero duty; the timer must be initialized first.
bool configure_channel(unsigned channel, int gpio);
// Converts 0..100% to hardware timing steps. This does not measure or set flow.
bool set_duty(unsigned channel, float percent);
// Holds the output low. A later successful set_duty() can resume PWM.
bool stop(unsigned channel);

} // namespace hal::pwm
