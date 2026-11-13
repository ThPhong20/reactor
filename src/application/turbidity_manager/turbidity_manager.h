#pragma once
#include "common/system_config.h"
#include "common/sensor_data.h"
namespace app::turbidity_manager {
// Leave the input untouched until the configuration confirms safe wiring.
bool init(const system_config::SensorConfig& config);
// Return current raw voltage or an explicit unavailable/error status.
sensor_data::Reading sample();
}
