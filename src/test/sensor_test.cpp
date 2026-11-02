#include "application/sensor_manager/sensor_manager.h"
#include "application/telemetry/telemetry.h"
#include "driver/ec/ec.h"
#include "driver/ph/ph.h"
#include "driver/serial/serial.h"
#include "hal/adc/adc.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
namespace sensors = app::sensor_manager;
using sensor_data::Status;
constexpr int ph_gpio = 8;
constexpr int ec_gpio = 10;
constexpr std::size_t gpio_count = 11; // GPIO-indexed fake ADC1 inputs, 0..10.
constexpr float invalid_float = std::numeric_limits<float>::quiet_NaN();
std::array<hal::adc::Sample, gpio_count> inputs{};
std::array<bool, gpio_count> configured{};
std::array<unsigned, gpio_count> reads{};
int fail_configure = -1;
int fail_read = -1;
unsigned configure_calls = 0;
std::vector<std::string> messages;

// Fail the test immediately when a required condition is false.
void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

// Check a converted value against the expected numerical tolerance.
void near(float value, float expected)
{
    require(std::isfinite(value) && std::fabs(value - expected) < 0.002f, "unexpected converted value");
}

// Set the simulated pH board voltage.
void ph_voltage(float mv)
{
    inputs[ph_gpio] = {2000, mv, false};
}

// Set simulated EC pin voltage using the configured divider gain.
void ec_voltage(float board_mv)
{
    inputs[ec_gpio] = {1000, board_mv / 1.5f, false};
}

// Check whether captured serial output contains the requested text.
bool logged(const char* text)
{
    for (const auto& message : messages) {
        if (message.find(text) != std::string::npos) return true;
    }
    return false;
}
} // namespace

namespace hal::adc {
// Simulate ADC initialization and optional channel failure.
bool configure(int gpio)
{
    ++configure_calls;
    require(gpio == ph_gpio || gpio == ec_gpio, "unexpected ADC GPIO");
    if (gpio == fail_configure) return false;
    configured[gpio] = true;
    return true;
}

// Return a configured ADC sample or an injected read failure.
bool read(int gpio, Sample& sample)
{
    require(gpio == ph_gpio || gpio == ec_gpio, "unexpected read GPIO");
    require(configured[gpio], "must configure input before reading");
    ++reads[gpio];
    if (gpio == fail_read) {
        sample = {};
        return false;
    }
    sample = inputs[gpio];
    return true;
}
} // namespace hal::adc

namespace drivers::serial {
// Capture serial messages for output assertions.
void write_line(const char* message)
{
    messages.emplace_back(message);
}
} // namespace drivers::serial

