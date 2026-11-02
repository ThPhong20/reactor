#include "hal/adc/adc.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "soc/soc_caps.h"

#include <array>

namespace hal::adc {

namespace {
constexpr unsigned sample_count = 32;
constexpr auto attenuation = ADC_ATTEN_DB_12;
constexpr auto bitwidth = ADC_BITWIDTH_12;
struct Unit {
    adc_oneshot_unit_handle_t handle = nullptr;
    std::array<adc_cali_handle_t, SOC_ADC_MAX_CHANNEL_NUM> calibration{};
};
// Channel numbers repeat across ADC1/ADC2; keep their handles/calibration separate.
std::array<Unit, SOC_ADC_PERIPH_NUM> units{};

// Log an ADC SDK error and return whether the call succeeded.
bool checked(esp_err_t error)
{
    if (error != ESP_OK) {
        ESP_LOGE("sensor_adc", "%s", esp_err_to_name(error));
    }
    return error == ESP_OK;
}

// Resolve a GPIO to its ADC unit and channel before indexing their state.
bool channel_for(int gpio, adc_unit_t& unit_id, adc_channel_t& channel)
{
    return checked(adc_oneshot_io_to_channel(gpio, &unit_id, &channel)) &&
           static_cast<unsigned>(unit_id) < units.size() &&
           static_cast<unsigned>(channel) < SOC_ADC_CHANNEL_NUM(unit_id);
}
} // namespace

// Configure an ADC1/ADC2 input and reuse or create its voltage calibration.
bool configure(int gpio)
{
    adc_unit_t unit_id;
    adc_channel_t channel;
    if (!channel_for(gpio, unit_id, channel)) {
        return false;
    }
    auto& unit = units[unit_id];
    if (unit.calibration[channel] != nullptr) {
        return true; // Reuse an already configured channel without allocating again.
    }
    if (unit.handle == nullptr) {
        adc_oneshot_unit_init_cfg_t config{};
        config.unit_id = unit_id;
        if (!checked(adc_oneshot_new_unit(&config, &unit.handle))) {
            return false;
        }
    }
    adc_oneshot_chan_cfg_t config{};
    config.bitwidth = bitwidth;
    config.atten = attenuation;
    if (!checked(adc_oneshot_config_channel(unit.handle, channel, &config))) {
        return false;
    }
    adc_cali_curve_fitting_config_t curve{};
    curve.unit_id = unit_id;
    curve.chan = channel;
    curve.atten = attenuation;
    curve.bitwidth = bitwidth;
    // A missing ADC calibration is an error; do not silently approximate volts
    // with raw * 3.3 / 4095, which bypasses the ADC's actual response curve.
    return checked(adc_cali_create_scheme_curve_fitting(&curve, &unit.calibration[channel]));
}

// Average ADC samples, detect clipping, and return calibrated millivolts.
bool read(int gpio, Sample& sample)
{
    sample = {};
    adc_unit_t unit_id;
    adc_channel_t channel;
    if (!channel_for(gpio, unit_id, channel)) {
        return false;
    }
    const auto& unit = units[unit_id];
    if (unit.handle == nullptr || unit.calibration[channel] == nullptr) {
        return false;
    }
    int sum = 0;
    bool clipped = false;
    for (unsigned i = 0; i < sample_count; ++i) {
        int raw = 0;
        if (!checked(adc_oneshot_read(unit.handle, channel, &raw))) {
            return false;
        }
        // Check each sample before averaging so a clipped input is not hidden.
        clipped |= raw <= 0 || raw >= 4090;
        sum += raw;
    }
    const int mean = sum / sample_count;
    int millivolts = 0;
    if (!checked(adc_cali_raw_to_voltage(unit.calibration[channel], mean, &millivolts))) {
        return false;
    }
    sample = {mean, static_cast<float>(millivolts), clipped};
    return true;
}

} // namespace hal::adc
