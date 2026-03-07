#pragma once

namespace interrupt_state {

// Request a cooperative stop across all running loops.
void request_stop() noexcept;

// Clear interrupt state before starting a new CLI run.
void reset() noexcept;

// Check if an interrupt has been requested.
bool is_interrupted() noexcept;

} // namespace interrupt_state
