#pragma once

#include "common/display_data.h"
#include "common/system_config.h"

namespace drivers::tft {
bool init(const system_config::DisplayConfig& config);
bool show(const display_data::Frame& frame);
} // namespace drivers::tft
