// Copyright (c) 2019 Andrew Depke

#pragma once

#include <iostream>  // std::cerr
#include <exception>  // std::terminate

#if _DEBUG || NDEBUG
  // MSVC doesn't yet support __VA_OPT__, see https://en.cppreference.com/w/cpp/compiler_support
  #if defined(_MSC_VER) /*&& _MSC_VER <= 1916*/
    #define JOBS_ASSERT(Expression, ...) \
	do \
	{ \
		if (!(Expression)) \
		{ \
			std::cerr << "Assertion \"" << #Expression << "\" failed in " << __FILE__ << ", line " << __LINE__ << ": " << __VA_ARGS__; \
			std::terminate(); \
		} \
	} \
	while (0)
  #else
    #define JOBS_ASSERT(Expression, ...) \
	do \
	{ \
		if (!(Expression)) \
		{ \
			std::cerr << "Assertion \"" << #Expression << "\" failed in " << __FILE__ << ", line " << __LINE__ __VA_OPT__(<< ": " << __VA_ARGS__); \
			std::terminate(); \
		} \
	} \
	while (0)
  #endif
#else
  #define JOBS_ASSERT(Expression, ...) do {} while (0)
#endif