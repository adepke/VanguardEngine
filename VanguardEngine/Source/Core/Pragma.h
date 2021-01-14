// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#define VGConcat(left, right) left ## right
#define VGStringify(raw) #raw

#ifdef _MSC_VER
#define VGWarningPush __pragma(warning(push))
#define VGWarningPop __pragma(warning(pop))
#define VGWarningDisable(number, string) __pragma(warning(disable:number))
#elif defined(__GNUC__) || defined(__clang__)
#define _Detail_VGExpandStringify(token) VGStringify(token)
#define VGWarningPush _Pragma("GCC diagnostic push")
#define VGWarningPop _Pragma("GCC diagnostic pop")
#define VGWarningDisable(number, string) _Pragma(_Detail_VGExpandStringify(GCC diagnostic ignored VGStringify(-W##string)))
#else
#define VGWarningPush
#define VGWarningPop
#define VGWarningDisable(number, string)
#endif