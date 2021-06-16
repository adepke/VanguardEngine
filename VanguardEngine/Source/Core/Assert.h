// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Core/Misc.h>
#include <Core/CrashHandler.h>

#include <cstdio>
#include <sstream>

#ifdef _MSC_VER
// Not sure why MSVC doesn't require the __VA_OPT__, oh well.
#define VGEnsure(condition, format, ...) \
	do \
	{ \
		if (!(condition)) VGUnlikely \
		{ \
			char buffer[256]; \
			std::snprintf(buffer, std::size(buffer), format, __VA_ARGS__); \
			std::wstringstream stream; \
			stream << "Assertion failed in " << __FILE__ << ":" << __LINE__ << "\nCondition: " << #condition << "\nMessage: " << buffer; \
			RequestCrash(stream.str(), true); \
		} \
	} \
	while (0)
#else
#define VGEnsure(condition, format, ...) \
	do \
	{ \
		if (!(condition)) VGUnlikely \
		{ \
			char buffer[256]; \
			std::snprintf(buffer, std::size(buffer), format __VA_OPT__(,) __VA_ARGS__); \
			std::wstringstream stream; \
			stream << "Assertion failed in " << __FILE__ << ":" << __LINE__ << "\nCondition: " << #condition << "\nMessage: " << buffer; \
			RequestCrash(stream.str(), true); \
		} \
	} \
	while (0)
#endif

#if !BUILD_RELEASE
#define VGAssert(condition, format, ...) VGEnsure(condition, format, __VA_ARGS__)
#else
#define VGAssert(condition, format, ...) do {} while (0)
#endif