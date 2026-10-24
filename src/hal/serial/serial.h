#pragma once

// UART/USB transport and default ESP-IDF console adapter.
namespace hal::serial {

void write_line(const char* message);

} // namespace hal::serial
