// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Jobs/Manager.h>

namespace Jobs
{
	namespace Detail
	{
		struct Empty;  // Forward declare, we need it in the algorithm payload.
	}

	template <typename... Ts>
	struct AlgorithmPayload;

	template <typename Container>
	struct AlgorithmPayload<Container>
	{
		decltype(std::declval<Container>().begin()) iterator;
	};

	template <typename Container>
	struct AlgorithmPayload<Container, Detail::Empty>
	{
		decltype(std::declval<Container>().begin()) iterator;
	};

	template <typename Container, typename CustomData>
	struct AlgorithmPayload<Container, CustomData>
	{
		decltype(std::declval<Container>().begin()) iterator;
		CustomData data;
	};

	namespace Detail
	{
		template <typename Iterator>
		struct FakeContainer
		{
			Iterator begin() const;
		};

		struct Empty {};

		template <typename Async, typename Iterator, typename CustomData, typename Function>
		void ParallelForInternal(Manager& manager, Iterator first, Iterator last, CustomData&& data, Function&& function)
		{
			std::shared_ptr<Counter<>> dependency;

			if constexpr (std::is_same_v<Async, std::false_type>)
			{
				dependency = std::make_shared<Counter<>>(0);
			}

			std::vector<AlgorithmPayload<Detail::FakeContainer<Iterator>, std::remove_reference_t<CustomData>>> payloads;
			payloads.resize(std::distance(first, last));
			// #TODO: Maybe we don't actually need this? Look into it.
			static_assert(std::is_trivially_copyable_v<AlgorithmPayload<Detail::FakeContainer<Iterator>, std::remove_reference_t<CustomData>>>, "AlgorithmPayload must be trivially copyable");

			if constexpr (!std::is_same_v<std::remove_reference_t<CustomData>, Detail::Empty>)
			{
				std::fill(payloads.begin(), payloads.end(), AlgorithmPayload<Detail::FakeContainer<Iterator>, std::remove_reference_t<CustomData>>{ first, data });
			}

			const auto cachedFirst = first;

			for (; first != last; ++first)
			{
				const auto index = std::distance(cachedFirst, first);
				payloads[index].Iterator = first;

				if constexpr (std::is_same_v<Async, std::true_type>)
				{
					manager.Enqueue(Job{ function, const_cast<void*>(static_cast<const void*>(&payloads[index])) });
				}

				else
				{
					manager.Enqueue(Job{ function, const_cast<void*>(static_cast<const void*>(&payloads[index])) }, dependency);
				}
			}

			if constexpr (std::is_same_v<Async, std::false_type>)
			{
				dependency->Wait(0);
			}
		}
	}

	template <typename Iterator, typename CustomData, typename Function>
	inline void ParallelFor(Manager& manager, Iterator first, Iterator last, CustomData&& data, Function&& function)
	{
		Detail::ParallelForInternal<std::false_type>(manager, first, last, std::forward<CustomData>(data), std::forward<Function>(function));
	}

	template <typename Iterator, typename Function>
	inline void ParallelFor(Manager& manager, Iterator first, Iterator last, Function&& function)
	{
		Detail::ParallelForInternal<std::false_type>(manager, first, last, Detail::Empty{}, std::forward<Function>(function));
	}

	template <typename Container, typename CustomData, typename Function>
	inline void ParallelFor(Manager& manager, Container&& container, CustomData&& data, Function&& function)
	{
		Detail::ParallelForInternal<std::false_type>(manager, std::begin(container), std::end(container), std::forward<CustomData>(data), std::forward<Function>(function));
	}

	template <typename Container, typename Function>
	inline void ParallelFor(Manager& manager, Container&& container, Function&& function)
	{
		Detail::ParallelForInternal<std::false_type>(manager, std::begin(container), std::end(container), Detail::Empty{}, std::forward<Function>(function));
	}

	template <typename Iterator, typename CustomData, typename Function>
	inline void ParallelForAsync(Manager& manager, Iterator first, Iterator last, CustomData&& data, Function&& function)
	{
		Detail::ParallelForInternal<std::true_type>(manager, first, last, std::forward<CustomData>(data), std::forward<Function>(function));
	}
	
	template <typename Iterator, typename Function>
	inline void ParallelForAsync(Manager& manager, Iterator first, Iterator last, Function&& function)
	{
		Detail::ParallelForInternal<std::true_type>(manager, first, last, Detail::Empty{}, std::forward<Function>(function));
	}

	template <typename Container, typename CustomData, typename Function>
	inline void ParallelForAsync(Manager& manager, Container&& container, CustomData&& data, Function&& function)
	{
		Detail::ParallelForInternal<std::true_type>(manager, std::begin(container), std::end(container), std::forward<CustomData>(data), std::forward<Function>(function));
	}

	template <typename Container, typename Function>
	inline void ParallelForAsync(Manager& manager, Container&& container, Function&& function)
	{
		Detail::ParallelForInternal<std::true_type>(manager, std::begin(container), std::end(container), Detail::Empty{}, std::forward<Function>(function));
	}

