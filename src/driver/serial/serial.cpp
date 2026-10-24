#include "driver/serial/serial.h"

#include "hal/serial/serial.h"

namespace drivers::serial {

void write_line(const char* message)
{
    hal::serial::write_line(message);
}

} // namespace drivers::serial
