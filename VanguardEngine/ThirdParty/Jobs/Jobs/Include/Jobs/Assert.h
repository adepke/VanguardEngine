// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <iostream>  // std::cerr
#include <exception>  // std::terminate

#if !NDEBUG
  #define JOBS_ASSERT(expression, ...) \
	do \
	{ \
		if (!(expression)) \
		{ \
			std::cerr << "Assertion \"" << #expression << "\" failed in " << __FILE__ << ", line " << __LINE__ << ": " << __VA_ARGS__; \
			std::terminate(); \
		} \
	} \
	while (0)
#else
  #define JOBS_ASSERT(expression, ...) do {} while (0)
#endif