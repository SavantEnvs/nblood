// The engine's Xmalloc/Xfree sit on smmalloc, which must be created before first use (engineInit does it
// in the game). A single 16-byte pool bucket (smmalloc cannot be created with none) and no thread cache
// sends every allocation larger than 16 bytes to the system allocator, so ASan sees real block bounds
// instead of slices of 2MB pools.
#pragma once
#include "compat.h"
#include "loguru.hpp"

static inline void nblood_heap_init()
{
    if (!g_sm_heap)
    {
        g_sm_heap = _sm_allocator_create(1, 4096);
        // parse errors are logged to stderr by the engine; silence them so each exec stays cheap
        loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
    }
}
