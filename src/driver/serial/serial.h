#pragma once

// USB/UART device driver; uses the serial HAL.
namespace drivers::serial {

// Forward a telemetry message to the serial HAL.
void write_line(const char* message);

} // namespace drivers::serial
