// Copyright (c) 2019 Andrew Depke

#include <Core/Logging.h>

#if PLATFORM_WINDOWS
#include <Core/Windows/WindowsMinimal.h>
#endif

PlatformErrorType GetPlatformError()
{
#if PLATFORM_WINDOWS
	return HRESULT_FROM_WIN32(GetLastError());
#else
	return 0;
#endif
}