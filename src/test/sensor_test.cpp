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
constexpr float invalid_float = std::numeric_limits<float>::quiet_NaN();
std::array<hal::adc::Sample, 3> inputs{};
std::array<bool, 3> configured{};
std::array<unsigned, 3> reads{};
int fail_configure = -1;
int fail_read = -1;
unsigned configure_calls = 0;
std::vector<std::string> messages;

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

void near(float value, float expected)
{
    require(std::isfinite(value) && std::fabs(value - expected) < 0.002f, "unexpected converted value");
}

void ph_voltage(float mv)
{
    inputs[1] = {2000, mv, false};
}

void ec_voltage(float board_mv)
{
    inputs[2] = {1000, board_mv / 1.5f, false};
}

bool logged(const char* text)
{
    for (const auto& message : messages) {
        if (message.find(text) != std::string::npos) return true;
    }
    return false;
}
} // namespace

namespace hal::adc {
bool configure(int gpio)
{
    ++configure_calls;
    require(gpio == 1 || gpio == 2, "unexpected ADC GPIO");
    if (gpio == fail_configure) return false;
    configured[gpio] = true;
    return true;
}

bool read(int gpio, Sample& sample)
{
    require(gpio == 1 || gpio == 2, "unexpected read GPIO");
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
void write_line(const char* message)
{
    messages.emplace_back(message);
}
} // namespace drivers::serial

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
        fail_configure = 1;
        require(!sensors::init(), "report partial init failure");
        snapshot = sensors::sample();
        require(snapshot.ph.status == Status::NotInitialized, "pH init failure is visible");
        near(snapshot.ec.value, 1.413f);
        require(reads[1] == 0 && reads[2] == 1, "EC must still be sampled");
        fail_configure = 2;
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
        fail_read = 1;
        snapshot = sensors::sample();
        require(snapshot.ph.status == Status::AdcError && std::isnan(snapshot.ph.value), "no stale pH after ADC failure");
        near(snapshot.ec.value, 1.413f);
        fail_read = 2;
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
        inputs[1].clipped = true;
        require(sensors::sample().ph.status == Status::VoltageOutOfRange, "reject clipped pH samples");
        ph_voltage(2990); // Valid board voltage, but conversion falls outside pH 0..14.
        require(sensors::sample().ph.status == Status::ValueOutOfRange, "reject impossible pH");
        for (float mv : {0.0f, 3401.0f, invalid_float}) {
            ec_voltage(mv);
            snapshot = sensors::sample();
            require(snapshot.ec.status == Status::VoltageOutOfRange && std::isnan(snapshot.ec.value), "reject bad EC voltage");
        }
        ec_voltage(164);
        inputs[2].clipped = true;
        require(sensors::sample().ec.status == Status::VoltageOutOfRange, "reject clipped EC samples");
        ec_voltage(3300); // Physically plausible voltage, conversion beyond 20 mS/cm.
        require(sensors::sample().ec.status == Status::ValueOutOfRange, "reject EC outside sensor range");
        return 0;
    }

    if (scenario == "pin_conflict") {
        const auto previous_calls = configure_calls;
        auto config = system_config::default_sensor_config();
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
        for (int pin : {9, 10, 11, 12}) {
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
    fail_read = 1;
    app::telemetry::report_sensors(sensors::sample());
    require(logged("pH unavailable") && logged("ADC read failed"), "display unavailable after failed read");
    require(!logged("pH=7.000"), "never log stale pH as a fresh reading");
    return 0;
}
