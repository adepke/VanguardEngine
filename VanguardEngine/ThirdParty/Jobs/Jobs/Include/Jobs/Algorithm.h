// Copyright (c) 2019 Andrew Depke

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
		decltype(std::declval<Container>().begin()) Iterator;
	};

	template <typename Container>
	struct AlgorithmPayload<Container, Detail::Empty>
	{
		decltype(std::declval<Container>().begin()) Iterator;
	};

	template <typename Container, typename CustomData>
	struct AlgorithmPayload<Container, CustomData>
	{
		decltype(std::declval<Container>().begin()) Iterator;
		CustomData Data;
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
		void ParallelForInternal(Manager& InManager, Iterator First, Iterator Last, CustomData&& Data, Function&& InFunction)
		{
			std::shared_ptr<Counter<>> Dependency;

			if constexpr (std::is_same_v<Async, std::false_type>)
			{
				Dependency = std::make_shared<Counter<>>(0);
			}

			std::vector<AlgorithmPayload<Detail::FakeContainer<Iterator>, std::remove_reference_t<CustomData>>> Payloads;
			Payloads.resize(std::distance(First, Last));
			// #TODO: Maybe we don't actually need this? Look into it.
			static_assert(std::is_trivially_copyable_v<AlgorithmPayload<Detail::FakeContainer<Iterator>, std::remove_reference_t<CustomData>>>, "AlgorithmPayload must be trivially copyable");

			if constexpr (!std::is_same_v<std::remove_reference_t<CustomData>, Detail::Empty>)
			{
				std::fill(Payloads.begin(), Payloads.end(), AlgorithmPayload<Detail::FakeContainer<Iterator>, std::remove_reference_t<CustomData>>{ First, Data });
			}

			const auto CachedFirst = First;

			for (; First != Last; ++First)
			{
				const auto Index = std::distance(CachedFirst, First);
				Payloads[Index].Iterator = First;

				if constexpr (std::is_same_v<Async, std::true_type>)
				{
					InManager.Enqueue(Job{ InFunction, const_cast<void*>(static_cast<const void*>(&Payloads[Index])) });
				}

				else
				{
					InManager.Enqueue(Job{ InFunction, const_cast<void*>(static_cast<const void*>(&Payloads[Index])) }, Dependency);
				}
			}

			if constexpr (std::is_same_v<Async, std::false_type>)
			{
				Dependency->Wait(0);
			}
		}
	}

	template <typename Iterator, typename CustomData, typename Function>
	inline void ParallelFor(Manager& InManager, Iterator First, Iterator Last, CustomData&& Data, Function&& InFunction)
	{
		Detail::ParallelForInternal<std::false_type>(InManager, First, Last, std::forward<CustomData>(Data), std::forward<Function>(InFunction));
	}

	template <typename Iterator, typename Function>
	inline void ParallelFor(Manager& InManager, Iterator First, Iterator Last, Function&& InFunction)
	{
		Detail::ParallelForInternal<std::false_type>(InManager, First, Last, Detail::Empty{}, std::forward<Function>(InFunction));
	}

	template <typename Container, typename CustomData, typename Function>
	inline void ParallelFor(Manager& InManager, Container&& InContainer, CustomData&& Data, Function&& InFunction)
	{
		Detail::ParallelForInternal<std::false_type>(InManager, std::begin(InContainer), std::end(InContainer), std::forward<CustomData>(Data), std::forward<Function>(InFunction));
	}

	template <typename Container, typename Function>
	inline void ParallelFor(Manager& InManager, Container&& InContainer, Function&& InFunction)
	{
		Detail::ParallelForInternal<std::false_type>(InManager, std::begin(InContainer), std::end(InContainer), Detail::Empty{}, std::forward<Function>(InFunction));
	}

	template <typename Iterator, typename CustomData, typename Function>
	inline void ParallelForAsync(Manager& InManager, Iterator First, Iterator Last, CustomData&& Data, Function&& InFunction)
	{
		Detail::ParallelForInternal<std::true_type>(InManager, First, Last, std::forward<CustomData>(Data), std::forward<Function>(InFunction));
	}
	
	template <typename Iterator, typename Function>
	inline void ParallelForAsync(Manager& InManager, Iterator First, Iterator Last, Function&& InFunction)
	{
		Detail::ParallelForInternal<std::true_type>(InManager, First, Last, Detail::Empty{}, std::forward<Function>(InFunction));
	}

	template <typename Container, typename CustomData, typename Function>
	inline void ParallelForAsync(Manager& InManager, Container&& InContainer, CustomData&& Data, Function&& InFunction)
	{
		Detail::ParallelForInternal<std::true_type>(InManager, std::begin(InContainer), std::end(InContainer), std::forward<CustomData>(Data), std::forward<Function>(InFunction));
	}

	template <typename Container, typename Function>
	inline void ParallelForAsync(Manager& InManager, Container&& InContainer, Function&& InFunction)
	{
		Detail::ParallelForInternal<std::true_type>(InManager, std::begin(InContainer), std::end(InContainer), Detail::Empty{}, std::forward<Function>(InFunction));
	}

	namespace Detail
	{
		struct NoOp
		{
			template <typename T>
			auto operator()(T Item) const { return Item; }
		};

		template <typename Iterator, typename UnaryOp, typename BinaryOp, typename OutIterator>
		struct MapReducePayload
		{
			Iterator Input;
			size_t Count;
			UnaryOp MapOperation;
			BinaryOp ReduceOperation;
			OutIterator Output;
		};

		template <typename Iterator, typename UnaryOp, typename BinaryOp>
		auto ParallelMapReduceInternal(Manager& InManager, Iterator First, Iterator Last, UnaryOp&& MapOperation, BinaryOp&& ReduceOperation)
		{
			using IntermediateType = decltype(MapOperation(*First));
			using ResultType = decltype(ReduceOperation(std::declval<IntermediateType>(), std::declval<IntermediateType>()));
			using ResultContainerType = std::vector<ResultType>;
			using PayloadType = MapReducePayload<Iterator, std::remove_reference_t<UnaryOp>, std::remove_reference_t<BinaryOp>, decltype(std::declval<ResultContainerType>().begin())>;

			auto Dependency{ std::make_shared<Counter<>>(0) };

			const auto Distance = std::distance(First, Last);  // Data set size.

			// If we have a relatively small data set, serial execution is probably faster.
			if (Distance < 1000)
			{
				if constexpr (std::is_same_v<std::remove_reference_t<UnaryOp>, NoOp>)
				{
					return std::reduce(First, Last, ResultType{}, std::forward<BinaryOp>(ReduceOperation));
				}

				else
				{
					return std::transform_reduce(First, Last, ResultType{}, std::forward<BinaryOp>(ReduceOperation), std::forward<UnaryOp>(MapOperation));
				}
			}

			const auto JobCount = InManager.GetWorkerCount();  // Number of jobs, excluding combiner.
			const auto PayloadSize = Distance / JobCount;  // Input data chunk size per job.
			const size_t Remainder = Distance % PayloadSize;  // Left over data chunk for the last job.

			ResultContainerType Results;
			Results.resize(JobCount);  // Each job produces an intermediate result.
			std::vector<PayloadType> Payloads;
			Payloads.resize(JobCount);  // Each job needs a payload as well.

			// Separate the loops in order to maintain cache locality.
			for (size_t Iter{ 0 }; Iter < JobCount; ++Iter)
			{
				Payloads[Iter].Input = First + (Iter * PayloadSize);
				Payloads[Iter].Count = PayloadSize;

				if constexpr (!std::is_same_v<std::remove_reference_t<UnaryOp>, NoOp>)
				{
					Payloads[Iter].MapOperation = MapOperation;
				}

				Payloads[Iter].ReduceOperation = ReduceOperation;
				Payloads[Iter].Output = Results.begin() + Iter;
			}

			// Fix the remainder.
			Payloads[JobCount - 1].Count += Remainder;
			
			for (size_t Iter{ 0 }; Iter < JobCount; ++Iter)
			{
				InManager.Enqueue(Job{ [](auto Payload)
					{
						auto* TypedPayload = reinterpret_cast<PayloadType*>(Payload);
						
						if constexpr (std::is_same_v<std::remove_reference_t<UnaryOp>, NoOp>)
						{
							*TypedPayload->Output = std::reduce(TypedPayload->Input, TypedPayload->Input + TypedPayload->Count, ResultType{}, TypedPayload->ReduceOperation);
						}

						else
						{
							*TypedPayload->Output = std::transform_reduce(TypedPayload->Input, TypedPayload->Input + TypedPayload->Count, ResultType{}, TypedPayload->ReduceOperation, TypedPayload->MapOperation);
						}
					}, static_cast<void*>(&Payloads[Iter]) }, Dependency);
			}

			Dependency->Wait(0);

			return std::accumulate(Results.begin(), Results.end(), ResultType{});
		}
	}

	template <typename Iterator, typename UnaryOp, typename BinaryOp>
	inline auto ParallelMapReduce(Manager& InManager, Iterator First, Iterator Last, UnaryOp&& MapOperation, BinaryOp&& ReduceOperation)
	{
		return Detail::ParallelMapReduceInternal(InManager, First, Last, std::forward<UnaryOp>(MapOperation), std::forward<BinaryOp>(ReduceOperation));
	}

	template <typename Container, typename UnaryOp, typename BinaryOp>
	inline auto ParallelMapReduce(Manager& InManager, Container&& InContainer, UnaryOp&& MapOperation, BinaryOp&& ReduceOperation)
	{
		return Detail::ParallelMapReduceInternal(InManager, std::begin(InContainer), std::end(InContainer), std::forward<UnaryOp>(MapOperation), std::forward<BinaryOp>(ReduceOperation));
	}

	template <typename Iterator, typename BinaryOp>
	inline auto ParallelReduce(Manager& InManager, Iterator First, Iterator Last, BinaryOp&& ReduceOperation)
	{
		return Detail::ParallelMapReduceInternal(InManager, First, Last, Detail::NoOp{}, std::forward<BinaryOp>(ReduceOperation));
	}

	template <typename Container, typename BinaryOp>
	inline auto ParallelReduce(Manager& InManager, Container&& InContainer, BinaryOp&& ReduceOperation)
	{
		return Detail::ParallelMapReduceInternal(InManager, std::begin(InContainer), std::end(InContainer), Detail::NoOp{}, std::forward<BinaryOp>(ReduceOperation));
	}
}