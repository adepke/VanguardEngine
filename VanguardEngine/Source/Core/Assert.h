// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Core/Misc.h>

#include <iostream>
#include <exception>
#include <cstdlib>

// MSVC doesn't yet support __VA_OPT__, see https://en.cppreference.com/w/cpp/compiler_support
#ifdef _MSC_VER
#define VGEnsure(Condition, ...) \
	do \
	{ \
		if (!(Condition)) VGUnlikely \
		{ \
			std::cerr << "Assertion \"" << #Condition << "\" failed in " << __FILE__ << ", line " << __LINE__ << ": " << __VA_ARGS__; \
			VGBreak(); \
			std::abort(); \
		} \
	} \
	while (0)
#else
#define VGEnsure(Condition, ...) \
	do \
	{ \
		if (!(Condition)) VGUnlikely \
		{ \
			std::cerr << "Assertion \"" << #Condition << "\" failed in " << __FILE__ << ", line " << __LINE__ __VA_OPT__(<< ": " << __VA_ARGS__); \
			VGBreak(); \
			std::abort(); \
		} \
	} \
	while (0)
#endif

#if !BUILD_RELEASE
#define VGAssert(Condition, ...) VGEnsure(Condition, __VA_ARGS__)
#else
#define VGAssert(Condition, ...) do {} while (0)
#endif