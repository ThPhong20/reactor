#include "application/hardware_test/session.h"
#include <cstring>
#include <cstdio>

namespace app::hardware_test {

// Require fresh measurements, including estimates, without fabricating readiness.
const char* Session::blocker(std::uint64_t now_ms) const
{
    if (!sensor_data::is_fresh(sample_, now_ms)) return "BLOCKED: waiting for fresh pH/EC monitoring data";
    if (!sensor_data::has_value(sample_.sensors.ph)) return "BLOCKED: pH reading invalid";
    if (!sensor_data::has_value(sample_.sensors.ec)) return "BLOCKED: EC reading invalid";
    if (require_turbidity_ && sensor_data::effective_status(sample_.sensors.turbidity) != sensor_data::Status::RawOnly)
        return "BLOCKED: turbidity voltage invalid";
    return nullptr;
}

// Prefer latched faults and running state over idle readiness messages.
const char* Session::status(std::uint64_t now_ms) const
{
    if (pumps_.running() || pumps_.faulted()) return pumps_.status();
    const char* reason = blocker(now_ms);
    return reason ? reason : pumps_.status();
}

// Supervise monitoring freshness independently of slow sensor/display work.
bool Session::tick(std::uint64_t now_ms)
{
    if (pumps_.running() && !pump_only_ && blocker(now_ms)) {
        pumps_.disable();
        return true;
    }
    return pumps_.tick(now_ms);
}

// Reject unsafe starts while keeping STOP, help, and status available.
const char* Session::command(const char* line, std::uint64_t now_ms)
{
    if (tick(now_ms)) return status(now_ms);
    if (!line) return "ERROR: missing command";
    const char* first = line;
    while (*first == ' ' || *first == '\t') ++first;
    if (!std::strcmp(first, "status")) return status(now_ms);
    if (!std::strcmp(first, "help"))
        return "run all SECONDS; run inlet|outlet1|outlet2 DUTY SECONDS; pump-test SECONDS (ignores sensors); stop; status; !";
    // Explicit supervised pump test: retain hardware faults and the 1..60 s limit.
    // Sensor readiness is bypassed for this run only; ordinary runs remain guarded.
    if (!std::strncmp(first, "pump-test ", 10)) {
        if (pumps_.running()) return "ERROR: stop the current test before starting another";
        char translated[96];
        const int length = std::snprintf(translated, sizeof(translated), "run all %s", first + 10);
        if (length < 0 || length >= static_cast<int>(sizeof(translated))) return "ERROR: command too long";
        const char* result = pumps_.command(translated, now_ms);
        pump_only_ = pumps_.running();
        return pump_only_ ? "RUNNING: timed PUMP-ONLY test; sensor checks bypassed" : result;
    }
    if (!std::strncmp(first, "run", 3) &&
        (first[3] == '\0' || first[3] == ' ' || first[3] == '\t')) {
        if (const char* reason = blocker(now_ms)) return reason;
    }
    const bool was_running = pumps_.running();
    const char* result = pumps_.command(line, now_ms);
    if (!was_running && pumps_.running()) pump_only_ = false;
    return result;
}
} // namespace app::hardware_test
