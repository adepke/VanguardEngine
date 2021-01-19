// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <cstdlib>
#include <utility>
#include <vector>

template <typename T>
struct RingBuffer
{
private:
	std::vector<T> buffer;
	size_t head = 0;
	size_t tail = 0;
	size_t contentSize = 0;

	void BumpHead()
	{
		if (contentSize == 0)
			return;

		++head;
		--contentSize;
		if (head == buffer.size())
			head = 0;
	}

	void BumpTail()
	{
		++tail;
		++contentSize;
		if (tail == buffer.size())
			tail = 0;
	}

public:
	RingBuffer(size_t size) : head(1)
	{
		buffer.resize(size);
	}

	RingBuffer(const RingBuffer&) = default;
	RingBuffer(RingBuffer&&) noexcept = default;

	RingBuffer& operator=(const RingBuffer&) = default;
	RingBuffer& operator=(RingBuffer&&) noexcept = default;

	T& front()
	{
		return buffer[head];
	}

	const T& front() const
	{
		return buffer[head];
	}

	T& back()
	{
		return buffer[tail];
	}

	const T& back() const
	{
		return buffer[tail];
	}

	size_t size() const
	{
		return contentSize;
	}

	void push_back(const T& element)
	{
		BumpTail();
		if (contentSize > buffer.size())
			BumpHead();
		buffer[tail] = element;
	}

	void pop_front()
	{
		BumpHead();
	}

	T& operator[](size_t index)
	{
		return buffer[index];
	}

	const T& operator[](size_t index) const
	{
		return buffer[index];
	}

	void* data() { return buffer.data(); }
};