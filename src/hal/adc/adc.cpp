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
adc_oneshot_unit_handle_t unit = nullptr;
std::array<adc_cali_handle_t, SOC_ADC_CHANNEL_NUM(0)> calibration{};

bool checked(esp_err_t error)
{
    if (error != ESP_OK) {
        ESP_LOGE("sensor_adc", "%s", esp_err_to_name(error));
    }
    return error == ESP_OK;
}

bool channel_for(int gpio, adc_channel_t& channel)
{
    adc_unit_t requested_unit;
    return checked(adc_oneshot_io_to_channel(gpio, &requested_unit, &channel)) &&
           requested_unit == ADC_UNIT_1 && static_cast<unsigned>(channel) < calibration.size();
}
} // namespace

bool configure(int gpio)
{
    adc_channel_t channel;
    if (!channel_for(gpio, channel)) {
        return false;
    }
    if (calibration[channel] != nullptr) {
        return true; // Reuse an already configured channel without allocating again.
    }
    if (unit == nullptr) {
        adc_oneshot_unit_init_cfg_t config{};
        config.unit_id = ADC_UNIT_1;
        if (!checked(adc_oneshot_new_unit(&config, &unit))) {
            return false;
        }
    }
    adc_oneshot_chan_cfg_t config{};
    config.bitwidth = bitwidth;
    config.atten = attenuation;
    if (!checked(adc_oneshot_config_channel(unit, channel, &config))) {
        return false;
    }
    adc_cali_curve_fitting_config_t curve{};
    curve.unit_id = ADC_UNIT_1;
    curve.chan = channel;
    curve.atten = attenuation;
    curve.bitwidth = bitwidth;
    // A missing ADC calibration is an error; do not silently approximate volts
    // with raw * 3.3 / 4095, which bypasses the ADC's actual response curve.
    return checked(adc_cali_create_scheme_curve_fitting(&curve, &calibration[channel]));
}

bool read(int gpio, Sample& sample)
{
    sample = {};
    adc_channel_t channel;
    if (unit == nullptr || !channel_for(gpio, channel) || calibration[channel] == nullptr) {
        return false;
    }
    int sum = 0;
    bool clipped = false;
    for (unsigned i = 0; i < sample_count; ++i) {
        int raw = 0;
        if (!checked(adc_oneshot_read(unit, channel, &raw))) {
            return false;
        }
        // Check each sample before averaging so a clipped input is not hidden.
        clipped |= raw <= 0 || raw >= 4090;
        sum += raw;
    }
    const int mean = sum / sample_count;
    int millivolts = 0;
    if (!checked(adc_cali_raw_to_voltage(calibration[channel], mean, &millivolts))) {
        return false;
    }
    sample = {mean, static_cast<float>(millivolts), clipped};
    return true;
}

} // namespace hal::adc
