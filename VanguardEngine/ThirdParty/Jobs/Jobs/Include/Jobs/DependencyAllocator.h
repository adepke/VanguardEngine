// Copyright (c) 2019 Andrew Depke

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
		std::aligned_storage_t<sizeof(T) * Size, alignof(T)> InlineBuffer = {};
		value_type* Pointer;  // Points to next available space in inline buffer.

	public:
		constexpr DependencyAllocator() noexcept : Pointer{ reinterpret_cast<value_type*>(&InlineBuffer) } {}
		constexpr DependencyAllocator(const DependencyAllocator& Other) noexcept : Pointer{ reinterpret_cast<value_type*>(&InlineBuffer) } {}
		template <typename U>
		constexpr DependencyAllocator(const DependencyAllocator<U, Size>& Other) noexcept : Pointer{ reinterpret_cast<value_type*>(&InlineBuffer) } {}

		template <typename U>
		struct rebind
		{
			using other = DependencyAllocator<U, Size>;
		};

		[[nodiscard]]
		constexpr T* allocate(size_t n)
		{
			// First attempt to inline allocate.
			if (reinterpret_cast<value_type*>(&InlineBuffer) + Size - Pointer >= static_cast<std::ptrdiff_t>(n))
			{
				const auto Old = Pointer;
				Pointer += n;

				return Old;
			}

			// Fall back to heap allocation.
			return static_cast<T*>(::operator new(n * sizeof(T), std::align_val_t{ alignof(T) }));
		}

		constexpr void deallocate(T* p, size_t n)
		{
			if (reinterpret_cast<value_type*>(&InlineBuffer) <= p && p < reinterpret_cast<value_type*>(&InlineBuffer) + Size)
			{
				// Stack allocated, do nothing.
				if (p + n == Pointer)
				{
					Pointer = p;
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
	constexpr bool operator==(const DependencyAllocator<T1, Size>& Left, const DependencyAllocator<T2, Size>& Right) noexcept
	{
		return &Left.InlineBuffer == &Right.InlineBuffer;
	}
}