#pragma once

// Telemetry: publish sensor readings and operating state through the serial driver.
namespace app::telemetry {

void report_startup();
void report_pump_initialization(bool ready);

} // namespace app::telemetry
