#pragma once

// Turbidity sensor driver scaffold; acquisition is not implemented yet.
// This layer will own sensor readings and model-specific calibration.
namespace drivers::turbidity {

// GPIO11 (ADC2 channel 0) is reserved in SensorConfig. ADC2 HAL support exists.
// TODO: Define reading APIs after verifying board voltage, divider, and calibration.

} // namespace drivers::turbidity
