#pragma once

#include "common/display_data.h"
#include "common/system_config.h"

namespace hal::lcd {
// Owns SPI2 and the ST7789 panel. Single application task, one init per boot.
bool init(const system_config::DisplayConfig& config);
// Copies to a DMA buffer and waits for completion before allowing reuse.
bool write_row(int row, const display_data::Pixels& pixels);
bool enable();
} // namespace hal::lcd
