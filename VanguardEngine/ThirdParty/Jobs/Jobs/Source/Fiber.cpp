// Copyright (c) 2019-2021 Andrew Depke

#include <Jobs/Fiber.h>

#include <Jobs/FiberRoutines.h>
#include <Jobs/Logging.h>
#include <Jobs/Assert.h>
#include <Jobs/Profiling.h>

#include <utility>  // std::swap

#if JOBS_PLATFORM_WINDOWS
  #include <Jobs/WindowsMinimal.h>
#else
  #include <unistd.h>
  #include <cstdlib>
#endif

namespace Jobs
{
	Fiber::Fiber(size_t stackSize, EntryType entry, Manager* owner) : data(reinterpret_cast<void*>(owner))
	{
		JOBS_SCOPED_STAT("Fiber Creation");

		JOBS_LOG(LogLevel::Log, "Building fiber.");
		JOBS_ASSERT(stackSize > 0, "Stack size must be greater than 0.");

		// Perform a page-aligned allocation for the stack. This is needed to allow for canary pages in overrun detection.

#if JOBS_PLATFORM_WINDOWS
		SYSTEM_INFO sysInfo{};
		GetSystemInfo(&sysInfo);
		const auto alignment = sysInfo.dwPageSize;

		stack = _aligned_malloc(stackSize, alignment);
#else
		const auto alignment = getpagesize();

		stack = std::aligned_alloc(alignment, stackSize);
#endif

		void* stackTop = reinterpret_cast<std::byte*>(stack) + (stackSize * sizeof(std::byte));

		context = make_fcontext(stackTop, stackSize, entry);

		JOBS_ASSERT(context, "Failed to build fiber.");
	}

	Fiber::Fiber(Fiber&& other) noexcept
	{
		Swap(other);
	}

	Fiber::~Fiber()
	{
#if JOBS_PLATFORM_WINDOWS
		_aligned_free(stack);
#else
		std::free(stack);
#endif
	}

	Fiber& Fiber::operator=(Fiber&& other) noexcept
	{
		Swap(other);

		return *this;
	}

	void Fiber::Schedule(Fiber& from)
	{
		JOBS_SCOPED_STAT("Fiber Schedule");

		JOBS_LOG(LogLevel::Log, "Scheduling fiber.");

		jump_fcontext(&from.context, context, data);
	}

	void Fiber::Swap(Fiber& other) noexcept
	{
		std::swap(context, other.context);
		std::swap(stack, other.stack);
		std::swap(data, other.data);
	}
}