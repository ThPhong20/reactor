#pragma once

#include "common/system_config.h"
#include <cstdint>

namespace app::hardware_test {

struct Outputs {
    bool (*set_duty)(system_config::Pump, float);
    bool (*stop_all)();
};

// Supervised bench controller; one task owns commands, time checks, and outputs.
class Controller {
public:
    // Hardware must already be initialized at zero duty; never starts outputs.
    Controller(Outputs outputs, bool ready) : outputs_(outputs), fault_(!ready) {}
    // Process a complete command. Returned messages have static storage.
    const char* command(const char* line, std::uint64_t now_ms);
    // Stop at the deadline (or clock rollback); true means a stop was attempted.
    bool tick(std::uint64_t now_ms);
    // Stop every pump; failed hardware actions stay latched until reboot.
    const char* stop();
    // Latch an external bench failure and attempt shutdown.
    const char* disable();
    // Report whether a timed test is active.
    bool running() const { return running_; }
    // Expose the fault latch without reading hardware from another task.
    bool faulted() const { return fault_; }
    // Report logical status, not measured motor motion.
    const char* status() const;

private:
    Outputs outputs_;
    bool fault_ = false;
    bool running_ = false;
    std::uint64_t started_ms_ = 0;
    std::uint64_t duration_ms_ = 0;
};
} // namespace app::hardware_test
