// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Core/Misc.h>

#include <iostream>
#include <exception>
#include <cstdlib>

// MSVC doesn't yet support __VA_OPT__, see https://en.cppreference.com/w/cpp/compiler_support
#ifdef _MSC_VER
#define VGEnsure(condition, ...) \
	do \
	{ \
		if (!(condition)) VGUnlikely \
		{ \
			std::cerr << "Assertion \"" << #condition << "\" failed in " << __FILE__ << ", line " << __LINE__ << ": " << __VA_ARGS__; \
			VGBreak(); \
			std::abort(); \
		} \
	} \
	while (0)
#else
#define VGEnsure(condition, ...) \
	do \
	{ \
		if (!(condition)) VGUnlikely \
		{ \
			std::cerr << "Assertion \"" << #condition << "\" failed in " << __FILE__ << ", line " << __LINE__ __VA_OPT__(<< ": " << __VA_ARGS__); \
			VGBreak(); \
			std::abort(); \
		} \
	} \
	while (0)
#endif

#if !BUILD_RELEASE
#define VGAssert(condition, ...) VGEnsure(condition, __VA_ARGS__)
#else
#define VGAssert(condition, ...) do {} while (0)
#endif