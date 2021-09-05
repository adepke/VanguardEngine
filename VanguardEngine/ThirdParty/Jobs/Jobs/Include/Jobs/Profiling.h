// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#if JOBS_ENABLE_PROFILING
  #include <Tracy.hpp>
  #define JOBS_SCOPED_STAT(name) ZoneScopedN(name)
#else
  #define JOBS_SCOPED_STAT(name)
#endif