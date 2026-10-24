#include "system_config.h"

namespace system_config {

RuntimeConfig default_config()
{
    // Baseline runtime configuration values.
    return RuntimeConfig{1000, true};
}

PumpConfigs default_pump_config()
{
    // Target flow balance: inlet 70 = outlet 49 + outlet 21 mL/min.
    // User measurement: inlet delivered 70 mL/min at 90% duty, 10 kHz, 10-bit PWM.
    // This is one measured point, not a linear flow-to-duty formula. Measure each
    // outlet independently; do not assume 49 and 21 mL/min mean 49% and 21% duty.
    // Saved duties are used by start_all(); initialization always sets outputs off.
    // Fields: GPIO, target flow in mL/min, operating duty in percent.
    return {{{4, 70.0f, 90.0f},  // Inlet: measured operating duty.
             {5, 49.0f, 0.0f},   // Outlet 1: duty awaiting calibration.
             {6, 21.0f, 0.0f}}}; // Outlet 2: duty awaiting calibration.
}

} // namespace system_config
