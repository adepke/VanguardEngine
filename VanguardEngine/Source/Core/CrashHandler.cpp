// Copyright (c) 2019-2022 Andrew Depke

#include <Core/CrashHandler.h>
#include <Core/Logging.h>
#include <Core/Globals.h>
#include <Core/Windows/WindowsMinimal.h>

#include <errhandlingapi.h>

#include <csignal>
#include <thread>
#include <map>
#include <sstream>
#include <fstream>

std::map<int, const char*> signalNames = {
	{ SIGINT, "Interrupt" },
	{ SIGILL, "Illegal instruction" },
	{ SIGFPE, "Floating point exception" },
	{ SIGSEGV, "Segment violation" },
	{ SIGTERM, "Software termination" },
	{ SIGBREAK, "Break sequence" },
	{ SIGABRT, "Abnormal termination via abort" },
	{ SIGABRT_COMPAT, "Abnormal termination via abort" }
};

void RegisterCrashHandlers()
{
	auto singalHandler = +[](int signal)
	{
		std::wstringstream stream;
		stream << "Signal: " << signalNames[signal] << " (" << signal << ")";
		RequestCrashMessage(stream.str());
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
			case EXCEPTION_ACCESS_VIOLATION: RequestCrashMessage(VGText("SEH: Access violation.")); break;
			case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: RequestCrashMessage(VGText("SEH: Array out of bounds.")); break;
			case EXCEPTION_BREAKPOINT: RequestCrashMessage(VGText("SEH: Breakpoint hit.")); break;
			case EXCEPTION_DATATYPE_MISALIGNMENT: RequestCrashMessage(VGText("SEH: Datatype misalignment.")); break;
			case EXCEPTION_FLT_DENORMAL_OPERAND: RequestCrashMessage(VGText("SEH: Floating-point denormal operand.")); break;
			case EXCEPTION_FLT_DIVIDE_BY_ZERO: RequestCrashMessage(VGText("SEH: Floating-point divide by zero.")); break;
			case EXCEPTION_FLT_INEXACT_RESULT: RequestCrashMessage(VGText("SEH: Floating-point inexact result.")); break;
			case EXCEPTION_FLT_INVALID_OPERATION: RequestCrashMessage(VGText("SEH: Floating-point invalid operation.")); break;
			case EXCEPTION_FLT_OVERFLOW: RequestCrashMessage(VGText("SEH: Floating-point value overflow.")); break;
			case EXCEPTION_FLT_STACK_CHECK: RequestCrashMessage(VGText("SEH: Floating-point stack overflow or underflow.")); break;
			case EXCEPTION_FLT_UNDERFLOW: RequestCrashMessage(VGText("SEH: Floating-point value underflow.")); break;
			case EXCEPTION_ILLEGAL_INSTRUCTION: RequestCrashMessage(VGText("SEH: Invalid instruction.")); break;
			case EXCEPTION_IN_PAGE_ERROR: RequestCrashMessage(VGText("SEH: Attempted to access page not present.")); break;
			case EXCEPTION_INT_DIVIDE_BY_ZERO: RequestCrashMessage(VGText("SEH: Integer divide by zero.")); break;
			case EXCEPTION_INT_OVERFLOW: RequestCrashMessage(VGText("SEH: Integer value overflow.")); break;
			case EXCEPTION_INVALID_DISPOSITION: RequestCrashMessage(VGText("SEH: Invalid disposition.")); break;
			case EXCEPTION_NONCONTINUABLE_EXCEPTION: RequestCrashMessage(VGText("SEH: Thread attempted to continue after fatal error.")); break;
			case EXCEPTION_PRIV_INSTRUCTION: RequestCrashMessage(VGText("SEH: Attempted to execute instruction not allowed.")); break;
			case EXCEPTION_SINGLE_STEP: RequestCrashMessage(VGText("SEH: Single step.")); break;
			case EXCEPTION_STACK_OVERFLOW: RequestCrashMessage(VGText("SEH: Stack overflow.")); break;
			default: RequestCrashMessage(VGText("SEH: Unrecognized exception code.")); break;
			}
		}

		else
		{
			RequestCrashMessage(VGText("Unknown SEH exception."));
		}

		return EXCEPTION_EXECUTE_HANDLER;
	});
};

std::vector<HANDLE> threadHandles;

void SuspendProcessThreads()
{
	const auto caller = std::this_thread::get_id();

	for (auto thread : GProcessThreads)
	{
		if (thread != caller)
		{
			const auto id = *(DWORD*)&thread;
			auto threadHandle = OpenThread(THREAD_SUSPEND_RESUME, false , id);
			if (threadHandle != 0)
			{
				SuspendThread(threadHandle);
			}
			
			else
			{
				VGLog(logCore, "Failed to suspend process thread {}: {}", id, GetLastError());
			}
		}
	}
}

void ResumeProcessThreads()
{
	for (auto handle : threadHandles)
	{
		ResumeThread(handle);
		CloseHandle(handle);
	}

	threadHandles.clear();
}

void ReportInternalCrashEvent(const std::wstring& reason, bool printToLog)
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