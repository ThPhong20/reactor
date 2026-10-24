#pragma once

// Finite state machine: INIT, IDLE, PRIME, RUNNING, and FAULT; commands the pump manager.
namespace app::state_machine {

enum class State { Init, Idle, Prime, Running, Fault };

// TODO: Define events, guards, and transitions.

} // namespace app::state_machine
