#pragma once
#include "application/state_machine/state_machine.h"

namespace app {

// Call once at boot, from the application task; initialize with pump outputs off.
void init();
// Call only from the application task. No serial/button command parser yet.
bool command(state_machine::Event event);
// Return the current reactor state; call only from the application task.
state_machine::State reactor_state();
// Continuously sample, store, evaluate the FSM, and update outputs.
[[noreturn]] void run();

} // namespace app
