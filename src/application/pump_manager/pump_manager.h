#pragma once

#include "common/system_config.h"

// Coordinate inlet, outlet 1, and outlet 2. Call only from the application task.
namespace app::pump_manager {

using Pump = system_config::Pump;

// Configures GPIO 4/5/6 with every output off. Never starts pumps automatically.
bool init();
// Store a duty in RAM for the next start_all(); does not change running outputs.
// Reinitialization or reboot reloads the defaults from system_config.cpp.
bool set_operating_duty(Pump pump, float percent);
// Requires nonzero duties for all pumps; flow targets do not imply PWM values.
bool start_all();
// Immediately drive one pump for manual calibration (0% stops that pump).
// Does not replace its saved operating duty or confirm its flow rate.
bool set_manual_duty(Pump pump, float percent);
// Try every channel, including after a fault. Check false for a failed stop.
bool stop_all();

} // namespace app::pump_manager
