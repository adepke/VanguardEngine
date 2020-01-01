// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <cstdlib>
#include <utility>

template <typename T, size_t Size>
struct RingBuffer
{
private:
	T Data[Size];
	size_t Head{ 0 };
	size_t Tail{ 0 };

public:
	RingBuffer() = default;
	RingBuffer(const RingBuffer&) = default;
	RingBuffer(RingBuffer&&) noexcept = default;

	RingBuffer& operator=(const RingBuffer&) = default;
	RingBuffer& operator=(RingBuffer&&) noexcept = default;

	template <typename U>
	bool push_back(U&& Element)
	{
		auto Next = (Head + 1) % Size;
		if (Next != Tail)
		{
			Data[Head] = std::move(Element);
			Head = Next;

			return true;
		}

		return false;
	}

	bool pop_front(T& Element)
	{
		if (Tail != Head)
		{
			Element = std::move(Data[Tail]);
			Tail = (Tail + 1) % Size;
			
			return true;
		}

		return false;
	}
};