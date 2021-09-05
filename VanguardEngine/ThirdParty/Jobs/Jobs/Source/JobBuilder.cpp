// Copyright (c) 2019-2021 Andrew Depke

#include <Jobs/JobBuilder.h>

#include <Jobs/Manager.h>

namespace Jobs
{
	void JobBuilder::operator()(Manager* owner)
	{
		// Execute our actual job before anything else, cache will probably be wiped out by the time we're back.
		(*entry)(owner, data);

		for (size_t iter{ 0 }; iter < jobTree->size(); ++iter)
		{
			for (auto& nextJob : (*jobTree)[iter].first)
			{
				if (iter > 0)
				{
					nextJob.AddDependency((*jobTree)[iter - 1].second);
				}

				owner->Enqueue(std::move(nextJob), (*jobTree)[iter].second);  // Increments the counter prior to the depending jobs' enqueue.
			}
		}

		auto lockedCounter{ std::prev(jobTree->end())->second };  // We hold the heap resource keeping this counter alive, it will persist until the cleanup job runs.
		lockedCounter->operator--();  // We queued up the jobs so the counter is guaranteed to be ready.

		auto cleanupJob{ Job{ [](auto, auto payload)
		{
			delete reinterpret_cast<TreeType*>(payload);
		}, static_cast<void*>(jobTree) } };
		cleanupJob.AddDependency(lockedCounter, 0);

		owner->Enqueue(std::move(cleanupJob));  // This trailing job will cleanup the heap resources allocated from the job builder after every job has ran.
	}
}