#include "application/pump_manager/pump_manager.h"
#include "driver/pump_pwm/pump_pwm.h"
#include "hal/pwm/pwm.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace {
namespace pumps = app::pump_manager;
using pumps::Pump;
std::array<float, 3> outputs{};
std::array<int, 3> pins{{-1, -1, -1}};
std::array<unsigned, 3> stops{};
unsigned writes = 0;
unsigned configured = 0;
int fail_configure = -1;
int fail_write = -1;
int fail_stop = -1;
bool fail_timer = false;

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

void set_operating_duties()
{
    // Arbitrary test duties, not pump flow calibration values.
    require(pumps::set_operating_duty(Pump::Inlet, 50.0f), "set inlet duty");
    require(pumps::set_operating_duty(Pump::Outlet1, 40.0f), "set outlet 1 duty");
    require(pumps::set_operating_duty(Pump::Outlet2, 30.0f), "set outlet 2 duty");
}

void require_stopped()
{
    require(outputs == std::array<float, 3>{0, 0, 0}, "all outputs must be off");
}
} // namespace

// Fake hardware boundary: test the real manager and driver without running motors.
namespace hal::pwm {
bool init(std::uint32_t frequency_hz)
{
    require(frequency_hz == 10000, "use measured calibration PWM frequency");
    return !fail_timer;
}

bool configure_channel(unsigned channel, int gpio)
{
    require(channel < 3, "unexpected channel");
    if (static_cast<int>(channel) == fail_configure) {
        return false;
    }
    pins[channel] = gpio;
    outputs[channel] = 0;
    ++configured;
    return true;
}

bool set_duty(unsigned channel, float percent)
{
    require(channel < configured, "write to unconfigured channel");
    ++writes;
    // Model a partially applied write before an SDK error is reported.
    outputs[channel] = percent;
    return static_cast<int>(channel) != fail_write;
}

bool stop(unsigned channel)
{
    require(channel < configured, "stop of unconfigured channel");
    ++stops[channel];
    if (static_cast<int>(channel) == fail_stop) {
        return false;
    }
    outputs[channel] = 0;
    return true;
}
} // namespace hal::pwm

int main(int argc, char** argv)
{
    require(argc == 2, "provide a scenario");
    const std::string scenario = argv[1];
    require(!pumps::start_all(), "must not start before initialization");
    require(!pumps::set_manual_duty(Pump::Inlet, 20), "manual control requires initialization");

    if (scenario == "duplicate_pins") {
        auto config = system_config::default_pump_config();
        config[1].gpio = config[0].gpio;
        require(!drivers::pump_pwm::init(config), "duplicate pins must fail");
        require(configured == 0, "invalid pin map must not configure channels");
        return 0;
    }
    if (scenario == "timer_failure" || scenario.find("init_failure_") == 0) {
        fail_timer = scenario == "timer_failure";
        fail_configure = fail_timer ? -1 : scenario.back() - '0';
        require(!pumps::init(), "initialization failure must be reported");
        require(!pumps::start_all(), "failed initialization must block start");
        require(!pumps::set_manual_duty(Pump::Inlet, 20), "failed initialization must block manual output");
        for (unsigned i = 0; i < configured; ++i) {
            require(stops[i] > 0, "partial initialization must stop configured channels");
        }
        require_stopped();
        require(!pumps::init(), "failed hardware initialization requires reboot");
        return 0;
    }

    require(pumps::init(), "initialization must succeed");
    require(pins == std::array<int, 3>{4, 5, 6}, "pump GPIO mapping");
    require_stopped();
    const auto config = system_config::default_pump_config();
    require(config[0].target_flow_ml_min == 70 && config[1].target_flow_ml_min == 49 &&
            config[2].target_flow_ml_min == 21, "flow target mapping");
    require(config[0].duty_percent == 90 && config[1].duty_percent == 0 &&
            config[2].duty_percent == 0, "store measured inlet duty; leave outlets uncalibrated");
    require(!pumps::start_all() && writes == 0, "unconfigured duties must not start any pump");
    set_operating_duties();
    require_stopped();

    if (scenario.find("write_failure_") == 0) {
        fail_write = scenario.back() - '0';
        require(!pumps::start_all(), "write failure must be reported");
        require_stopped();
        for (auto attempts : stops) {
            require(attempts > 0, "write failure must stop every pump");
        }
        fail_write = -1;
        require(!pumps::start_all(), "hardware fault must block later starts");
        require(!pumps::init(), "reinit must not clear hardware fault");
        return 0;
    }

    require(pumps::start_all(), "configured start must succeed");
    require(outputs == std::array<float, 3>{50, 40, 30}, "independent operating duties");
    if (scenario == "stop_failure") {
        fail_stop = 1;
        require(!pumps::stop_all(), "stop failure must be reported");
        require(outputs[0] == 0 && outputs[2] == 0, "stop must continue after a failed channel");
        require(!pumps::start_all(), "stop failure must disable starts");
        fail_stop = -1;
        require(pumps::stop_all(), "stop may be retried after failure");
        require_stopped();
        return 0;
    }
    if (scenario == "reinit") {
        require(pumps::init(), "reinitialization must stop existing outputs");
        require_stopped();
        require(configured == 3, "reinit must not allocate channels again");
        require(!pumps::start_all(), "uncalibrated outlets must still block start after reinit");
        require(pumps::set_operating_duty(Pump::Outlet1, 40), "set outlet 1 test duty");
        require(pumps::set_operating_duty(Pump::Outlet2, 30), "set outlet 2 test duty");
        require(pumps::start_all(), "start after setting outlet duties");
        require(outputs == std::array<float, 3>{90, 40, 30}, "apply measured inlet duty from defaults");
        require(pumps::stop_all(), "stop after checking calibrated inlet default");
        require_stopped();
        return 0;
    }

    require(scenario == "healthy", "unknown scenario");
    const auto previous_outputs = outputs;
    for (float duty : {-1.0f, 100.1f, std::numeric_limits<float>::infinity(),
                       std::numeric_limits<float>::quiet_NaN()}) {
        require(!pumps::set_manual_duty(Pump::Inlet, duty), "reject invalid manual duty");
        require(!pumps::set_operating_duty(Pump::Inlet, duty), "reject invalid operating duty");
    }
    require(!pumps::set_manual_duty(static_cast<Pump>(3), 50), "reject invalid pump");
    require(!pumps::set_operating_duty(static_cast<Pump>(-1), 50), "reject invalid pump setting");
    require(outputs == previous_outputs, "invalid commands must not change outputs");
    require(pumps::set_manual_duty(Pump::Outlet1, 100), "allow full duty");
    require(outputs == std::array<float, 3>{50, 100, 30}, "manual adjustment affects one pump");
    require(pumps::set_manual_duty(Pump::Outlet1, 0), "zero duty stops one pump");
    require(outputs == std::array<float, 3>{50, 0, 30}, "manual stop affects one pump");
    require(pumps::stop_all(), "stop all pumps");
    require_stopped();
    require(pumps::set_operating_duty(Pump::Outlet2, 0), "allow clearing stored duty");
    const auto previous_writes = writes;
    require(!pumps::start_all() && writes == previous_writes, "preflight all duties before starting");
    require(pumps::set_operating_duty(Pump::Outlet2, 30), "restore stored duty");
    require(pumps::start_all(), "restart after a normal stop");
    require(outputs == std::array<float, 3>{50, 40, 30}, "manual tuning must not overwrite operating duties");
    require(pumps::stop_all(), "final stop");
    require_stopped();
    return 0;

}
