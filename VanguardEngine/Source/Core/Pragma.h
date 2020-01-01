// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#define VGConcat(Left, Right) Left ## Right
#define VGStringify(Raw) #Raw

#ifdef _MSC_VER
#define VGWarningPush __pragma(warning(push))
#define VGWarningPop __pragma(warning(pop))
#define VGWarningDisable(Number, String) __pragma(warning(disable:Number))
#elif defined(__GNUC__) || defined(__clang__)
#define _Detail_VGExpandStringify(Token) VGStringify(Token)
#define VGWarningPush _Pragma("GCC diagnostic push")
#define VGWarningPop _Pragma("GCC diagnostic pop")
#define VGWarningDisable(Number, String) _Pragma(_Detail_VGExpandStringify(GCC diagnostic ignored VGStringify(-W##String)))
#else
#define VGWarningPush
#define VGWarningPop
#define VGWarningDisable(Number, String)
#endif