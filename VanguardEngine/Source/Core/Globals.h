// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Core/CommandLine.h>

#include <vector>
#include <string>
#include <thread>

inline CommandLineOptions GCommandLineOptions;
inline std::vector<std::thread::id> GProcessThreads;