#pragma once

#include "common/display_data.h"
#include "common/system_config.h"

namespace drivers::tft {
// Initialize the LCD hardware and remember its readiness.
bool init(const system_config::DisplayConfig& config);
// Transfer all text rows and enable the panel after a complete frame.
bool show(const display_data::Frame& frame);
} // namespace drivers::tft
