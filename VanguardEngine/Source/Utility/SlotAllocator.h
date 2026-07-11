// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <cstdint>
#include <vector>

// Non-owning allocator, used to interface with GPU memory. Can be used with CPU as well,
// just need to bring your own backing storage.
class SlotAllocator
{
public:
	static constexpr uint32_t invalidSlot = 0xffffffff;

private:
	struct Range
	{
		uint32_t offset;
		uint32_t count;
	};

	std::vector<Range> freeRanges;  // Sorted by offset.
	uint32_t watermark = 0;
	uint32_t capacity = 0;

public:
	void Initialize(uint32_t inCapacity)
	{
		capacity = inCapacity;
		watermark = 0;
		freeRanges.clear();
	}

	uint32_t Allocate(uint32_t count)
	{
		if (count == 0)
		{
			return invalidSlot;
		}

		// Walk all ranges and pick the first one big enough.
		for (size_t i = 0; i < freeRanges.size(); i++)
		{
			if (freeRanges[i].count >= count)
			{
				const auto slot = freeRanges[i].offset;

				if (freeRanges[i].count == count)
				{
					freeRanges.erase(freeRanges.begin() + i);
				}
				else
				{
					freeRanges[i].offset += count;
					freeRanges[i].count -= count;
				}

				return slot;
			}
		}

		// Fall back to bumping the tail.
		if (watermark + count > capacity)
		{
			return invalidSlot;
		}

		const auto slot = watermark;
		watermark += count;

		return slot;
	}

	void Free(uint32_t offset, uint32_t count)
	{
		if (count == 0)
		{
			return;
		}

		// Insert sorted by offset.
		auto it = freeRanges.begin();
		while (it != freeRanges.end() && it->offset < offset)
		{
			++it;
		}

		it = freeRanges.insert(it, { offset, count });

		// Merge with the next range if adjacent.
		if (const auto next = it + 1; next != freeRanges.end() && it->offset + it->count == next->offset)
		{
			it->count += next->count;
			freeRanges.erase(next);
		}

		// Merge with the previous range if adjacent.
		if (it != freeRanges.begin())
		{
			if (const auto previous = it - 1; previous->offset + previous->count == it->offset)
			{
				previous->count += it->count;
				it = freeRanges.erase(it);
				it = it - 1;
			}
		}

		// Give back to the watermark if we freed the tail of the pool.
		if (!freeRanges.empty() && freeRanges.back().offset + freeRanges.back().count == watermark)
		{
			watermark = freeRanges.back().offset;
			freeRanges.pop_back();
		}
	}

	uint32_t GetCapacity() const noexcept { return capacity; }
	uint32_t GetWatermark() const noexcept { return watermark; }
};
