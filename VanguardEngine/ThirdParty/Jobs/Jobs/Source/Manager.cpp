// Copyright (c) 2019 Andrew Depke

#include <Jobs/Manager.h>

#include <Jobs/Assert.h>
#include <Jobs/Logging.h>

#include <chrono>  // std::chrono

namespace Jobs
{
	void ManagerWorkerEntry(Manager* const Owner)
	{
		JOBS_ASSERT(Owner, "Manager thread entry missing owner.");

		// Spin until the manager is ready.
		while (!Owner->Ready.load(std::memory_order_acquire))[[unlikely]]
		{
			std::this_thread::yield();
		}

		auto& Representation{ Owner->Workers[Owner->GetThisThreadID()] };

		// We don't have a fiber at this point, so grab an available fiber.
		auto NextFiberIndex{ Owner->GetAvailableFiber() };
		JOBS_ASSERT(Owner->IsValidID(NextFiberIndex), "Failed to retrieve an available fiber from worker.");

		Representation.FiberIndex = NextFiberIndex;

		// Schedule the fiber, which executes the work. We will resume here when the manager is shutting down.
		Owner->Fibers[NextFiberIndex].first.Schedule(Owner->Workers[Owner->GetThisThreadID()].GetThreadFiber());

		JOBS_LOG(LogLevel::Log, "Worker Shutdown | ID: %i", Representation.GetID());

		// Kill ourselves. #TODO: This is a little dirty, we should fix this so that we perform the standard thread cleanup procedure and get a return code of 0.
		delete &Representation.GetThreadFiber();
	}

	void ManagerFiberEntry(void* Data)
	{
		JOBS_ASSERT(Data, "Manager fiber entry missing data.");

		auto FData = static_cast<FiberData*>(Data);

		while (FData->Owner->CanContinue())
		{
			// Cleanup an unfinished state from the previous fiber if we need to.
			auto& ThisFiber{ FData->Owner->Fibers[FData->Owner->Workers[FData->Owner->GetThisThreadID()].FiberIndex] };
			const auto PreviousFiberIndex{ ThisFiber.first.PreviousFiberIndex };
			if (FData->Owner->IsValidID(PreviousFiberIndex))
			{
				ThisFiber.first.PreviousFiberIndex = Manager::InvalidID;  // Reset.
				auto& PreviousFiber{ FData->Owner->Fibers[PreviousFiberIndex] };

				// We're back, first make sure we restore availability to the fiber that scheduled us or enqueue it in the wait pool.
				if (PreviousFiber.first.NeedsWaitEnqueue)
				{
					PreviousFiber.first.NeedsWaitEnqueue = false;  // Reset.
					FData->Owner->WaitingFibers.enqueue(PreviousFiberIndex);
				}

				else
				{
					PreviousFiber.second.store(true);  // #TODO: Memory order.
				}
			}

			const auto ThisThreadID{ FData->Owner->GetThisThreadID() };
			auto& ThisThread{ FData->Owner->Workers[ThisThreadID] };
			ThisFiber.first.WaitPoolPriority = !ThisFiber.first.WaitPoolPriority;  // Alternate wait pool priority.

			bool Continue = !ThisFiber.first.WaitPoolPriority || FData->Owner->WaitingFibers.size_approx() == 0;  // Used to favor jobs or waiters.

			if (Continue)
			{
				auto NewJob{ std::move(FData->Owner->Dequeue(ThisThreadID)) };
				if (NewJob)
				{
					Continue = false;  // We're satisfied, don't continue.

					// Loop until we satisfy all of our dependencies.
					bool RequiresEvaluation = true;
					while (RequiresEvaluation)
					{
						RequiresEvaluation = false;

						for (const auto& Dependency : NewJob->Dependencies)
						{
							if (auto StrongDependency{ Dependency.first.lock() }; !StrongDependency->UnsafeWait(Dependency.second, std::chrono::milliseconds{ 1 }))
							{
								// This dependency timed out, move ourselves to the wait pool.
								JOBS_LOG(LogLevel::Log, "Job dependencies timed out, moving to the wait pool.");

								auto NextFiberIndex{ FData->Owner->GetAvailableFiber() };
								JOBS_ASSERT(FData->Owner->IsValidID(NextFiberIndex), "Failed to retrieve an available fiber from waiting fiber.");

								auto& ThisThreadLocal = FData->Owner->Workers[FData->Owner->GetThisThreadID()];  // We might resume on any worker, so we need to update this each iteration.

								ThisFiber.first.NeedsWaitEnqueue = true;  // We are waiting on a dependency, so make sure we get added to the wait pool.
								FData->Owner->Fibers[NextFiberIndex].first.PreviousFiberIndex = ThisThreadLocal.FiberIndex;
								ThisThreadLocal.FiberIndex = NextFiberIndex;  // Update the fiber index.
								FData->Owner->Fibers[NextFiberIndex].first.Schedule(FData->Owner->Fibers[ThisThread.FiberIndex].first);

								JOBS_LOG(LogLevel::Log, "Job resumed from wait pool, re-evaluating dependencies.");

								// Next we can re-evaluate the dependencies.
								RequiresEvaluation = true;
							}
						}
					}

					if (NewJob->Stream) [[unlikely]]
					{
						(*NewJob)(FData->Owner);
					}

					else
					{
						(static_cast<Job>(*NewJob))();  // Slice.
					}

					// Finished, notify the counter if we have one. Handles expired counters (cleanup jobs) fine.
					if (auto StrongCounter{ NewJob->AtomicCounter.lock() })
					{
						StrongCounter->operator--();
					}
				}
			}

			if (Continue)
			{
				size_t WaitingFiberIndex = Manager::InvalidID;
				if (FData->Owner->WaitingFibers.try_dequeue(WaitingFiberIndex))
				{
					Continue = false;  // Satisfied, don't continue.

					auto& WaitingFiber{ FData->Owner->Fibers[WaitingFiberIndex].first };

					JOBS_ASSERT(!ThisFiber.first.NeedsWaitEnqueue, "Logic error, should never request an enqueue if we pulled down a fiber through a dequeue.");

					WaitingFiber.PreviousFiberIndex = ThisThread.FiberIndex;
					ThisThread.FiberIndex = WaitingFiberIndex;
					WaitingFiber.Schedule(ThisFiber.first);  // Schedule the waiting fiber. It might be a long time before we resume, if ever.
				}
			}

			if (Continue)
			{
				JOBS_LOG(LogLevel::Log, "Fiber sleeping.");

				FData->Owner->QueueCV.Lock();

				// Test the shutdown condition once more under lock, as it could've been set during the transitional period.
				if (!FData->Owner->CanContinue())
				{
					FData->Owner->QueueCV.Unlock();

					break;
				}

				FData->Owner->QueueCV.Wait();  // We will be woken up either by a shutdown event or if new work is available.
				FData->Owner->QueueCV.Unlock();
			}
		}

		// End of fiber lifetime, we are switching out to the worker thread to perform any final cleanup. We cannot be scheduled again beyond this point.
		auto& ActiveWorker{ FData->Owner->Workers[FData->Owner->GetThisThreadID()] };

		ActiveWorker.GetThreadFiber().Schedule(FData->Owner->Fibers[ActiveWorker.FiberIndex].first);

		JOBS_ASSERT(false, "Dead fiber was rescheduled.");
	}

