#include "application/hardware_test/session.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace {
unsigned writes = 0;
unsigned stops = 0;
// Count actuator requests without touching physical outputs.
bool set_duty(system_config::Pump, float) { ++writes; return true; }
// Record shutdown requests from monitoring supervision.
bool stop_all() { ++stops; return true; }
// Fail the scenario with a readable explanation.
void require(bool condition, const char* message)
{
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
// Provide a fresh, usable but uncalibrated monitoring snapshot.
sensor_data::Record reading()
{
    sensor_data::Record result;
    result.sequence = 1;
    result.sampled_at_ms = 100;
    result.sensors.ph.status = sensor_data::Status::NeedsCalibration;
    result.sensors.ph.value = 7;
    result.sensors.ec.status = sensor_data::Status::NeedsCalibration;
    result.sensors.ec.value = 1;
    return result;
}
}
// Exercise sensor supervision independently from USB and FreeRTOS scheduling.
int main(int argc, char** argv)
{
    require(argc == 2, "scenario required");
    const std::string scenario = argv[1];
    app::hardware_test::Session session({set_duty, stop_all}, true);
    auto record = reading();
    if (scenario == "pump_only") {
        for (const auto* bad : {"pump-test 0", "pump-test 61", "pump-test 5 extra", "pump-test nan"}) {
            session.command(bad, 100);
            require(writes == 0, "pump-only command must enforce duration syntax");
        }
        session.command("pump-test 5", 100);
        require(session.running() && writes == 3, "explicit pump test can run without sensors");
        session.tick(4000);
        require(session.running(), "missing sensors must not interrupt explicit pump test");
        session.command("pump-test 10", 4000);
        session.tick(5100);
        require(!session.running(), "busy command must not extend deadline");
        session.command("run all 5", 5200);
        require(!session.running() && writes == 3, "ordinary run must still require sensors");
        session.command("pump-test 5", 5300);
        session.command("stop", 5400);
        require(!session.running(), "STOP must interrupt pump-only run");
        session.disable();
        const auto previous_writes = writes;
        session.command("pump-test 5", 5500);
        require(!session.running() && writes == previous_writes, "pump-only command must not bypass faults");
    } else if (scenario == "missing") {
        session.command("run all 10", 100);
        require(writes == 0, "missing samples must block startup");
        session.command("stop", 100);
        require(stops == 1, "STOP must work without sensor data");
    } else if (scenario == "invalid_start") {
        record.sensors.ec.value = std::numeric_limits<float>::quiet_NaN();
        session.update(record);
        session.command("run all 10", 100);
        require(writes == 0, "invalid reported value must block startup");
    } else if (scenario == "future") {
        session.update(record);
        session.command("run all 10", 99);
        require(writes == 0, "future sample must block startup");
    } else {
        session.update(record);
        session.command("run all 10", 100);
        require(session.running() && writes == 3, "usable estimates should allow a water test");
        if (scenario == "fresh") {
            record.sampled_at_ms = 3000;
            session.update(record);
            session.tick(3101);
            require(session.running(), "fresh monitoring should keep test running");
            session.command("stop", 3102);
            require(!session.running(), "operator STOP must stop pumps");
            app::hardware_test::Session with_turbidity({set_duty, stop_all}, true, true);
            record.sampled_at_ms = 4000;
            with_turbidity.update(record);
            with_turbidity.command("run all 10", 4000);
            require(!with_turbidity.running(), "enabled turbidity must be available before startup");
            record.sensors.turbidity.status = sensor_data::Status::RawOnly;
            record.sensors.turbidity.pin_mv = 2000;
            record.sensors.turbidity.board_mv = 4000;
            with_turbidity.update(record);
            with_turbidity.command("run all 10", 4000);
            require(with_turbidity.running(), "valid raw turbidity permits a water test");
            record.sensors.turbidity.pin_mv = std::numeric_limits<float>::quiet_NaN();
            with_turbidity.update(record);
            with_turbidity.tick(4001);
            require(with_turbidity.faulted() && !with_turbidity.running(), "lost raw turbidity must stop pumps");
            return 0;
        }
        if (scenario == "invalid_running") {
            record.sensors.ph.status = sensor_data::Status::AdcError;
            session.update(record);
            session.tick(101);
        } else {
            require(scenario == "stale", "unknown scenario");
            session.tick(3100);
            require(session.running(), "freshness boundary should be inclusive");
            session.tick(3101);
        }
        require(!session.running() && session.faulted(), "lost monitoring must stop and latch fault");
        require(stops >= 2, "shutdown must reach actuator callback");
        record = reading();
        record.sampled_at_ms = 4000;
        session.update(record);
        session.command("stop", 4000);
        session.command("run all 10", 4000);
        require(writes == 3, "recovered samples and STOP must not clear fault");
    }
}
