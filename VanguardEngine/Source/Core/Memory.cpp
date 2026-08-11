// Copyright (c) 2019-2022 Andrew Depke

// Global replacement of operator new/delete, used to feed every heap allocation to Tracy.
// See: https://en.cppreference.com/cpp/memory/new/operator_new

#if 0

#include <tracy/Tracy.hpp>

#include <cstddef>
#include <cstdlib>
#include <new>

#if PLATFORM_WINDOWS
#include <malloc.h>
#endif

// Set to >0 to enable call stack capture for every memory allocation. This is expensive and
// should only be used for debugging a leak or such. 0 is disabled.
#define MEMORY_CALLSTACK_DEPTH 0

#if MEMORY_CALLSTACK_DEPTH > 0
#define VGTrackAlloc(ptr, size) TracyAllocS(ptr, size, MEMORY_CALLSTACK_DEPTH)
#define VGTrackFree(ptr) TracyFreeS(ptr, MEMORY_CALLSTACK_DEPTH)
#else
#define VGTrackAlloc(ptr, size) TracyAlloc(ptr, size)
#define VGTrackFree(ptr) TracyFree(ptr)
#endif

namespace
{
	void* AllocateTracked(std::size_t size) noexcept
	{
		void* ptr = std::malloc(size);
		VGTrackAlloc(ptr, size);
		return ptr;
	}

	void* AllocateTrackedAligned(std::size_t size, std::size_t alignment) noexcept
	{
#if PLATFORM_WINDOWS
		void* ptr = ::_aligned_malloc(size, alignment);
#else
		// std::aligned_alloc requires the size to be a multiple of the alignment.
		void* ptr = std::aligned_alloc(alignment, (size + alignment - 1) & ~(alignment - 1));
#endif
		VGTrackAlloc(ptr, size);
		return ptr;
	}

	void FreeTracked(void* ptr) noexcept
	{
		if (!ptr)
			return;
		VGTrackFree(ptr);
		std::free(ptr);
	}

	void FreeTrackedAligned(void* ptr) noexcept
	{
		if (!ptr)
			return;
		VGTrackFree(ptr);
#if PLATFORM_WINDOWS
		::_aligned_free(ptr);
#else
		std::free(ptr);
#endif
	}
}

// Global replacements

void* operator new(std::size_t size)
{
	if (void* ptr = AllocateTracked(size))
		return ptr;

	throw std::bad_alloc{};
}

void* operator new(std::size_t size, std::align_val_t alignment)
{
	if (void* ptr = AllocateTrackedAligned(size, static_cast<std::size_t>(alignment)))
		return ptr;

	throw std::bad_alloc{};
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
	return AllocateTracked(size);
}

void* operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
	return AllocateTrackedAligned(size, static_cast<std::size_t>(alignment));
}

void* operator new[](std::size_t size)
{
	return operator new(size);
}

void* operator new[](std::size_t size, std::align_val_t alignment)
{
	return operator new(size, alignment);
}

void* operator new[](std::size_t size, const std::nothrow_t& tag) noexcept
{
	return operator new(size, tag);
}

void* operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t& tag) noexcept
{
	return operator new(size, alignment, tag);
}

// Single object deallocation

void operator delete(void* ptr) noexcept
{
	FreeTracked(ptr);
}

void operator delete(void* ptr, std::align_val_t) noexcept
{
	FreeTrackedAligned(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept
{
	operator delete(ptr);
}

void operator delete(void* ptr, std::size_t, std::align_val_t alignment) noexcept
{
	operator delete(ptr, alignment);
}

void operator delete(void* ptr, const std::nothrow_t&) noexcept
{
	operator delete(ptr);
}

void operator delete(void* ptr, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
	operator delete(ptr, alignment);
}

// Array deallocation

void operator delete[](void* ptr) noexcept
{
	operator delete(ptr);
}

void operator delete[](void* ptr, std::align_val_t alignment) noexcept
{
	operator delete(ptr, alignment);
}

void operator delete[](void* ptr, std::size_t) noexcept
{
	operator delete(ptr);
}

void operator delete[](void* ptr, std::size_t, std::align_val_t alignment) noexcept
{
	operator delete(ptr, alignment);
}

void operator delete[](void* ptr, const std::nothrow_t&) noexcept
{
	operator delete(ptr);
}

void operator delete[](void* ptr, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
	operator delete(ptr, alignment);
}

#endif