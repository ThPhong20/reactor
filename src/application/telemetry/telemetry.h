#pragma once

#include "common/sensor_data.h"

// Telemetry: publish sensor readings and operating state through the serial driver.
namespace app::telemetry {
// Report whether TFT updates are available.
void report_display_status(bool ready);
// Print the FSM state and its blocker or fault reason.
void report_state(const char* state, const char* reason);

// Print the firmware startup message.
void report_startup();
// Report pump initialization and the stopped startup condition.
void report_pump_initialization(bool ready);
// Report sensor initialization and currently missing features.
void report_sensor_initialization(bool ready);
// Print pH, EC, and turbidity readings with their status.
void report_sensors(const sensor_data::Snapshot& readings);

} // namespace app::telemetry
