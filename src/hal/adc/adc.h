#pragma once

// Shared ADC1 adapter. Call from the application task, not an interrupt handler.
namespace hal::adc {

struct Sample {
    int raw = 0;
    float millivolts = 0.0f;
    bool clipped = false; // Any of the averaged samples touched an ADC rail.
};

// Uses 12-bit sampling, 12 dB attenuation, and per-channel voltage calibration.
// Both pH and EC share one ADC1 unit. ADC2 pins are rejected.
bool configure(int gpio);
// Average 32 samples, then convert the mean through ESP-IDF ADC calibration.
// On SDK failure, clear the output; never return the previous successful sample.
bool read(int gpio, Sample& sample);

} // namespace hal::adc