// Run the requested test scenario and return its result.
int main(int argc, char** argv)
{
    require(argc == 2, "provide a scenario");
    const std::string scenario = argv[1];
    auto snapshot = sensors::sample();
    require(snapshot.ph.status == Status::NotInitialized && snapshot.ec.status == Status::NotInitialized,
            "read before init must be unavailable");
    require(std::isnan(snapshot.ph.value) && std::isnan(snapshot.ec.value), "no invented startup measurements");
    require(!sensors::set_ec_temperature(25), "temperature setter requires EC init");
    ph_voltage(1500);
    ec_voltage(1.413f * 164);

    if (scenario == "partial_init") {
        fail_configure = ph_gpio;
        require(!sensors::init(), "report partial init failure");
        snapshot = sensors::sample();
        require(snapshot.ph.status == Status::NotInitialized, "pH init failure is visible");
        near(snapshot.ec.value, 1.413f);
        require(reads[ph_gpio] == 0 && reads[ec_gpio] == 1, "EC must still be sampled");
        fail_configure = ec_gpio;
        require(!sensors::init(), "report EC init failure");
        snapshot = sensors::sample();
        near(snapshot.ph.value, 7);
        require(snapshot.ec.status == Status::NotInitialized, "EC init failure is visible");
        return 0;
    }

    require(sensors::init(), "default sensor init");
    snapshot = sensors::sample();
    require(snapshot.ph.status == Status::NeedsCalibration && snapshot.ec.status == Status::NeedsCalibration,
            "default coefficients must not claim calibration");

    if (scenario == "conversion") {
        near(snapshot.ph.value, 7);
        near(snapshot.ec.value, 1.413f);
        near(snapshot.ec.board_mv, snapshot.ec.pin_mv * 1.5f);
        near(snapshot.ec_temperature_c, 25);
        ph_voltage(2032.44f);
        ec_voltage(12.88f * 164);
        snapshot = sensors::sample();
        near(snapshot.ph.value, 4);
        near(snapshot.ec.value, 12.88f);
        ph_voltage((1500.0f + 2032.44f) / 2);
        near(sensors::sample().ph.value, 5.5f);
        return 0;
    }

    if (scenario == "calibration") {
        require(sensors::set_ph_calibration({1520, 2050, true}), "accept measured pH references");
        ph_voltage(1520);
        snapshot = sensors::sample();
        near(snapshot.ph.value, 7);
        require(snapshot.ph.status == Status::Ok, "measured pH calibration status");
        for (const auto cal : {system_config::PhCalibration{1520, 1520, true},
                               system_config::PhCalibration{invalid_float, 2050, true},
                               system_config::PhCalibration{2050, 1520, true}}) {
            require(!sensors::set_ph_calibration(cal), "reject invalid reference pair");
        }
        near(sensors::sample().ph.value, 7); // Failed updates preserve valid calibration.
        ph_voltage(2050);
        near(sensors::sample().ph.value, 4);
        require(sensors::set_ec_calibration({1.1f, 0.9f, true}), "accept EC factors");
        ec_voltage(164);
        snapshot = sensors::sample();
        near(snapshot.ec.value, 1.1f);
        require(snapshot.ec.status == Status::Ok, "measured EC calibration status");
        ec_voltage(3 * 164);
        near(sensors::sample().ec.value, 2.7f); // Switch to high factor.
        ec_voltage(2.4f * 164);
        near(sensors::sample().ec.value, 2.16f); // Retain high within hysteresis band.
        ec_voltage(2 * 164);
        near(sensors::sample().ec.value, 2.2f); // Switch back to low.
        require(!sensors::set_ec_calibration({0.5f, 1, true}), "reject K at lower bound");
        require(!sensors::set_ec_calibration({1, 1.5f, true}), "reject K at upper bound");
        require(!sensors::set_ec_calibration({invalid_float, 1, true}), "reject NaN K");
        near(sensors::sample().ec.value, 2.2f);
        return 0;
    }

    if (scenario == "range_stability") {
        require(sensors::set_ec_calibration({1.4f, 0.6f, true}), "accept supported EC factors");
        // A steady input must not toggle between K factors on successive reads.
        ec_voltage(2 * 164);
        for (unsigned i = 0; i < 10; ++i) near(sensors::sample().ec.value, 2.8f);
        ec_voltage(4 * 164);
        for (unsigned i = 0; i < 10; ++i) near(sensors::sample().ec.value, 2.4f);
        ec_voltage(2.4f * 164);
        for (unsigned i = 0; i < 10; ++i) near(sensors::sample().ec.value, 3.36f);
        return 0;
    }

    if (scenario == "temperature") {
        ec_voltage(1.413f * 164 * (1 + 0.0185f * 5));
        require(sensors::set_ec_temperature(30), "accept actual temperature");
        snapshot = sensors::sample();
        near(snapshot.ec.value, 1.413f);
        near(snapshot.ec_temperature_c, 30);
        for (float temperature : {-1.0f, 40.1f, invalid_float, std::numeric_limits<float>::infinity()}) {
            require(!sensors::set_ec_temperature(temperature), "reject invalid temperature");
        }
        near(sensors::sample().ec_temperature_c, 30);
        require(sensors::set_ec_temperature(0), "allow temperature lower bound");
        require(sensors::set_ec_temperature(40), "allow temperature upper bound");
        return 0;
    }

    if (scenario == "invalid_readings") {
        fail_read = ph_gpio;
        snapshot = sensors::sample();
        require(snapshot.ph.status == Status::AdcError && std::isnan(snapshot.ph.value), "no stale pH after ADC failure");
        near(snapshot.ec.value, 1.413f);
        fail_read = ec_gpio;
        snapshot = sensors::sample();
        near(snapshot.ph.value, 7);
        require(snapshot.ec.status == Status::AdcError && std::isnan(snapshot.ec.value), "no stale EC after ADC failure");
        fail_read = -1;
        for (float mv : {0.0f, 3001.0f, invalid_float, std::numeric_limits<float>::infinity()}) {
            ph_voltage(mv);
            snapshot = sensors::sample();
            require(snapshot.ph.status == Status::VoltageOutOfRange && std::isnan(snapshot.ph.value), "reject bad pH voltage");
        }
        ph_voltage(1500);
        inputs[ph_gpio].clipped = true;
        require(sensors::sample().ph.status == Status::VoltageOutOfRange, "reject clipped pH samples");
        ph_voltage(2990); // Valid board voltage, but conversion falls outside pH 0..14.
        require(sensors::sample().ph.status == Status::ValueOutOfRange, "reject impossible pH");
        for (float mv : {0.0f, 3401.0f, invalid_float}) {
            ec_voltage(mv);
            snapshot = sensors::sample();
            require(snapshot.ec.status == Status::VoltageOutOfRange && std::isnan(snapshot.ec.value), "reject bad EC voltage");
        }
        ec_voltage(164);
        inputs[ec_gpio].clipped = true;
        require(sensors::sample().ec.status == Status::VoltageOutOfRange, "reject clipped EC samples");
        ec_voltage(3300); // Physically plausible voltage, conversion beyond 20 mS/cm.
        require(sensors::sample().ec.status == Status::ValueOutOfRange, "reject EC outside sensor range");
        return 0;
    }

    if (scenario == "pin_conflict") {
        const auto previous_calls = configure_calls;
        auto config = system_config::default_sensor_config();
        require(config.turbidity_gpio == 11, "reserve ADC2 GPIO11 for turbidity");
        for (int pin : {-1, 1, 2, 4, 5, 6, 7, 8, 9, 10}) {
            config = system_config::default_sensor_config();
            config.turbidity_gpio = pin;
            require(!sensors::init(config), "reject turbidity conflicts before configuring ADC");
        }
        config = system_config::default_sensor_config();
        config.ph.gpio = config.turbidity_gpio;
        require(!sensors::init(config), "pH must not take the turbidity input");
        config = system_config::default_sensor_config();
        config.ec.gpio = config.turbidity_gpio;
        require(!sensors::init(config), "EC must not take the turbidity input");
        config = system_config::default_sensor_config();
        config.ec.gpio = config.ph.gpio;
        require(!sensors::init(config), "reject shared sensor pin");
        for (int pin : {4, 5, 6}) {
            config = system_config::default_sensor_config();
            config.ph.gpio = pin;
            require(!sensors::init(config), "reject pump pin for pH");
            config = system_config::default_sensor_config();
            config.ec.gpio = pin;
            require(!sensors::init(config), "reject pump pin for EC");
        }
        for (int pin : {7, 9, 1, 2}) {
            config = system_config::default_sensor_config();
            config.ph.gpio = pin;
            require(!sensors::init(config), "reject TFT pin for pH before touching ADC");
            config = system_config::default_sensor_config();
            config.ec.gpio = pin;
            require(!sensors::init(config), "reject TFT pin for EC before touching ADC");
        }
        auto display = system_config::default_display_config();
        display.dc = 8;
        config = system_config::default_sensor_config();
        config.ph.gpio = 8;
        require(!sensors::init(config, display), "honor custom display pin map");
        display.enabled = false;
        require(system_config::valid_pin_assignments(config, display), "disabled TFT reserves no pins");
        require(configure_calls == previous_calls, "pin conflicts must not touch hardware");
        snapshot = sensors::sample();
        require(snapshot.ph.status == Status::NotInitialized && snapshot.ec.status == Status::NotInitialized,
                "invalid reinit must not expose readings from old configuration");
        require(!sensors::set_ph_calibration({1500, 2032.44f, true}), "invalid init blocks calibration");
        return 0;
    }

    if (scenario == "invalid_output") {
        for (auto status : {Status::Ok, Status::NeedsCalibration}) {
            for (float value : {invalid_float, std::numeric_limits<float>::infinity(), -1.0f}) {
                snapshot.ph.status = status;
                snapshot.ph.value = value;
                require(!sensor_data::has_value(snapshot.ph), "invalid numeric value is unavailable to every consumer");
                messages.clear();
                app::telemetry::report_sensors(snapshot);
                require(logged("pH unavailable") && logged("sensor value outside range"), "serial rejects invalid value/status pair");
                require(!logged("pH=nan") && !logged("pH=inf") && !logged("pH=-1"), "no invalid number printed as calibrated");
            }
        }
        return 0;
    }

    require(scenario == "telemetry", "unknown scenario");
    app::telemetry::report_sensors(snapshot);
    require(logged("pH=7.000") && logged("1.413 mS/cm"), "report both readings with correct units");
    require(logged("manual T=25.00 C") && logged("estimate; needs calibration"), "expose assumptions in output");
    messages.clear();
    fail_read = ph_gpio;
    app::telemetry::report_sensors(sensors::sample());
    require(logged("pH unavailable") && logged("ADC read failed"), "display unavailable after failed read");
    require(!logged("pH=7.000"), "never log stale pH as a fresh reading");
    return 0;
}
