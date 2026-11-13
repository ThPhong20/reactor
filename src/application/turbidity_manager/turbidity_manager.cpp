#include "application/turbidity_manager/turbidity_manager.h"
#include "driver/turbidity/turbidity.h"
namespace app::turbidity_manager {
namespace { bool ready = false; }
// Treat an intentionally disabled input as optional during hardware preparation.
bool init(const system_config::SensorConfig& config)
{
    ready = false;
    if (!config.turbidity_enabled) return true;
    ready = drivers::turbidity::init(config.turbidity_gpio, config.turbidity_divider_gain);
    return ready;
}
// Never reuse an old voltage after acquisition failure or disabled configuration.
sensor_data::Reading sample()
{
    return ready ? drivers::turbidity::read() : sensor_data::Reading{};
}
}
