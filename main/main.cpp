#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char* TAG = "reactor_main";

extern "C" void app_main(void)
{
    // Baseline startup hook.
    ESP_LOGI(TAG, "Reactor firmware bootstrap (C++).");

    // Keep the task alive in this initial frame.
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
