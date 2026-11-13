#pragma once
#include <cstddef>

namespace hal::test_console {
// Install the native USB Serial/JTAG transport once for the bench image.
bool init();
// Read immediately available bytes; never wait for a complete command.
int read(char* data, std::size_t size);
// Queue a best-effort reply; full output buffers drop replies, never delay pumps.
void reply(const char* text);
// Report USB bus presence, not whether the monitor application is open.
bool connected();
// Discard a bounded amount of queued input after stop/disconnect.
void discard_input();
} // namespace hal::test_console
