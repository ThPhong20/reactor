#pragma once
#include "common/sensor_data.h"

namespace drivers::turbidity {
// Configure a TS-300B input only after its external divider is verified.
bool init(int gpio, float divider_gain);
// Return board voltage only; NTU stays unavailable without a calibration model.
sensor_data::Reading read();
} // namespace drivers::turbidity
