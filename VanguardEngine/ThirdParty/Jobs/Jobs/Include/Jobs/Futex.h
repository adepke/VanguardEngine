// Copyright (c) 2019 Andrew Depke

#pragma once

#include <chrono>

namespace Jobs
{
	class Futex
	{
	private:
		void* Address = nullptr;

	public:
		Futex() = default;
		Futex(const Futex&) = delete;
		Futex(Futex&&) noexcept = delete;  // #TODO: Implement.

		Futex& operator=(const Futex&) = delete;
		Futex& operator=(Futex&&) noexcept = delete;  // #TODO: Implement.

		template <typename T>
		void Set(T* InAddress);

		template <typename T>
		bool Wait(T* CompareAddress) const;
		template <typename T, typename Rep, typename Period>
		bool Wait(T* CompareAddress, const std::chrono::duration<Rep, Period>& Timeout) const;

		void NotifyOne() const;
		void NotifyAll() const;

	private:
		bool Wait(void* CompareAddress, size_t Size, uint64_t TimeoutNs) const;
	};

	template <typename T>
	void Futex::Set(T* InAddress)
	{
		Address = static_cast<void*>(InAddress);
	}

	template <typename T>
	bool Futex::Wait(T* CompareAddress) const
	{
		return Wait(static_cast<void*>(CompareAddress), sizeof(T), 0);
	}

	template <typename T, typename Rep, typename Period>
	bool Futex::Wait(T* CompareAddress, const std::chrono::duration<Rep, Period>& Timeout) const
	{
		return Wait(static_cast<void*>(CompareAddress), sizeof(T), std::chrono::duration_cast<std::chrono::nanoseconds>(Timeout).count());
	}
}