// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <chrono>

namespace Jobs
{
	class Futex
	{
	private:
		void* address = nullptr;

	public:
		Futex() = default;
		Futex(const Futex&) = delete;
		Futex(Futex&&) noexcept = delete;  // #TODO: Implement.

		Futex& operator=(const Futex&) = delete;
		Futex& operator=(Futex&&) noexcept = delete;  // #TODO: Implement.

		template <typename T>
		void Set(T* inAddress);

		template <typename T>
		bool Wait(T* compareAddress) const;
		template <typename T, typename Rep, typename Period>
		bool Wait(T* compareAddress, const std::chrono::duration<Rep, Period>& timeout) const;

		void NotifyOne() const;
		void NotifyAll() const;

	private:
		bool Wait(void* compareAddress, size_t size, uint64_t timeoutNs) const;
	};

	template <typename T>
	void Futex::Set(T* inAddress)
	{
		address = static_cast<void*>(inAddress);
	}

	template <typename T>
	bool Futex::Wait(T* compareAddress) const
	{
		return Wait(static_cast<void*>(compareAddress), sizeof(T), 0);
	}

	template <typename T, typename Rep, typename Period>
	bool Futex::Wait(T* compareAddress, const std::chrono::duration<Rep, Period>& timeout) const
	{
		return Wait(static_cast<void*>(compareAddress), sizeof(T), std::chrono::duration_cast<std::chrono::nanoseconds>(timeout).count());
	}
}