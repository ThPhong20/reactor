#include "hal/serial/serial.h"

#include "driver/uart.h"
#include "esp_log.h"

namespace hal::serial {

void write_line(const char* message)
{
    // Uses the configured ESP-IDF console until an explicit transport is selected.
    ESP_LOGI("reactor", "%s", message);
}

} // namespace hal::serial
