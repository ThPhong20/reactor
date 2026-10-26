#pragma once

#include "common/sensor_data.h"
#include "common/system_config.h"

// EC device conversion and calibration; uses the ADC HAL.
namespace drivers::ec {

bool init(const system_config::EcConfig& config);
sensor_data::Reading read(); // Conductivity compensated to 25 C, in mS/cm.
bool set_temperature(float temperature_c); // Manual input: 0..40 C.
float temperature();
bool set_calibration(const system_config::EcCalibration& calibration);

} // namespace drivers::ec
