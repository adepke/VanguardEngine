// Copyright (c) 2019-2021 Andrew Depke

#include <Core/CrashHandler.h>
#include <Core/Windows/WindowsMinimal.h>

#include <errhandlingapi.h>

#include <csignal>
#include <thread>
#include <sstream>
#include <fstream>

void RegisterCrashHandlers()
{
	auto singalHandler = +[](int signal)
	{
		std::wstringstream stream;
		stream << "Signal: " << signal;
		RequestCrash(stream.str());
	};

	std::signal(SIGTERM, singalHandler);
	std::signal(SIGSEGV, singalHandler);
	std::signal(SIGINT, singalHandler);
	std::signal(SIGILL, singalHandler);
	std::signal(SIGABRT, singalHandler);
	std::signal(SIGFPE, singalHandler);

	// SEH handler.
	SetUnhandledExceptionFilter(+[](PEXCEPTION_POINTERS exceptionPointers) -> LONG
	{
		if (exceptionPointers && exceptionPointers->ExceptionRecord)
		{
			switch (exceptionPointers->ExceptionRecord->ExceptionCode)
			{
			case EXCEPTION_ACCESS_VIOLATION: RequestCrash(VGText("SEH: Access violation.")); break;
			case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: RequestCrash(VGText("SEH: Array out of bounds.")); break;
			case EXCEPTION_BREAKPOINT: RequestCrash(VGText("SEH: Breakpoint hit.")); break;
			case EXCEPTION_DATATYPE_MISALIGNMENT: RequestCrash(VGText("SEH: Datatype misalignment.")); break;
			case EXCEPTION_FLT_DENORMAL_OPERAND: RequestCrash(VGText("SEH: Floating-point denormal operand.")); break;
			case EXCEPTION_FLT_DIVIDE_BY_ZERO: RequestCrash(VGText("SEH: Floating-point divide by zero.")); break;
			case EXCEPTION_FLT_INEXACT_RESULT: RequestCrash(VGText("SEH: Floating-point inexact result.")); break;
			case EXCEPTION_FLT_INVALID_OPERATION: RequestCrash(VGText("SEH: Floating-point invalid operation.")); break;
			case EXCEPTION_FLT_OVERFLOW: RequestCrash(VGText("SEH: Floating-point value overflow.")); break;
			case EXCEPTION_FLT_STACK_CHECK: RequestCrash(VGText("SEH: Floating-point stack overflow or underflow.")); break;
			case EXCEPTION_FLT_UNDERFLOW: RequestCrash(VGText("SEH: Floating-point value underflow.")); break;
			case EXCEPTION_ILLEGAL_INSTRUCTION: RequestCrash(VGText("SEH: Invalid instruction.")); break;
			case EXCEPTION_IN_PAGE_ERROR: RequestCrash(VGText("SEH: Attempted to access page not present.")); break;
			case EXCEPTION_INT_DIVIDE_BY_ZERO: RequestCrash(VGText("SEH: Integer divide by zero.")); break;
			case EXCEPTION_INT_OVERFLOW: RequestCrash(VGText("SEH: Integer value overflow.")); break;
			case EXCEPTION_INVALID_DISPOSITION: RequestCrash(VGText("SEH: Invalid disposition.")); break;
			case EXCEPTION_NONCONTINUABLE_EXCEPTION: RequestCrash(VGText("SEH: Thread attempted to continue after fatal error.")); break;
			case EXCEPTION_PRIV_INSTRUCTION: RequestCrash(VGText("SEH: Attempted to execute instruction not allowed.")); break;
			case EXCEPTION_SINGLE_STEP: RequestCrash(VGText("SEH: Single step.")); break;
			case EXCEPTION_STACK_OVERFLOW: RequestCrash(VGText("SEH: Stack overflow.")); break;
			default: RequestCrash(VGText("SEH: Unrecognized exception code.")); break;
			}
		}

		else
		{
			RequestCrash(VGText("Unknown SEH exception."));
		}

		return EXCEPTION_EXECUTE_HANDLER;
	});
};

void ReportCrashEvent(const std::wstring& reason, bool printToLog)
{
	std::wstringstream stream;
	stream << "Crash reported on thread '" << std::this_thread::get_id() << "': " << reason;

	// Crashes from asserts will not output anything to the normal logger, so output it here.
	if (printToLog)
	{
		OutputDebugString(stream.str().c_str());
		OutputDebugString(VGText("\n"));
	}

	std::wofstream log{ "CrashLog.txt", std::ios::out | std::ios::trunc };
	if (log.is_open())
	{
		log << stream.str();
		log.flush();
		log.close();
	}

	// If we have a debugger, break on the offending line instead of displaying a message box.
	if (!HasDebuggerAttached())
	{
		MessageBox(nullptr, stream.str().c_str(), VGText("Vanguard Crashed"), MB_OK | MB_ICONERROR);
	}
}

bool HasDebuggerAttached()
{
	return IsDebuggerPresent();
}