#pragma once

#include "common/sensor_data.h"

// Telemetry: publish sensor readings and operating state through the serial driver.
namespace app::telemetry {
void report_display_status(bool ready);

void report_startup();
void report_pump_initialization(bool ready);
void report_sensor_initialization(bool ready);
void report_sensors(const sensor_data::Snapshot& readings);

} // namespace app::telemetry
