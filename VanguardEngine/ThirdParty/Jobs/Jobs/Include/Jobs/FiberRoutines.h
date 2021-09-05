// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <cstddef>

namespace Jobs
{
    // Routines implemented in assembly.
    extern "C" void* make_fcontext(void* stack, size_t stackSize, void (*entry)(void* data));
    extern "C" void jump_fcontext(void* from, void* to, void* data);
}