// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <atomic>  // std::atomic
#include <mutex>  // std::mutex
#include <Jobs/Fiber.h>
#include <Jobs/Futex.h>
#include <Jobs/FutexConditionVariable.h>

namespace Jobs
{
	template <typename T = unsigned int>
	class Counter
	{
		friend void ManagerFiberEntry(void*);

	public:
		using Type = T;

	private:
		std::atomic<T> internalValue;
		Futex insideLock;  // Timed unsafe signaling for jobs.
		FutexConditionVariable outsideLock;  // Blind spot safe signaling for non-worker threads.

		bool Evaluate(const T& expectedValue) const
		{
			return internalValue.load() <= expectedValue;  // #TODO: Memory order.
		}

	public:
		Counter();
		Counter(T initialValue);
		Counter(const Counter&) = delete;
		Counter(Counter&& other) noexcept = delete;  // #TODO: Implement.
		~Counter() = default;

		Counter& operator=(const Counter&) = delete;
		Counter& operator=(Counter&&) noexcept = delete;  // #TODO: Implement.

		Counter& operator++();
		Counter& operator--();

		Counter& operator+=(T target);

		// Atomically fetch the current value.
		const T Get() const;

		// Blocking operation.
		void Wait(T expectedValue);

		// Blocking operation.
		template <typename Rep, typename Period>
		bool WaitFor(T expectedValue, const std::chrono::duration<Rep, Period>& timeout);

	private:
		// Futex based blocking, reserved for jobs. Susceptible to blind spot signaling.
		template <typename Rep, typename Period>
		bool UnsafeWait(T expectedValue, const std::chrono::duration<Rep, Period>& timeout);
	};

	template <typename T>
	Counter<T>::Counter() : internalValue(0) {}

	template <typename T>
	Counter<T>::Counter(T initialValue) : internalValue(initialValue) {}

	template <typename T>
	Counter<T>& Counter<T>::operator++()
	{
		++internalValue;

		// Don't notify.

		return *this;
	}

	template <typename T>
	Counter<T>& Counter<T>::operator--()
	{
		--internalValue;

		// Notify waiting jobs.
		insideLock.NotifyAll();  // We don't notify under lock since a blind spot signal isn't fatal, it will only cost us the timeout period.

		// Notify waiting outsiders.
		outsideLock.Lock();
		outsideLock.NotifyAll();  // Notify under lock to prevent a blind spot signal, which can be fatal.
		outsideLock.Unlock();

		return *this;
	}

	template <typename T>
	Counter<T>& Counter<T>::operator+=(T target)
	{
		internalValue += target;

		// Don't notify.

		return *this;
	}

	template <typename T>
	const T Counter<T>::Get() const
	{
		return internalValue.load();  // #TODO: Memory order.
	}

	template <typename T>
	void Counter<T>::Wait(T expectedValue)
	{
		outsideLock.Lock();

		while (!Evaluate(expectedValue))
		{
			outsideLock.Wait();
		}

		outsideLock.Unlock();
	}

	template <typename T>
	template <typename Rep, typename Period>
	bool Counter<T>::UnsafeWait(T expectedValue, const std::chrono::duration<Rep, Period>& timeout)
	{
		// InternalCapture is the saved state of Internal at the time of sleeping. We will use this to know if it changed.
		if (auto internalCapture{ internalValue.load() }; internalCapture != expectedValue)  // #TODO: Memory order.
		{
			insideLock.Set(&internalValue);

			auto timeRemaining{ timeout };  // Used to represent the time budget of the sleep operation. Changes.
			auto start{ std::chrono::system_clock::now() };

			// The counter can change multiple times during our allocated timeout period, so we need to loop until we either timeout or successfully met expected value.
			while (true)
			{
				if (insideLock.Wait(&internalCapture, timeRemaining))
				{
					// Value changed, did not time out. Re-evaluate and potentially try again if we have time.

					if (Evaluate(expectedValue))
					{
						// Success.
						return true;
					}

					// Didn't meet the requirement, fall through.
				}

				// Timed out.
				else
				{
					break;
				}

				// Attempt to reschedule a sleep.
				auto remainder{ timeout - (std::chrono::system_clock::now() - start) };
				if (remainder.count() > 0)
				{
					// We have time for another, prepare for the upcoming sleep.

					internalCapture = internalValue.load();  // #TODO: Memory order.
					timeRemaining = std::chrono::round<decltype(timeRemaining)>(remainder);  // Update the time budget.
				}

				else
				{
					// Spent our time budget, fail out.
					return false;
				}
			}

			return false;
		}

		return true;
	}
}