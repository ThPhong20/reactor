#pragma once

#include "common/sensor_data.h"
#include "common/system_config.h"

// pH device conversion and calibration; uses the ADC HAL.
namespace drivers::ph {

bool init(const system_config::PhConfig& config);
sensor_data::Reading read();
// Apply BOTH measured buffer voltages. Runtime updates last until reboot;
// put measured values in default_sensor_config() to retain them in firmware.
bool set_calibration(const system_config::PhCalibration& calibration);

} // namespace drivers::ph
