// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#if defined(_WINDOWS_) && !defined(WINDOWSMINIMAL)
// #TODO: Fix leaking in Logging.h, then re-enable this.
//#error "Included Windows.h manually, include this file instead."
#endif

#define WINDOWSMINIMAL

#define WIN32_LEAN_AND_MEAN
#define NOGDICAPMASKS
//#define NOVIRTUALKEYCODES
//#define NOWINMESSAGES
//#define NOWINSTYLES
//#define NOSYSMETRICS
#define NOMENUS
#define NOICONS
#define NOKEYSTATES
//#define NOSYSCOMMANDS
#define NORASTEROPS
//#define NOSHOWWINDOW
#define NOATOM
#define NOCLIPBOARD
#define NOCOLOR
#define NOCTLMGR
#define NODRAWTEXT
#define NOGDI
#define NOKERNEL
//#define NOUSER
//#define NONLS
//#define NOMB
#define NOMEMMGR
#define NOMETAFILE
#define NOMINMAX
//#define NOMSG
#define NOOPENFILE
#define NOSCROLL
#define NOSERVICE
#define NOSOUND
#define NOTEXTMETRIC
#define NOWH
//#define NOWINOFFSETS
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX

#include <Windows.h>

#undef INT
#undef UINT
#undef DWORD
#undef FLOAT
#undef TEXT
#undef TRUE
#undef FALSE