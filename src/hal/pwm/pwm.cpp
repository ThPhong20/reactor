#include "hal/pwm/pwm.h"

#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include <array>
#include <cmath>

namespace hal::pwm {

namespace {
constexpr auto mode = LEDC_LOW_SPEED_MODE;
constexpr auto timer = LEDC_TIMER_0;
// One 10-bit PWM period has 2^10 = 1024 timing steps. Derive the scale from the
// timer resolution so duty conversion stays consistent with the hardware setup.
constexpr auto resolution = LEDC_TIMER_10_BIT;
constexpr unsigned duty_steps = 1U << static_cast<unsigned>(resolution);
std::array<bool, LEDC_CHANNEL_MAX> configured{};
bool timer_ready = false;

bool checked(esp_err_t error)
{
    if (error != ESP_OK) {
        ESP_LOGE("pump_pwm", "%s", esp_err_to_name(error));
    }
    return error == ESP_OK;
}
} // namespace

bool init(std::uint32_t frequency_hz)
{
    // Reconfiguring a shared timer could change every running pump's waveform.
    if (timer_ready || frequency_hz == 0) {
        return false;
    }
    ledc_timer_config_t config{};
    config.speed_mode = mode;
    config.duty_resolution = resolution;
    config.timer_num = timer;
    config.freq_hz = frequency_hz;
    config.clk_cfg = LEDC_AUTO_CLK;
    timer_ready = checked(ledc_timer_config(&config));
    return timer_ready;
}

bool configure_channel(unsigned channel, int gpio)
{
    if (!timer_ready || channel >= configured.size() || configured[channel] ||
        !GPIO_IS_VALID_OUTPUT_GPIO(gpio)) {
        return false;
    }
    // Establish a low level before handing the pin to LEDC.
    if (!checked(gpio_set_level(static_cast<gpio_num_t>(gpio), 0)) ||
        !checked(gpio_set_direction(static_cast<gpio_num_t>(gpio), GPIO_MODE_OUTPUT))) {
        return false;
    }
    ledc_channel_config_t config{};
    config.gpio_num = gpio;
    config.speed_mode = mode;
    config.channel = static_cast<ledc_channel_t>(channel);
    config.timer_sel = timer;
    config.duty = 0; // Never apply a saved operating duty during initialization.
    config.hpoint = 0;
    configured[channel] = checked(ledc_channel_config(&config));
    return configured[channel];
}

bool set_duty(unsigned channel, float percent)
{
    if (channel >= configured.size() || !configured[channel] ||
        !std::isfinite(percent) || percent < 0.0f || percent > 100.0f) {
        return false;
    }
    if (percent == 0.0f) {
        return stop(channel);
    }
    // raw duty = round(percent / 100 * 1024).
    // Example: 90% -> round(921.6) = 922, approximately 90.04% after rounding.
    // At 10 kHz, a period lasts 100 us; 922 steps keep the signal high for
    // approximately 90.04 us. At this resolution, 1024 is the full-on setting.
    const auto duty = static_cast<std::uint32_t>(std::lround(percent * duty_steps / 100.0f));
    // Setting duty writes the new value; update_duty applies it and resumes a
    // stopped channel. Keep both calls in the same application task.
    return checked(ledc_set_duty(mode, static_cast<ledc_channel_t>(channel), duty)) &&
           checked(ledc_update_duty(mode, static_cast<ledc_channel_t>(channel)));
}

bool stop(unsigned channel)
{
    if (channel >= configured.size() || !configured[channel]) {
        return false;
    }
    // Active-high control: idle level 0 requests off. The return value confirms
    // the SDK operation, not that a physical pump has stopped moving liquid.
    return checked(ledc_stop(mode, static_cast<ledc_channel_t>(channel), 0));
}

} // namespace hal::pwm