	Manager::~Manager()
	{
		QueueCV.Lock();  // This is used to make sure we don't let any fibers slip by and not catch the shutdown notify, which causes a deadlock.
		Shutdown.store(true, std::memory_order_seq_cst);

		QueueCV.NotifyAll();  // Wake all sleepers, it's time to shutdown.
		QueueCV.Unlock();

		// Wait for all of the workers to die before deleting the fiber data.
		for (auto& Worker : Workers)
		{
			Worker.GetHandle().join();
		}
	}

	void Manager::Initialize(size_t ThreadCount)
	{
		JOBS_ASSERT(ThreadCount <= std::thread::hardware_concurrency(), "Job manager thread count should not exceed hardware concurrency.");

		Data = std::move(std::make_unique<FiberData>(this));

		for (auto Iter = 0; Iter < FiberCount; ++Iter)
		{
			Fibers[Iter].first = std::move(Fiber{ FiberStackSize, &ManagerFiberEntry, Data.get() });
			Fibers[Iter].second.store(true);  // #TODO: Memory order.
		}

		if (ThreadCount == 0)
		{
			ThreadCount = std::thread::hardware_concurrency();
		}

		Workers.reserve(ThreadCount);

		for (size_t Iter = 0; Iter < ThreadCount; ++Iter)
		{
			Worker NewWorker{ this, Iter, &ManagerWorkerEntry };

			// Fix for a rare data race when the swap occurs while the worker is saving the fiber pointer.
			while (!NewWorker.IsReady())[[unlikely]]
			{
				// Should almost never end up spinning here.
				std::this_thread::yield();
			}

			Workers.push_back(std::move(NewWorker));
		}

		Shutdown.store(false, std::memory_order_relaxed);  // This must be set before we are ready.
		Ready.store(true, std::memory_order_release);  // This must be set last.
	}

	std::optional<JobBuilder> Manager::Dequeue(size_t ThreadID)
	{
		JobBuilder Result{};

		if (Workers[ThreadID].GetJobQueue().try_dequeue(Result))
		{
			return Result;
		}

		else
		{
			// Our queue is empty, time to steal.
			// #TODO: Implement a smart stealing algorithm.

			for (auto Iter = 1; Iter < Workers.size(); ++Iter)
			{
				if (Workers[(Iter + ThreadID) % Workers.size()].GetJobQueue().try_dequeue(Result))
				{
					return Result;
				}
			}
		}

		return std::nullopt;
	}

	size_t Manager::GetThisThreadID() const
	{
		auto ThisID{ std::this_thread::get_id() };

		for (auto& Worker : Workers)
		{
			if (Worker.GetNativeID() == ThisID)
			{
				return Worker.GetID();
			}
		}

		return InvalidID;
	}

	size_t Manager::GetAvailableFiber()
	{
		for (auto Index = 0; Index < Fibers.size(); ++Index)
		{
			auto Expected{ true };
			if (Fibers[Index].second.compare_exchange_weak(Expected, false, std::memory_order_acquire))
			{
				return Index;
			}
		}

		JOBS_LOG(LogLevel::Error, "No free fibers!");

		return InvalidID;
	}
}