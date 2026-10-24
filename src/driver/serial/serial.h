#pragma once

// USB/UART device driver; uses the serial HAL.
namespace drivers::serial {

void write_line(const char* message);

} // namespace drivers::serial
