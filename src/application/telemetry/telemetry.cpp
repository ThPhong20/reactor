#include "application/telemetry/telemetry.h"

#include "driver/serial/serial.h"

namespace app::telemetry {

void report_startup()
{
    drivers::serial::write_line("Reactor firmware bootstrap (C++).");
}

} // namespace app::telemetry
