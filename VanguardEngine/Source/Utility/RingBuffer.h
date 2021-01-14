// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <cstdlib>
#include <utility>

template <typename T, size_t size>
struct RingBuffer
{
private:
	T data[size];
	size_t head{ 0 };
	size_t tail{ 0 };

public:
	RingBuffer() = default;
	RingBuffer(const RingBuffer&) = default;
	RingBuffer(RingBuffer&&) noexcept = default;

	RingBuffer& operator=(const RingBuffer&) = default;
	RingBuffer& operator=(RingBuffer&&) noexcept = default;

	template <typename U>
	bool push_back(U&& element)
	{
		auto next = (head + 1) % size;
		if (next != tail)
		{
			data[head] = std::move(element);
			head = next;

			return true;
		}

		return false;
	}

	bool pop_front(T& element)
	{
		if (tail != head)
		{
			element = std::move(data[tail]);
			tail = (tail + 1) % size;
			
			return true;
		}

		return false;
	}
};