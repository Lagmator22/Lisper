#include "interrupt.h"

#include <csignal>

namespace {
volatile std::sig_atomic_t g_interrupted = 0;
}

namespace interrupt_state {

void request_stop() noexcept { g_interrupted = 1; }

void reset() noexcept { g_interrupted = 0; }

bool is_interrupted() noexcept { return g_interrupted != 0; }

} // namespace interrupt_state
