// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <cstddef>  // std::size_t, std::ptrdiff_t

// Inline LIFO allocator with heap fall back for usage with job dependencies. C++20 compliant.

namespace Jobs
{
	template <typename T, size_t Size>
	struct DependencyAllocator
	{
		using value_type = T;
		using size_type = size_t;
		using difference_type = std::ptrdiff_t;
		using propagate_on_container_move_assignment = std::true_type;
		using is_always_equal = std::true_type;

	private:
		std::aligned_storage_t<sizeof(T) * Size, alignof(T)> inlineBuffer = {};
		value_type* pointer;  // Points to next available space in inline buffer.

	public:
		constexpr DependencyAllocator() noexcept : pointer{ reinterpret_cast<value_type*>(&inlineBuffer) } {}
		constexpr DependencyAllocator(const DependencyAllocator& Other) noexcept : pointer{ reinterpret_cast<value_type*>(&inlineBuffer) } {}
		template <typename U>
		constexpr DependencyAllocator(const DependencyAllocator<U, Size>& Other) noexcept : pointer{ reinterpret_cast<value_type*>(&inlineBuffer) } {}

		template <typename U>
		struct rebind
		{
			using other = DependencyAllocator<U, Size>;
		};

		[[nodiscard]]
		constexpr T* allocate(size_t n)
		{
			// First attempt to inline allocate.
			if (reinterpret_cast<value_type*>(&inlineBuffer) + Size - pointer >= static_cast<std::ptrdiff_t>(n))
			{
				const auto old = pointer;
				pointer += n;

				return old;
			}

			// Fall back to heap allocation.
			return static_cast<T*>(::operator new(n * sizeof(T), std::align_val_t{ alignof(T) }));
		}

		constexpr void deallocate(T* p, size_t n)
		{
			if (reinterpret_cast<value_type*>(&inlineBuffer) <= p && p < reinterpret_cast<value_type*>(&inlineBuffer) + Size)
			{
				// Stack allocated, do nothing.
				if (p + n == pointer)
				{
					pointer = p;
				}
			}

			else
			{
				// Heap allocated, delete it.
				::operator delete(p, std::align_val_t{ alignof(T) });
			}
		}
	};

	template <typename T1, typename T2, size_t Size>
	constexpr bool operator==(const DependencyAllocator<T1, Size>& left, const DependencyAllocator<T2, Size>& right) noexcept
	{
		return &left.inlineBuffer == &right.inlineBuffer;
	}
}