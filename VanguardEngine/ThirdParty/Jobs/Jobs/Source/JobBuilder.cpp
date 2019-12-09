// Copyright (c) 2019 Andrew Depke

#include <Jobs/JobBuilder.h>

#include <Jobs/Manager.h>

namespace Jobs
{
	void JobBuilder::operator()(Manager* InOwner)
	{
		// Execute our actual job before anything else, cache will probably be wiped out by the time we're back.
		(*Entry)(Data);

		for (size_t Iter{ 0 }; Iter < JobTree->size(); ++Iter)
		{
			for (auto& NextJob : (*JobTree)[Iter].first)
			{
				if (Iter > 0)
				{
					NextJob.AddDependency((*JobTree)[Iter - 1].second);
				}

				InOwner->Enqueue(std::move(NextJob), (*JobTree)[Iter].second);  // Increments the counter prior to the depending jobs' enqueue.
			}
		}

		auto LockedCounter{ std::prev(JobTree->end())->second };  // We hold the heap resource keeping this counter alive, it will persist until the cleanup job runs.
		LockedCounter->operator--();  // We queued up the jobs so the counter is guaranteed to be ready.

		auto CleanupJob{ Job{ [](auto Payload)
		{
			delete reinterpret_cast<TreeType*>(Payload);
		}, static_cast<void*>(JobTree) } };
		CleanupJob.AddDependency(LockedCounter, 0);

		InOwner->Enqueue(std::move(CleanupJob));  // This trailing job will cleanup the heap resources allocated from the job builder after every job has ran.
	}
}