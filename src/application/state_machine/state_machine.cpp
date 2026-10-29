#include "application/state_machine/state_machine.h"

namespace app::state_machine {

// Return the display label for a reactor state.
const char* name(State state)
{
    switch (state) {
    case State::Init: return "INIT";
    case State::Idle: return "IDLE";
    case State::Prime: return "PRIME";
    case State::Running: return "RUNNING";
    case State::Stopping: return "STOPPING";
    case State::Fault: return "FAULT";
    }
    return "UNKNOWN";
}

// Return a short operator message for a blocker or fault.
const char* description(Reason reason)
{
    switch (reason) {
    case Reason::None: return "READY";
    case Reason::PumpUnavailable: return "PUMPS UNAVAILABLE";
    case Reason::PumpDutiesMissing: return "SET PUMP DUTIES";
    case Reason::FlowCalibrationMissing: return "VERIFY PUMP FLOWS";
    case Reason::SensorUnavailable: return "SENSOR UNAVAILABLE";
    case Reason::SensorUncalibrated: return "CALIBRATE SENSORS";
    case Reason::StaleSensors: return "SENSORS STALE";
    case Reason::InterlocksUnavailable: return "INTERLOCKS MISSING";
    case Reason::InterlockOpen: return "INTERLOCK OPEN";
    case Reason::PowerUnavailable: return "POWER CTRL MISSING";
    case Reason::PrimeTimeout: return "PRIME TIMEOUT";
    case Reason::PumpStartFailed: return "PUMP START FAILED";
    case Reason::PowerStartFailed: return "POWER START FAILED";
    case Reason::ShutdownFailed: return "SHUTDOWN FAILED";
    case Reason::ExternalFault: return "EXTERNAL FAULT";
    }
    return "UNKNOWN FAULT";
}

// Return the first failed readiness check, or None when ready.
Reason Machine::start_blocker(const Inputs& in) const
{
    if (in.external_fault) return Reason::ExternalFault;
    if (!in.pumps_ready || !outputs_.start_pumps || !outputs_.stop_pumps) return Reason::PumpUnavailable;
    if (!in.duties_ready) return Reason::PumpDutiesMissing;
    if (!in.flows_verified) return Reason::FlowCalibrationMissing;
    if (!in.sample.sequence || in.now_ms < in.sample.sampled_at_ms ||
        in.now_ms - in.sample.sampled_at_ms > max_sample_age_ms) return Reason::StaleSensors;
    const sensor_data::Reading* readings[] = {&in.sample.sensors.ph, &in.sample.sensors.ec,
                                            &in.sample.sensors.turbidity};
    for (const auto* reading : readings) {
        if (!sensor_data::has_value(*reading)) return Reason::SensorUnavailable;
    }
    for (const auto* reading : readings) {
        if (sensor_data::effective_status(*reading) != sensor_data::Status::Ok)
            return Reason::SensorUncalibrated;
    }
    if (!in.interlocks_available) return Reason::InterlocksUnavailable;
    if (!in.interlocks_ok) return Reason::InterlockOpen;
    if (!outputs_.set_power) return Reason::PowerUnavailable;
    return Reason::None;
}

// Request power off, then attempt pump shutdown even if power-off fails.
bool Machine::stop_outputs()
{
    // Do not short-circuit: pump shutdown is attempted even if power-off fails.
    // An absent power backend cannot have been enabled by this FSM.
    const bool power_off = !outputs_.set_power || outputs_.set_power(false);
    const bool pumps_off = outputs_.stop_pumps && outputs_.stop_pumps();
    return power_off && pumps_off;
}

// Latch a fault and attempt to shut down both outputs.
void Machine::fault(Reason reason)
{
    state_ = State::Fault;
    reason_ = reason;
    if (!stop_outputs()) reason_ = Reason::ShutdownFailed;
}

// Confirm shutdown and enter IDLE, or latch an initialization fault.
bool Machine::initialize(bool pumps_ready)
{
    if (state_ != State::Init) return false;
    if (!stop_outputs()) {
        state_ = State::Fault;
        reason_ = Reason::ShutdownFailed;
        return false;
    }
    state_ = pumps_ready ? State::Idle : State::Fault;
    reason_ = pumps_ready ? Reason::None : Reason::PumpUnavailable;
    return pumps_ready;
}

// Apply an event with readiness guards, shutdown, and fault handling.
bool Machine::handle(Event event, const Inputs& in)
{
    if (event == Event::Stop) {
        const auto previous_state = state_;
        const bool was_fault = state_ == State::Fault;
        state_ = State::Stopping;
        if (!stop_outputs()) {
            state_ = State::Fault;
            reason_ = Reason::ShutdownFailed;
            return false;
        }
        state_ = State::Idle;
        if (was_fault) state_ = State::Fault;
        else if (previous_state == State::Init) state_ = State::Init;
        if (!was_fault) reason_ = Reason::None;
        if (in.external_fault && !was_fault) {
            state_ = State::Fault;
            reason_ = Reason::ExternalFault;
        }
        return true;
    }
    if (state_ == State::Fault) {
        // Faults stay latched. RESET requires restored readiness, then INIT.
        if (event != Event::Reset || start_blocker(in) != Reason::None) return false;
        if (!stop_outputs()) {
            reason_ = Reason::ShutdownFailed;
            return false;
        }
        state_ = State::Init;
        reason_ = Reason::None;
        return true;
    }
    if (event == Event::FaultDetected || in.external_fault) {
        fault(Reason::ExternalFault);
        return true;
    }
    if (state_ == State::Init) return false;
    if (state_ == State::Idle) {
        reason_ = start_blocker(in);
        if (event == Event::Tick) return true;
        if (event != Event::Start || reason_ != Reason::None) return false;
        if (!outputs_.set_power(false)) {
            fault(Reason::ShutdownFailed);
            return false;
        }
        if (!outputs_.start_pumps()) {
            fault(Reason::PumpStartFailed);
            return false;
        }
        state_ = State::Prime;
        prime_started_ms_ = in.now_ms;
        return true;
    }
    // PRIME/RUNNING recheck fresh calibrated required sensors and interlocks.
    const auto blocker = start_blocker(in);
    if (blocker != Reason::None) {
        fault(blocker);
        return false;
    }
    if (state_ == State::Prime) {
        if (in.now_ms < prime_started_ms_ || in.now_ms - prime_started_ms_ >= prime_timeout_ms) {
            fault(Reason::PrimeTimeout);
            return false;
        }
        if (event == Event::FlowConfirmed) {
            if (!outputs_.set_power(true)) {
                fault(Reason::PowerStartFailed);
                return false;
            }
            state_ = State::Running;
            return true;
        }
    }
    return event == Event::Tick;
}
} // namespace app::state_machine
