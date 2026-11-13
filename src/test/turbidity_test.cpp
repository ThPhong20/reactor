#include "application/turbidity_manager/turbidity_manager.h"
#include "application/display_manager/display_manager.h"
#include "driver/turbidity/turbidity.h"
#include "driver/tft/tft.h"
#include "hal/adc/adc.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
namespace {
bool adc_ok = true;
bool clipped = false;
float voltage = 2000;
unsigned configurations = 0;
// Report a failed expected behavior and stop this independent scenario.
void require(bool condition, const char* message)
{
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
}
namespace hal::adc {
// Model initialization without accessing GPIO hardware.
bool configure(int gpio) { ++configurations; return gpio == 11; }
// Inject ADC voltage, clipping, and acquisition failures.
bool read(int, Sample& sample) { sample = {2000, voltage, clipped}; return adc_ok; }
}
namespace drivers::tft {
// Supply display linkage while testing text formatting only.
bool init(const system_config::DisplayConfig&) { return true; }
// Avoid hardware drawing in the formatter test.
bool show(const display_data::Frame&) { return true; }
}
// Verify raw voltage math, unavailable NTU, and failed/disabled input behavior.
int main(int argc, char** argv)
{
    require(argc == 2, "scenario required");
    const std::string scenario = argv[1];
    auto config = system_config::default_sensor_config();
    if (scenario == "disabled") {
        require(app::turbidity_manager::init(config), "disabled input is optional");
        require(configurations == 0, "disabled input must not touch ADC");
        require(app::turbidity_manager::sample().status == sensor_data::Status::NotInitialized, "disabled status");
        return 0;
    }
    if (scenario == "divider") {
        for (float gain : {0.0f, 1.5f, 11.0f, std::numeric_limits<float>::quiet_NaN()})
            require(!drivers::turbidity::init(11, gain), "unsafe divider must fail");
        require(configurations == 0, "invalid gain must not touch ADC");
        return 0;
    }
    config.turbidity_enabled = true;
    require(app::turbidity_manager::init(config), "input should initialize");
    if (scenario == "errors") {
        adc_ok = false;
        require(app::turbidity_manager::sample().status == sensor_data::Status::AdcError, "ADC failure");
        adc_ok = true;
        for (float bad : {0.0f, -1.0f, 2300.0f, std::numeric_limits<float>::quiet_NaN()}) {
            voltage = bad;
            require(app::turbidity_manager::sample().status == sensor_data::Status::VoltageOutOfRange, "voltage failure");
        }
        voltage = 2000;
        clipped = true;
        require(app::turbidity_manager::sample().status == sensor_data::Status::VoltageOutOfRange, "clipping failure");
        return 0;
    }
    require(scenario == "voltage", "unknown scenario");
    const auto reading = app::turbidity_manager::sample();
    require(reading.status == sensor_data::Status::RawOnly && reading.board_mv == 4000, "divider reconstruction");
    require(!sensor_data::has_value(reading) && std::isnan(reading.value), "voltage must never claim NTU readiness");
    sensor_data::Record record;
    record.sequence = 1;
    record.sensors.turbidity = reading;
    const auto frame = app::display_manager::format(record, 0);
    require(std::string(frame[6].text.data()) == "TURB 4.00 V", "TFT must show volts, not NTU");
    require(std::string(frame[7].text.data()) == "VOLTAGE ONLY", "TFT must explain missing conversion");
}
