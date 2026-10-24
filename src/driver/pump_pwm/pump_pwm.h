#pragma once

#include "common/system_config.h"

// Three active-high pump PWM channels. All calls must come from one task.
namespace drivers::pump_pwm {

// Uses only pin assignments; saved flow targets and duties do not start outputs.
bool init(const system_config::PumpConfigs& config);
// A hardware write failure triggers stop attempts for all pumps and blocks
// further duty changes until reboot. Invalid arguments simply return false.
bool set_duty(system_config::Pump pump, float percent);
// Always attempts every configured channel, even after an earlier stop fails.
bool stop_all();

} // namespace drivers::pump_pwm
