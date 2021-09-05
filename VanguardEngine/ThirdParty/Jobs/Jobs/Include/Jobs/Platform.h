// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#ifdef _WIN32
  #define JOBS_PLATFORM_WINDOWS 1
#else
  #define JOBS_PLATFORM_POSIX 1
#endif

#ifndef JOBS_PLATFORM_WINDOWS
  #define JOBS_PLATFORM_WINDOWS 0
#endif
#ifndef JOBS_PLATFORM_POSIX
  #define JOBS_PLATFORM_POSIX 0
#endif