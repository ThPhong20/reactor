#include "driver/serial/serial.h"

#include "hal/serial/serial.h"

namespace drivers::serial {

// Forward a telemetry message to the serial HAL.
void write_line(const char* message)
{
    hal::serial::write_line(message);
}

} // namespace drivers::serial
