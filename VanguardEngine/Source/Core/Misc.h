// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#ifndef _MSC_VER
#include <signal.h>
#endif

#define VGText(Literal) L##Literal

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