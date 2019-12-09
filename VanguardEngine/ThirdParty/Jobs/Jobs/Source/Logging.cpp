// Copyright (c) 2019 Andrew Depke

#include <Jobs/Logging.h>

#include <cstddef>  // std::size_t
#include <cstdarg>  // std::va_list, va_start, va_end
#include <cstdio>  // std::vsnprintf

namespace Jobs
{
	void LogManager::Log(LogLevel Level, const std::string& Format, ...)
	{
		if (!OutputDevice)
		{
			return;
		}

		std::string Output;

		switch (Level)
		{
		case LogLevel::Log:
			Output = "[Log] ";
			break;
		case LogLevel::Warning:
			Output = "[Warning] ";
			break;
		case LogLevel::Error:
			Output = "[Error] ";
			break;
		}

		constexpr size_t BufferSize = 1024;
		char Buffer[BufferSize];

		std::va_list Args;
		va_start(Args, &Format);

		Lock.Lock();  // Latest possible point to lock: We need to guard our c_str() buffer resource.

		std::vsnprintf(Buffer, BufferSize, Format.c_str(), Args);
		va_end(Args);

		std::string Formatted{ Buffer };

		Output += Formatted + "\n";

		//Lock.Lock();  // Can't lock this late, we accessed the c_str() resource.
		*OutputDevice << Output.c_str();
		Lock.Unlock();
	}

	void LogManager::SetOutputDevice(std::ostream& Device)
	{
		OutputDevice = &Device;
	}
}