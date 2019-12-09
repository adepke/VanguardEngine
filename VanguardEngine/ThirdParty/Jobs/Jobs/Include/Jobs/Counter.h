// Copyright (c) 2019 Andrew Depke

#pragma once

#include <atomic>  // std::atomic
#include <mutex>  // std::mutex
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
		std::atomic<T> Internal;
		Futex InsideLock;  // Timed unsafe signaling for jobs.
		FutexConditionVariable OutsideLock;  // Blind spot safe signaling for non-worker threads.

		bool Evaluate(const T& ExpectedValue) const
		{
			return Internal.load() <= ExpectedValue;  // #TODO: Memory order.
		}

	public:
		Counter();
		Counter(T InitialValue);
		Counter(const Counter&) = delete;
		Counter(Counter&& Other) noexcept = delete;  // #TODO: Implement.
		~Counter() = default;

		Counter& operator=(const Counter&) = delete;
		Counter& operator=(Counter&&) noexcept = delete;  // #TODO: Implement.

		Counter& operator++();
		Counter& operator--();

		Counter& operator+=(T Target);

		// Atomically fetch the current value.
		const T Get() const;

		// Blocking operation.
		void Wait(T ExpectedValue);

		// Blocking operation.
		template <typename Rep, typename Period>
		bool WaitFor(T ExpectedValue, const std::chrono::duration<Rep, Period>& Timeout);

	private:
		// Futex based blocking, reserved for jobs. Susceptible to blind spot signaling.
		template <typename Rep, typename Period>
		bool UnsafeWait(T ExpectedValue, const std::chrono::duration<Rep, Period>& Timeout);
	};

	template <typename T>
	Counter<T>::Counter() : Internal(0) {}

	template <typename T>
	Counter<T>::Counter(T InitialValue) : Internal(InitialValue) {}

	template <typename T>
	Counter<T>& Counter<T>::operator++()
	{
		++Internal;

		// Don't notify.

		return *this;
	}

	template <typename T>
	Counter<T>& Counter<T>::operator--()
	{
		--Internal;

		// Notify waiting jobs.
		InsideLock.NotifyAll();  // We don't notify under lock since a blind spot signal isn't fatal, it will only cost us the timeout period.

		// Notify waiting outsiders.
		OutsideLock.Lock();
		OutsideLock.NotifyAll();  // Notify under lock to prevent a blind spot signal, which can be fatal.
		OutsideLock.Unlock();

		return *this;
	}

	template <typename T>
	Counter<T>& Counter<T>::operator+=(T Target)
	{
		Internal += Target;

		// Don't notify.

		return *this;
	}

	template <typename T>
	const T Counter<T>::Get() const
	{
		return Internal.load();  // #TODO: Memory order.
	}

	template <typename T>
	void Counter<T>::Wait(T ExpectedValue)
	{
		OutsideLock.Lock();

		while (!Evaluate(ExpectedValue))
		{
			OutsideLock.Wait();
		}

		OutsideLock.Unlock();
	}

	template <typename T>
	template <typename Rep, typename Period>
	bool Counter<T>::WaitFor(T ExpectedValue, const std::chrono::duration<Rep, Period>& Timeout)
	{
		auto Start{ std::chrono::system_clock::now() };

		while (!Evaluate(ExpectedValue))
		{
			OutsideLock.Lock();
			bool Result{ OutsideLock.WaitFor(Timeout) };
			OutsideLock.Unlock();

			if (!Result || Start + std::chrono::system_clock::now() >= Timeout)
			{
				return false;
			}
		}

		return true;
	}

	template <typename T>
	template <typename Rep, typename Period>
	bool Counter<T>::UnsafeWait(T ExpectedValue, const std::chrono::duration<Rep, Period>& Timeout)
	{
		// InternalCapture is the saved state of Internal at the time of sleeping. We will use this to know if it changed.
		if (auto InternalCapture{ Internal.load() }; InternalCapture != ExpectedValue)  // #TODO: Memory order.
		{
			InsideLock.Set(&Internal);

			auto TimeRemaining{ Timeout };  // Used to represent the time budget of the sleep operation. Changes.
			auto Start{ std::chrono::system_clock::now() };

			// The counter can change multiple times during our allocated timeout period, so we need to loop until we either timeout or successfully met expected value.
			while (true)
			{
				if (InsideLock.Wait(&InternalCapture, TimeRemaining))
				{
					// Value changed, did not time out. Re-evaluate and potentially try again if we have time.

					if (Evaluate(ExpectedValue))
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
				auto Remainder{ Timeout - (std::chrono::system_clock::now() - Start) };
				if (Remainder.count() > 0)
				{
					// We have time for another, prepare for the upcoming sleep.

					InternalCapture = Internal.load();  // #TODO: Memory order.
					TimeRemaining = std::chrono::round<decltype(TimeRemaining)>(Remainder);  // Update the time budget.
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