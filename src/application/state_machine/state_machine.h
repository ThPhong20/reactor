#pragma once

#include "common/sensor_data.h"

// Single-task FSM. Hardware actions are injected to permit host simulation.
namespace app::state_machine {

enum class State { Init, Idle, Prime, Running, Stopping, Fault };
enum class Event { Tick, Start, Stop, FlowConfirmed, Reset, FaultDetected };
enum class Reason {
    None, PumpUnavailable, PumpDutiesMissing, FlowCalibrationMissing,
    SensorUnavailable, SensorUncalibrated, StaleSensors, InterlocksUnavailable,
    InterlockOpen, PowerUnavailable, PrimeTimeout, PumpStartFailed,
    PowerStartFailed, ShutdownFailed, ExternalFault
};

struct Inputs {
    sensor_data::Record sample;
    std::uint64_t now_ms = 0;
    bool pumps_ready = false;
    bool duties_ready = false;
    bool flows_verified = false;
    bool interlocks_available = false;
    bool interlocks_ok = false;
    bool external_fault = false;
};

// A missing power backend blocks START; it never pretends to control a PSU.
struct Outputs {
    bool (*start_pumps)() = nullptr;
    bool (*stop_pumps)() = nullptr;
    bool (*set_power)(bool enabled) = nullptr;
};

class Machine {
public:
    // Create an INIT-state controller with the supplied output callbacks.
    explicit Machine(Outputs outputs) : outputs_(outputs) {}
    static constexpr std::uint64_t prime_timeout_ms = 60000;
    static constexpr std::uint64_t max_sample_age_ms = sensor_data::max_sample_age_ms;
    // Valid only in INIT; never starts outputs.
    bool initialize(bool pumps_ready);
    // Synchronous, single-task API. STOP attempts shutdown even in FAULT.
    bool handle(Event event, const Inputs& inputs);
    // Return the current FSM state without changing it.
    State state() const { return state_; }
    // Return the current blocked-start or fault reason.
    Reason reason() const { return reason_; }
    // Return the first failed readiness check, or None when ready.
    Reason start_blocker(const Inputs& inputs) const;

private:
    // Request power off, then attempt pump shutdown even if power-off fails.
    bool stop_outputs();
    // Latch a fault and attempt to shut down both outputs.
    void fault(Reason reason);
    Outputs outputs_;
    State state_ = State::Init;
    Reason reason_ = Reason::None;
    std::uint64_t prime_started_ms_ = 0;
};

// Return the display label for a reactor state.
const char* name(State state);
// Return a short operator message for a blocker or fault.
const char* description(Reason reason);
} // namespace app::state_machine
