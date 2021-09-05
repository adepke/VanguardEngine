// Copyright (c) 2019-2021 Andrew Depke

#include <Jobs/Manager.h>

#include <Jobs/FiberMutex.h>
#include <Jobs/Assert.h>
#include <Jobs/Logging.h>

#include <chrono>  // std::chrono

namespace Jobs
{
	void ManagerWorkerEntry(void* data)
	{
		auto* owner = reinterpret_cast<Manager*>(data);

		JOBS_ASSERT(owner, "Manager thread entry missing owner.");

		// Spin until the manager is ready.
		while (!owner->ready.load(std::memory_order_acquire))[[unlikely]]
		{
			std::this_thread::yield();
		}

		auto& representation{ owner->workers[owner->GetThisThreadID()] };

		// We don't have a fiber at this point, so grab an available fiber.
		auto nextFiberIndex{ owner->GetAvailableFiber() };
		JOBS_ASSERT(owner->IsValidID(nextFiberIndex), "Failed to retrieve an available fiber from worker.");

		auto& nextFiber = owner->fibers[nextFiberIndex];
		representation.fiberIndex = nextFiberIndex;  // Update the fiber index.
		nextFiber.first.Schedule(representation.GetThreadFiber());

		JOBS_LOG(LogLevel::Log, "Worker Shutdown | ID: %i", representation.GetID());

		// Exit the thread, this will not return to the host thread, but instead perform standard thread exit procedure.
	}

	void ManagerFiberEntry(void* data)
	{
		JOBS_ASSERT(data, "Manager fiber entry missing data.");

		auto* owner = reinterpret_cast<Manager*>(data);

		while (owner->CanContinue())
		{
			const auto thisThreadID = owner->GetThisThreadID();

			// Cleanup any unfinished state from the previous fiber if we need to.
			auto& thisFiber{ owner->fibers[owner->workers[thisThreadID].fiberIndex] };
			auto previousFiberIndex{ thisFiber.first.previousFiberIndex };
			if (owner->IsValidID(previousFiberIndex))
			{
				thisFiber.first.previousFiberIndex = Manager::invalidID;  // Reset.
				auto& previousFiber{ owner->fibers[previousFiberIndex] };

				// Next make sure we restore availability to the fiber that scheduled us or enqueue it in the wait pool.
				if (previousFiber.first.needsWaitEnqueue)
				{
					previousFiber.first.needsWaitEnqueue = false;  // Reset.
					owner->waitingFibers.enqueue(previousFiberIndex);
				}

				else
				{
					previousFiber.second.store(true);  // #TODO: Memory order.
				}
			}

			auto& thisThread{ owner->workers[thisThreadID] };
			thisFiber.first.waitPoolPriority = !thisFiber.first.waitPoolPriority;  // Alternate wait pool priority.

			bool shouldContinue = !thisFiber.first.waitPoolPriority || owner->waitingFibers.size_approx() == 0;  // Used to favor jobs or waiters.

			if (shouldContinue)
			{
				auto newJob{ std::move(owner->Dequeue(thisThreadID)) };
				if (newJob)
				{
					shouldContinue = false;  // We're satisfied, don't continue.

					// Loop until we satisfy all of our dependencies.
					bool requiresEvaluation = true;
					while (requiresEvaluation)
					{
						requiresEvaluation = false;

						for (const auto& dependency : newJob->dependencies)
						{
							if (auto strongDependency{ dependency.first.lock() }; !strongDependency->UnsafeWait(dependency.second, std::chrono::milliseconds{ 1 }))
							{
								// This dependency timed out, move ourselves to the wait pool.
								JOBS_LOG(LogLevel::Log, "Job dependencies timed out, moving to the wait pool.");

								auto nextFiberIndex{ owner->GetAvailableFiber() };
								JOBS_ASSERT(owner->IsValidID(nextFiberIndex), "Failed to retrieve an available fiber from waiting fiber.");
								auto& nextFiber = owner->fibers[nextFiberIndex].first;

								auto& thisNewThread = owner->workers[owner->GetThisThreadID()];  // We might resume on any worker, so we need to update this each iteration.

								thisFiber.first.needsWaitEnqueue = true;  // We are waiting on a dependency, so make sure we get added to the wait pool.
								nextFiber.previousFiberIndex = thisNewThread.fiberIndex;
								thisNewThread.fiberIndex = nextFiberIndex;  // Update the fiber index.
								nextFiber.Schedule(thisFiber.first);

								JOBS_LOG(LogLevel::Log, "Job resumed from wait pool, re-evaluating dependencies.");

								// We just returned from a fiber, so we need to make sure to fix up its state. We can't wait until the main loop begins again
								// because if any of the dependencies still hold, we lose that information about the previous fiber, causing a leak.

								previousFiberIndex = thisFiber.first.previousFiberIndex;
								if (owner->IsValidID(previousFiberIndex))
								{
									thisFiber.first.previousFiberIndex = Manager::invalidID;  // Reset. Skipping this will cause a double-cleanup on the next loop beginning if the dependency doesn't hold.
									auto& previousFiber{ owner->fibers[previousFiberIndex] };

									if (previousFiber.first.needsWaitEnqueue)
									{
										previousFiber.first.needsWaitEnqueue = false;  // Reset.
										owner->waitingFibers.enqueue(previousFiberIndex);
									}

									else
									{
										previousFiber.second.store(true);  // #TODO: Memory order.
									}
								}

								// Next we can re-evaluate the dependencies.
								requiresEvaluation = true;
							}
						}
					}

					if (newJob->stream) [[unlikely]]
					{
						(*newJob)(owner);
					}

					else
					{
						(static_cast<Job>(*newJob))(owner);  // Slice.
					}

					// Finished, notify the counter if we have one. Handles expired counters (cleanup jobs) fine.
					if (auto strongCounter{ newJob->atomicCounter.lock() })
					{
						strongCounter->operator--();
					}
				}
			}

			if (shouldContinue)
			{
				size_t waitingFiberIndex = Manager::invalidID;
				if (owner->waitingFibers.try_dequeue(waitingFiberIndex))
				{
					auto& waitingFiber{ owner->fibers[waitingFiberIndex].first };

					JOBS_ASSERT(!thisFiber.first.needsWaitEnqueue, "Logic error, should never request an enqueue if we pulled down a fiber through a dequeue.");

					// Before we schedule the waiting fiber, we need to check if it's waiting on a mutex.
					if (waitingFiber.mutex && !waitingFiber.mutex->try_lock())
					{
						JOBS_LOG(LogLevel::Log, "Waiting fiber failed to acquire mutex.");

						// Move the waiting fiber to the back of the wait queue. We don't need to mark either fiber as needing a wait enqueue
						// since we never left this fiber.
						owner->waitingFibers.enqueue(waitingFiberIndex);

						// Fall through and continue on this fiber.
					}

					else
					{
						shouldContinue = false;  // Satisfied, don't continue.

						waitingFiber.previousFiberIndex = thisThread.fiberIndex;
						thisThread.fiberIndex = waitingFiberIndex;
						waitingFiber.Schedule(thisFiber.first);  // Schedule the waiting fiber. We're not a waiter, so we'll be marked as available.
					}
				}
			}

			if (shouldContinue)
			{
				JOBS_LOG(LogLevel::Log, "Fiber sleeping.");

				owner->queueCV.Lock();

				// Test the shutdown condition once more under lock, as it could've been set during the transitional period.
				if (!owner->CanContinue())
				{
					owner->queueCV.Unlock();

					break;
				}

				owner->queueCV.Wait();  // We will be woken up either by a shutdown event or if new work is available.
				owner->queueCV.Unlock();
			}
		}

		const auto& thisWorker = owner->workers[owner->GetThisThreadID()];

		// End of fiber lifetime, we are switching out to the worker thread to perform any final cleanup. We cannot be scheduled again beyond this point.
		thisWorker.GetThreadFiber().Schedule(owner->fibers[thisWorker.fiberIndex].first);

		JOBS_ASSERT(false, "Dead fiber was rescheduled.");
	}

