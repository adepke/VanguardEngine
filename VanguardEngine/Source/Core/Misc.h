// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#ifndef _MSC_VER
#include <csignal>
#endif

#define VGText(literal) L##literal

#if __has_cpp_attribute(likely)
#define VGLikely [[likely]]
#else
#define VGLikely
#endif

#if __has_cpp_attribute(unlikely)
#define VGUnlikely [[unlikely]]
#else
#define VGUnlikely
#endif

#ifdef _MSC_VER
#define VGBreak() __debugbreak()
#else
#define VGBreak() raise(SIGTRAP)
#endif

#ifdef _MSC_VER
#define VGForceInline __forceinline
#else
#define VGForceInline __attribute__((always_inline))
#endif