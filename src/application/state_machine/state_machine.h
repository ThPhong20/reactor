#pragma once

// Planned reactor states. Transition and output-control logic are not implemented.
namespace app::state_machine {

enum class State { Init, Idle, Prime, Running, Fault };

// TODO: Define events, guards, and transitions.

} // namespace app::state_machine
