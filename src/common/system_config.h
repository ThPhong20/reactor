#pragma once

namespace system_config {

struct RuntimeConfig {
    int sample_interval_ms;
    bool logging_enabled;
};

RuntimeConfig default_config();

} // namespace system_config
