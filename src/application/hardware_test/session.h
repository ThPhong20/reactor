#pragma once
#include "application/hardware_test/controller.h"
#include "common/sensor_data.h"

namespace app::hardware_test {

// Integrate timed pump commands with fresh pH/EC data for supervised water tests.
// Estimates are allowed here; this does not relax the production reactor FSM.
class Session {
public:
    // Start idle with no sensor data and never energize outputs automatically.
    Session(Outputs outputs, bool ready, bool require_turbidity = false)
        : pumps_(outputs, ready), require_turbidity_(require_turbidity) {}
    // Copy the latest monitoring record; only the control task calls this API.
    void update(const sensor_data::Record& record) { sample_ = record; }
    // Stop on timeout or lost monitoring readiness; report a stop attempt.
    bool tick(std::uint64_t now_ms);
    // Handle commands with sensor readiness checks before any pump startup.
    const char* command(const char* line, std::uint64_t now_ms);
    // Attempt shutdown without clearing any latched hardware/sensor fault.
    const char* stop() { return pumps_.stop(); }
    // Disable operation after a control-task or hardware failure.
    const char* disable() { return pumps_.disable(); }
    // Return the logical pump state for the monitoring task.
    bool running() const { return pumps_.running(); }
    // Return whether further pump starts are blocked until reboot.
    bool faulted() const { return pumps_.faulted(); }
    // Describe current pump state or why an idle test cannot start.
    const char* status(std::uint64_t now_ms) const;

private:
    // Require fresh usable pH/EC; optionally require the enabled turbidity voltage channel.
    const char* blocker(std::uint64_t now_ms) const;
    Controller pumps_;
    bool require_turbidity_;
    bool pump_only_ = false; // Applies only to an explicitly requested timed pump test.
    sensor_data::Record sample_{};
};
} // namespace app::hardware_test
