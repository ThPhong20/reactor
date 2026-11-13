#include "driver/turbidity/turbidity.h"
#include "hal/adc/adc.h"
#include <cmath>

namespace drivers::turbidity {
namespace {
bool ready = false;
int input_gpio = -1;
float gain = 0;
}
// Reject divider settings that could exceed the ADC's usable voltage range.
bool init(int gpio, float divider_gain)
{
    ready = false;
    if (!std::isfinite(divider_gain) || divider_gain < 2.0f || divider_gain > 10.0f) return false;
    input_gpio = gpio;
    gain = divider_gain;
    ready = hal::adc::configure(gpio);
    return ready;
}
// Preserve diagnostic voltage without inventing a voltage-to-NTU equation.
sensor_data::Reading read()
{
    sensor_data::Reading result;
    if (!ready) return result;
    hal::adc::Sample sample;
    if (!hal::adc::read(input_gpio, sample)) {
        result.status = sensor_data::Status::AdcError;
        return result;
    }
    result.raw = sample.raw;
    result.pin_mv = sample.millivolts;
    result.board_mv = sample.millivolts * gain;
    if (sample.clipped || !std::isfinite(result.board_mv) || result.pin_mv <= 0 ||
        result.pin_mv > 3100 || result.board_mv > 4500) {
        result.status = sensor_data::Status::VoltageOutOfRange;
    } else {
        result.status = sensor_data::Status::RawOnly;
    }
    return result;
}
} // namespace drivers::turbidity
