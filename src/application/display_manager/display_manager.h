#pragma once

#include "common/display_data.h"
#include "common/sensor_data.h"
#include "common/system_config.h"

namespace app::display_manager {
// Pure formatting is separate from hardware so status handling can be tested.
display_data::Frame format(const sensor_data::Record& record, std::uint64_t now_ms,
                          const char* state = nullptr, const char* reason = nullptr);
// Pass the same sensor configuration used by sensor_manager for pin ownership.
bool init(const system_config::DisplayConfig& config = system_config::default_display_config(),
          const system_config::SensorConfig& sensors = system_config::default_sensor_config());
// Draw the latest record; disable further updates after a failure.
bool update(const sensor_data::Record& record, std::uint64_t now_ms,
            const char* state = nullptr, const char* reason = nullptr);
} // namespace app::display_manager
