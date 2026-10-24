#include "application/telemetry/telemetry.h"

#include "driver/serial/serial.h"

namespace app::telemetry {

void report_startup()
{
    drivers::serial::write_line("Reactor firmware bootstrap (C++).");
}

void report_pump_initialization(bool ready)
{
    drivers::serial::write_line(ready
        ? "Pump PWM initialized; all pumps stopped. Outlet duties need manual calibration."
        : "Pump PWM initialization failed; pump commands disabled.");
}

} // namespace app::telemetry
