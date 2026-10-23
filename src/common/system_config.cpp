#include "system_config.h"

namespace system_config {

RuntimeConfig default_config()
{
    // Baseline runtime configuration values.
    return RuntimeConfig{1000, true};
}

} // namespace system_config