	namespace Detail
	{
		struct NoOp
		{
			template <typename T>
			auto operator()(T item) const { return item; }
		};

		template <typename Iterator, typename UnaryOp, typename BinaryOp, typename OutIterator>
		struct MapReducePayload
		{
			Iterator input;
			size_t count;
			UnaryOp mapOperation;
			BinaryOp reduceOperation;
			OutIterator output;
		};

		template <typename Iterator, typename UnaryOp, typename BinaryOp>
		auto ParallelMapReduceInternal(Manager& manager, Iterator first, Iterator last, UnaryOp&& mapOperation, BinaryOp&& reduceOperation)
		{
			using IntermediateType = decltype(mapOperation(*first));
			using ResultType = decltype(reduceOperation(std::declval<IntermediateType>(), std::declval<IntermediateType>()));
			using ResultContainerType = std::vector<ResultType>;
			using PayloadType = MapReducePayload<Iterator, std::remove_reference_t<UnaryOp>, std::remove_reference_t<BinaryOp>, decltype(std::declval<ResultContainerType>().begin())>;

			auto dependency{ std::make_shared<Counter<>>(0) };

			const auto distance = std::distance(first, last);  // Data set size.

			// If we have a relatively small data set, serial execution is probably faster.
			if (distance < 1000)
			{
				if constexpr (std::is_same_v<std::remove_reference_t<UnaryOp>, NoOp>)
				{
					return std::reduce(first, last, ResultType{}, std::forward<BinaryOp>(reduceOperation));
				}

				else
				{
					return std::transform_reduce(first, last, ResultType{}, std::forward<BinaryOp>(reduceOperation), std::forward<UnaryOp>(mapOperation));
				}
			}

			const auto jobCount = manager.GetWorkerCount();  // Number of jobs, excluding combiner.
			const auto payloadSize = distance / jobCount;  // Input data chunk size per job.
			const size_t remainder = distance % payloadSize;  // Left over data chunk for the last job.

			ResultContainerType results;
			results.resize(jobCount);  // Each job produces an intermediate result.
			std::vector<PayloadType> payloads;
			payloads.resize(jobCount);  // Each job needs a payload as well.

			// Separate the loops in order to maintain cache locality.
			for (size_t iter{ 0 }; iter < jobCount; ++iter)
			{
				payloads[iter].input = first + (iter * payloadSize);
				payloads[iter].count = payloadSize;

				if constexpr (!std::is_same_v<std::remove_reference_t<UnaryOp>, NoOp>)
				{
					payloads[iter].mapOperation = mapOperation;
				}

				payloads[iter].reduceOperation = reduceOperation;
				payloads[iter].output = results.begin() + iter;
			}

			// Fix the remainder.
			payloads[jobCount - 1].count += remainder;
			
			for (size_t iter{ 0 }; iter < jobCount; ++iter)
			{
				manager.Enqueue(Job{ [](auto payload)
					{
						auto* typedPayload = reinterpret_cast<PayloadType*>(payload);
						
						if constexpr (std::is_same_v<std::remove_reference_t<UnaryOp>, NoOp>)
						{
							*typedPayload->output = std::reduce(typedPayload->input, typedPayload->input + typedPayload->count, ResultType{}, typedPayload->reduceOperation);
						}

						else
						{
							*typedPayload->output = std::transform_reduce(typedPayload->input, typedPayload->input + typedPayload->count, ResultType{}, typedPayload->reduceOperation, typedPayload->mapOperation);
						}
					}, static_cast<void*>(&payloads[iter]) }, dependency);
			}

			dependency->Wait(0);

			return std::accumulate(results.begin(), results.end(), ResultType{});
		}
	}

	template <typename Iterator, typename UnaryOp, typename BinaryOp>
	inline auto ParallelMapReduce(Manager& manager, Iterator first, Iterator last, UnaryOp&& mapOperation, BinaryOp&& reduceOperation)
	{
		return Detail::ParallelMapReduceInternal(manager, first, last, std::forward<UnaryOp>(mapOperation), std::forward<BinaryOp>(reduceOperation));
	}

	template <typename Container, typename UnaryOp, typename BinaryOp>
	inline auto ParallelMapReduce(Manager& manager, Container&& container, UnaryOp&& mapOperation, BinaryOp&& reduceOperation)
	{
		return Detail::ParallelMapReduceInternal(manager, std::begin(container), std::end(container), std::forward<UnaryOp>(mapOperation), std::forward<BinaryOp>(reduceOperation));
	}

	template <typename Iterator, typename BinaryOp>
	inline auto ParallelReduce(Manager& manager, Iterator first, Iterator last, BinaryOp&& reduceOperation)
	{
		return Detail::ParallelMapReduceInternal(manager, first, last, Detail::NoOp{}, std::forward<BinaryOp>(reduceOperation));
	}

	template <typename Container, typename BinaryOp>
	inline auto ParallelReduce(Manager& manager, Container&& container, BinaryOp&& reduceOperation)
	{
		return Detail::ParallelMapReduceInternal(manager, std::begin(container), std::end(container), Detail::NoOp{}, std::forward<BinaryOp>(reduceOperation));
	}
}