	Manager::~Manager()
	{
		queueCV.Lock();  // This is used to make sure we don't let any fibers slip by and not catch the shutdown notify, which causes a deadlock.
		shutdown.store(true, std::memory_order_seq_cst);

		queueCV.NotifyAll();  // Wake all sleepers, it's time to shutdown.
		queueCV.Unlock();

		// Wait for all of the workers to die before deleting the fiber data.
		for (auto& worker : workers)
		{
			worker.GetHandle().join();
		}
	}

	void Manager::Initialize(size_t threadCount)
	{
		JOBS_ASSERT(threadCount <= std::thread::hardware_concurrency(), "Job manager thread count should not exceed hardware concurrency.");

		for (auto iter = 0; iter < fiberCount; ++iter)
		{
			fibers[iter].first = std::move(Fiber{ fiberStackSize, &ManagerFiberEntry, this });
			fibers[iter].second.store(true);  // #TODO: Memory order.
		}

		if (threadCount == 0)
		{
			threadCount = std::thread::hardware_concurrency();
		}

		workers.reserve(threadCount);

		for (size_t iter = 0; iter < threadCount; ++iter)
		{
			workers.emplace_back(this, iter, &ManagerWorkerEntry);
		}

		shutdown.store(false, std::memory_order_relaxed);  // This must be set before we are ready.
		ready.store(true, std::memory_order_release);  // This must be set last.
	}

	std::optional<JobBuilder> Manager::Dequeue(size_t threadID)
	{
		JobBuilder result{};

		if (workers[threadID].GetJobQueue().try_dequeue(result))
		{
			return result;
		}

		else
		{
			// Our queue is empty, time to steal.
			// #TODO: Implement a smart stealing algorithm.

			for (auto iter = 1; iter < workers.size(); ++iter)
			{
				if (workers[(iter + threadID) % workers.size()].GetJobQueue().try_dequeue(result))
				{
					return result;
				}
			}
		}

		return std::nullopt;
	}

	size_t Manager::GetThisThreadID() const
	{
		auto thisID{ std::this_thread::get_id() };

		for (auto& worker : workers)
		{
			if (worker.GetNativeID() == thisID)
			{
				return worker.GetID();
			}
		}

		return invalidID;
	}

	size_t Manager::GetAvailableFiber()
	{
		for (auto index = 0; index < fibers.size(); ++index)
		{
			auto expected{ true };
			if (fibers[index].second.compare_exchange_weak(expected, false, std::memory_order_acquire))
			{
				return index;
			}
		}

		JOBS_LOG(LogLevel::Error, "No free fibers!");

		return invalidID;
	}
}