#include "hal/serial/serial.h"

#include "esp_log.h"
#ifdef REACTOR_HARDWARE_TEST
#include "hal/test_console/test_console.h"
#endif

namespace hal::serial {

// Write a message through the configured ESP-IDF logging console.
void write_line(const char* message)
{
    // Uses the configured ESP-IDF console until an explicit transport is selected.
#ifdef REACTOR_HARDWARE_TEST
    hal::test_console::reply(message); // Never wait on USB while running hardware tests.
#else
    ESP_LOGI("reactor", "%s", message);
#endif
}

} // namespace hal::serial
