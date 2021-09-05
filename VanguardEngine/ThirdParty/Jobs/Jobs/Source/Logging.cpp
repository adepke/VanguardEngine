// Copyright (c) 2019-2021 Andrew Depke

#include <Jobs/Logging.h>

#include <cstddef>  // std::size_t
#include <cstdarg>  // std::va_list, va_start, va_end
#include <cstdio>  // std::vsnprintf

namespace Jobs
{
	void LogManager::Log(LogLevel level, const std::string& format, ...)
	{
		if (!outputDevice)
		{
			return;
		}

		std::string output;

		switch (level)
		{
		case LogLevel::Log:
			output = "[Log] ";
			break;
		case LogLevel::Warning:
			output = "[Warning] ";
			break;
		case LogLevel::Error:
			output = "[Error] ";
			break;
		}

		constexpr size_t bufferSize = 1024;
		char buffer[bufferSize];

		std::va_list args;
		va_start(args, &format);

		lock.Lock();  // Latest possible point to lock: We need to guard our c_str() buffer resource.

		std::vsnprintf(buffer, bufferSize, format.c_str(), args);
		va_end(args);

		std::string formatted{ buffer };

		output += formatted + "\n";

		//Lock.Lock();  // Can't lock this late, we accessed the c_str() resource.
		*outputDevice << output.c_str();
		lock.Unlock();
	}

	void LogManager::SetOutputDevice(std::ostream& device)
	{
		outputDevice = &device;
	}
}