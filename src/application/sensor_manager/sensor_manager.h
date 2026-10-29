#pragma once

#include "common/sensor_data.h"
#include "common/system_config.h"

// Coordinate required sensor readings. pH/EC work; required turbidity is pending.
namespace app::sensor_manager {

// Initializes pH and EC independently. true describes only these implemented
// inputs, not completion of the required turbidity or theoretical-pH features.
// Pass the same display configuration to this module and display_manager.
bool init(const system_config::SensorConfig& config = system_config::default_sensor_config(),
          const system_config::DisplayConfig& display = system_config::default_display_config());
// Read available sensors into a fresh snapshot, preserving error status.
sensor_data::Snapshot sample();
// These C++ configuration APIs do not write NVS or parse serial commands.
bool set_ph_calibration(const system_config::PhCalibration& calibration);
// Apply EC calibration in RAM when the EC driver is ready.
bool set_ec_calibration(const system_config::EcCalibration& calibration);
// Update the manual EC compensation temperature when ready.
bool set_ec_temperature(float temperature_c);

} // namespace app::sensor_manager
