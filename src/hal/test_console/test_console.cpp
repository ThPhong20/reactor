#include "hal/test_console/test_console.h"
#include "driver/usb_serial_jtag.h"
#include <cstdio>

namespace hal::test_console {
namespace { bool ready = false; }
// Install native USB buffers; GPIO19/20 must remain connected to USB only.
bool init()
{
    usb_serial_jtag_driver_config_t config{};
    config.tx_buffer_size = 1024;
    config.rx_buffer_size = 256;
    ready = usb_serial_jtag_driver_install(&config) == ESP_OK;
    return ready;
}

// Consume a bounded batch without waiting for input.
int read(char* data, std::size_t size)
{
    return usb_serial_jtag_read_bytes(data, size, 0);
}

// Avoid blocking the timed-stop loop when the host is absent or not reading.
void reply(const char* text)
{
    if (!ready) return;
    char message[256];
    const int length = std::snprintf(message, sizeof(message), "%s\r\n", text);
    if (length > 0 && length < static_cast<int>(sizeof(message))) {
        usb_serial_jtag_write_bytes(message, length, 0);
    }
}

// USB SOF packets indicate bus presence; closing a terminal may not remove them.
bool connected()
{
    return usb_serial_jtag_is_connected();
}

// Drain at most the receive-buffer capacity so input floods cannot starve timing.
void discard_input()
{
    char discarded[256];
    read(discarded, sizeof(discarded));
}
} // namespace hal::test_console
