#pragma once

#include "common/display_data.h"
#include "common/sensor_data.h"
#include "common/system_config.h"

namespace app::display_manager {
// Pure formatting is separate from hardware so status handling can be tested.
display_data::Frame format(const sensor_data::Record& record, std::uint64_t now_ms);
// Pass the same sensor configuration used by sensor_manager for pin ownership.
bool init(const system_config::DisplayConfig& config = system_config::default_display_config(),
          const system_config::SensorConfig& sensors = system_config::default_sensor_config());
bool update(const sensor_data::Record& record, std::uint64_t now_ms);
} // namespace app::display_manager
