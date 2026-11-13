#include "application/hardware_test/controller.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
using system_config::Pump;
std::array<float, 3> duties{};
unsigned writes = 0;
unsigned stops = 0;
int failed_channel = -1;
bool failed_stop = false;

// Abort on a failed expected behavior with a useful diagnostic.
void require(bool condition, const char* message)
{
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}

// Model a write that can change the output before reporting failure.
bool set_duty(Pump pump, float duty)
{
    const auto channel = static_cast<std::size_t>(pump);
    duties[channel] = duty;
    ++writes;
    return static_cast<int>(channel) != failed_channel;
}

// Model all-pump shutdown, including failure to confirm it.
bool stop_all()
{
    ++stops;
    if (failed_stop) return false;
    duties.fill(0);
    return true;
}

// Check that no simulated motor is commanded on.
void off()
{
    require(duties == std::array<float, 3>{0, 0, 0}, "all pumps must be off");
}
} // namespace

// Exercise one independent timed-bench scenario per process.
int main(int argc, char** argv)
{
    require(argc == 2, "provide scenario");
    const std::string scenario = argv[1];
    app::hardware_test::Controller bench({set_duty, stop_all}, scenario != "unavailable");
    require(!bench.running() && writes == 0, "construction must not start pumps");
    if (scenario == "unavailable") {
        bench.command("run all 10", 0);
        require(writes == 0, "failed setup blocks all starts");
        bench.command("stop", 0);
        require(stops == 1, "STOP still attempts shutdown after failed setup");
        bench.command("run inlet 90 10", 1);
        require(writes == 0, "STOP does not clear a fault");
        return 0;
    }
    if (scenario == "invalid") {
        for (const auto* line : {"run", "run inlet", "run all", "run all 0", "run all 61",
                "run all -1", "run all 99999999999999999999", "run all 1.5", "run all 10 extra",
                "run inlet 0 10", "run inlet -1 10", "run inlet 100.1 10", "run inlet nan 10",
                "run inlet inf 10", "run inlet 90x 10", "run inlet 90 0", "run inlet 90 61",
                "run outlet3 90 10", "run inlet 90 10 extra", "stop extra"}) {
            bench.command(line, 1000);
            require(writes == 0 && !bench.running(), "invalid command must not start a pump");
        }
        bench.command(std::string(96, 'x').c_str(), 1000);
        require(writes == 0, "reject oversized command");
        off(); return 0;
    }
    if (scenario == "single") {
        const char* commands[] = {"run inlet 90 1", "run outlet1 65 1", "run outlet2 42 1"};
        const float expected[] = {90, 65, 42};
        for (unsigned i = 0; i < 3; ++i) {
            bench.command(commands[i], 1000);
            require(bench.running() && duties[i] == expected[i], "select the correct pump and duty");
            for (unsigned j = 0; j < 3; ++j) if (j != i) require(duties[j] == 0, "other pumps stay off");
            require(!bench.tick(1999), "do not stop before deadline");
            require(bench.tick(2000) && !bench.running(), "stop exactly at deadline");
            off();
        }
        return 0;
    }
    if (scenario == "write_failure") failed_channel = 1;
    if (scenario == "preflight_stop_failure") failed_stop = true;
    bench.command("run all 60", 1000);
    if (scenario == "preflight_stop_failure" || scenario == "write_failure") {
        require(!bench.running(), "failed start must not be reported running");
        if (failed_stop) require(writes == 0, "must confirm shutdown before startup");
        else off();
        const auto old_writes = writes;
        failed_channel = -1;
        failed_stop = false;
        bench.command("stop", 1100);
        bench.command("run all 1", 1200);
        require(writes == old_writes, "fault stays latched after retrying STOP");
        return 0;
    }
    require(bench.running() && duties == std::array<float, 3>{90, 65, 42}, "apply saved duties for all pumps");
    if (scenario == "busy") {
        bench.command("run inlet 50 60", 30000);
        require(writes == 3 && bench.running(), "reject running changes without extending deadline");
        bench.command("status", 40000);
        require(bench.tick(61000), "original deadline remains effective");
        off(); return 0;
    }
    if (scenario == "rollback") {
        require(bench.tick(999) && !bench.running(), "clock rollback must stop");
        bench.command("run all 10", 1000);
        require(writes == 3, "clock failure stays latched");
        off(); return 0;
    }
    if (scenario == "stop_failure") {
        failed_stop = true;
        bench.command("stop", 2000);
        require(std::string(bench.status()).find("FAULT") == 0, "failed shutdown must be visible");
        failed_stop = false;
        bench.command("!", 3000);
        off();
        bench.command("run all 1", 4000);
        require(writes == 3, "failed shutdown requires reboot");
        return 0;
    }
    if (scenario == "external_failure") {
        bench.disable();
        off();
        bench.command("run all 1", 2000);
        require(writes == 3, "external failure blocks restart");
        return 0;
    }
    if (scenario == "deadline_command") {
        bench.command("run all 1", 61000);
        require(writes == 3 && !bench.running(), "a late command cannot restart after timeout");
        off(); return 0;
    }
    require(scenario == "stop", "unknown scenario");
    bench.command("stop", 2000);
    off();
    bench.command("run outlet2 42 1", 3000);
    require(bench.running(), "explicit restart allowed after normal stop");
    bench.command("!", 3100);
    off();
    return 0;
}
