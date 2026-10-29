#pragma once

// UART/USB transport and default ESP-IDF console adapter.
namespace hal::serial {

// Write a message through the configured ESP-IDF logging console.
void write_line(const char* message);

} // namespace hal::serial
