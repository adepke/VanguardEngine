// Copyright (c) 2019 Andrew Depke

#pragma once

#include <Core/Pragma.h>
#include <Core/Logging.h>

using namespace std::literals::chrono_literals;
using namespace std::literals::string_literals;
using namespace std::literals::string_view_literals;

#ifdef TEXT
#undef TEXT
#endif

#define TEXT(Literal) L##Literal