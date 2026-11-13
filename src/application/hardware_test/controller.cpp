#include "application/hardware_test/controller.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace app::hardware_test {
namespace {
constexpr const char* usage = "run inlet|outlet1|outlet2 DUTY SECONDS; run all SECONDS (saved duties); stop; status; !";

// Split a small command in place; reject extra tokens without heap allocation.
int split(char* text, char* (&tokens)[5])
{
    int count = 0;
    while (*text) {
        while (*text == ' ' || *text == '\t') ++text;
        if (!*text) break;
        if (count == 5) return -1;
        tokens[count++] = text;
        while (*text && *text != ' ' && *text != '\t') ++text;
        if (*text) *text++ = '\0';
    }
    return count;
}

// Accept a finite, strictly positive duty up to 100 percent.
bool duty_value(const char* text, float& value)
{
    char* end = nullptr;
    value = std::strtof(text, &end);
    return end != text && *end == '\0' && std::isfinite(value) && value > 0 && value <= 100;
}

// Accept only whole seconds 1..60; avoid signed and overflowing input.
bool seconds_value(const char* text, unsigned& value)
{
    value = 0;
    if (!*text) return false;
    for (; *text; ++text) {
        if (*text < '0' || *text > '9') return false;
        value = value * 10 + static_cast<unsigned>(*text - '0');
        if (value > 60) return false;
    }
    return value > 0;
}
} // namespace

// Report logical status, not measured motor motion.
const char* Controller::status() const
{
    if (fault_) return "FAULT: commands disabled; disconnect motor power and inspect before reboot";
    return running_ ? "RUNNING: timed test active" : "IDLE: outputs commanded off";
}

// Stop every pump even after a fault; never clear a hardware fault by retrying.
const char* Controller::stop()
{
    running_ = false;
    if (!outputs_.stop_all || !outputs_.stop_all()) fault_ = true;
    return status();
}

// Latch an external bench failure and attempt shutdown.
const char* Controller::disable()
{
    fault_ = true;
    return stop();
}

// Apply the time limit without relying on serial traffic or display updates.
bool Controller::tick(std::uint64_t now_ms)
{
    if (!running_) return false;
    if (now_ms < started_ms_ || now_ms - started_ms_ >= duration_ms_) {
        if (now_ms < started_ms_) fault_ = true;
        stop();
        return true;
    }
    return false;
}

// Validate the entire command before touching outputs; never extend a live run.
const char* Controller::command(const char* line, std::uint64_t now_ms)
{
    // A command arriving at an expired deadline cannot start another test.
    if (tick(now_ms)) return status();
    if (!line || std::strlen(line) >= 96) return "ERROR: command too long";
    char text[96];
    std::strcpy(text, line);
    char* tokens[5]{};
    const int count = split(text, tokens);
    if (count == 1) {
        if (!std::strcmp(tokens[0], "stop") || !std::strcmp(tokens[0], "!")) return stop();
        if (!std::strcmp(tokens[0], "status")) return status();
        if (!std::strcmp(tokens[0], "help")) return usage;
    }
    if (count < 3 || count > 4 || std::strcmp(tokens[0], "run")) return usage;
    if (fault_ || !outputs_.set_duty || !outputs_.stop_all) return "FAULT: pump control unavailable";
    if (running_) return "ERROR: stop the current test before starting another";

    auto pumps = system_config::default_pump_config();
    const bool all = !std::strcmp(tokens[1], "all");
    unsigned seconds = 0;
    std::size_t selected = pumps.size();
    float duty = 0;
    if (all) {
        if (count != 3 || !seconds_value(tokens[2], seconds)) return "ERROR: duration must be 1..60 whole seconds";
        for (const auto& pump : pumps) {
            if (!std::isfinite(pump.duty_percent) || pump.duty_percent <= 0 || pump.duty_percent > 100)
                return "ERROR: invalid saved pump duties";
        }
    } else {
        if (!std::strcmp(tokens[1], "inlet")) selected = 0;
        else if (!std::strcmp(tokens[1], "outlet1")) selected = 1;
        else if (!std::strcmp(tokens[1], "outlet2")) selected = 2;
        if (selected == pumps.size() || count != 4) return usage;
        if (!duty_value(tokens[2], duty)) return "ERROR: duty must be greater than 0 and at most 100";
        if (!seconds_value(tokens[3], seconds)) return "ERROR: duration must be 1..60 whole seconds";
    }
    stop(); // Confirm all channels are off before starting a fresh test.
    if (fault_) return status();
    started_ms_ = now_ms;
    duration_ms_ = seconds * 1000ULL;
    for (std::size_t i = 0; i < pumps.size(); ++i) {
        if (!all && i != selected) continue;
        if (!outputs_.set_duty(static_cast<system_config::Pump>(i), all ? pumps[i].duty_percent : duty)) {
            fault_ = true;
            return stop(); // A failed write may have partially started a motor.
        }
    }
    running_ = true;
    return status();
}
} // namespace app::hardware_test
