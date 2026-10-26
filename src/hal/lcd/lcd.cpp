#include "hal/lcd/lcd.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <cstring>

namespace hal::lcd {
namespace {
constexpr const char* tag = "reactor_tft";
constexpr std::size_t row_bytes = sizeof(display_data::Pixels);
esp_lcd_panel_io_handle_t io = nullptr;
esp_lcd_panel_handle_t panel = nullptr;
SemaphoreHandle_t complete = nullptr;
std::uint16_t* dma_pixels = nullptr;
bool bus_owned = false;
bool attempted = false;
bool ready = false;
bool enabled = false;

bool transfer_done(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void* context)
{
    BaseType_t wake = pdFALSE;
    xSemaphoreGiveFromISR(static_cast<SemaphoreHandle_t>(context), &wake);
    return wake == pdTRUE;
}

bool check(esp_err_t result)
{
    if (result == ESP_OK) return true;
    ESP_LOGE(tag, "%s; display disabled until reboot", esp_err_to_name(result));
    ready = false;
    return false;
}
} // namespace

bool init(const system_config::DisplayConfig& config)
{
    if (attempted) {
        return ready;
    }
    attempted = true;
    if (!config.enabled || !system_config::valid_display_config(config)) {
        return false;
    }
    // Board policy lives in system_config; the HAL checks SDK pin capability.
    const int pins[] = {config.sclk, config.mosi, config.dc, config.reset};
    for (int pin : pins) {
        if (!GPIO_IS_VALID_OUTPUT_GPIO(pin)) {
            return false;
        }
    }

    // A 24-row strip needs 11.25 KiB instead of a full 112.5 KiB framebuffer.
    dma_pixels = static_cast<std::uint16_t*>(heap_caps_malloc(row_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    complete = xSemaphoreCreateBinary();
    const auto setup = [&]() -> bool {
        if (!dma_pixels || !complete) return check(ESP_ERR_NO_MEM);
        spi_bus_config_t bus{};
        bus.sclk_io_num = config.sclk;
        bus.mosi_io_num = config.mosi;
        bus.miso_io_num = -1;
        bus.quadwp_io_num = -1;
        bus.quadhd_io_num = -1;
        bus.max_transfer_sz = row_bytes;
        if (!check(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO))) return false;
        bus_owned = true;
        esp_lcd_panel_io_spi_config_t io_config{};
        io_config.cs_gpio_num = GPIO_NUM_NC; // GMT130 seven-pin module has no CS.
        io_config.dc_gpio_num = static_cast<gpio_num_t>(config.dc);
        io_config.spi_mode = 3; // Bench-tested idle-high clock for this module.
        io_config.pclk_hz = 20 * 1000 * 1000;
        io_config.trans_queue_depth = 1;
        io_config.on_color_trans_done = transfer_done;
        io_config.user_ctx = complete;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        if (!check(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &io))) return false;
        esp_lcd_panel_dev_config_t device{};
        device.reset_gpio_num = static_cast<gpio_num_t>(config.reset);
        device.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        device.data_endian = LCD_RGB_DATA_ENDIAN_LITTLE;
        device.bits_per_pixel = 16;
        return check(esp_lcd_new_panel_st7789(io, &device, &panel)) &&
               check(esp_lcd_panel_reset(panel)) && check(esp_lcd_panel_init(panel)) &&
               check(esp_lcd_panel_disp_on_off(panel, false)) &&
               check(esp_lcd_panel_set_gap(panel, 0, config.y_offset)) &&
               check(esp_lcd_panel_invert_color(panel, config.invert_colors));
    };
    ready = setup();
    if (!ready) {
        // No color transfers have started during setup; cleanup is safe here.
        if (panel) {
            esp_lcd_panel_del(panel);
            panel = nullptr;
        }
        if (io) {
            esp_lcd_panel_io_del(io);
            io = nullptr;
        }
        if (bus_owned) {
            spi_bus_free(SPI2_HOST);
            bus_owned = false;
        }
        if (complete) {
            vSemaphoreDelete(complete);
            complete = nullptr;
        }
        heap_caps_free(dma_pixels);
        dma_pixels = nullptr;
    }
    return ready;
}

bool write_row(int row, const display_data::Pixels& pixels)
{
    if (!ready || row < 0 || row >= static_cast<int>(display_data::row_count)) return false;
    std::memcpy(dma_pixels, pixels.data(), row_bytes);
    const int y = row * display_data::row_height;
    if (!check(esp_lcd_panel_draw_bitmap(panel, 0, y, display_data::width,
                                        y + display_data::row_height, dma_pixels))) return false;
    if (xSemaphoreTake(complete, pdMS_TO_TICKS(250)) != pdTRUE) {
        // DMA might still be reading. Retain its buffer/semaphore and disable
        // further writes until reboot, so sampling can continue safely.
        return check(ESP_ERR_TIMEOUT);
    }
    return true;
}

bool enable()
{
    if (!ready) return false;
    if (!enabled) enabled = check(esp_lcd_panel_disp_on_off(panel, true));
    return enabled;
}
} // namespace hal::lcd
