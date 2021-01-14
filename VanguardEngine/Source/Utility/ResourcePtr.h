// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <type_traits>

namespace Detail
{
	template <typename T>
	struct Releasable
	{
		typedef char(&yes)[1];
		typedef char(&no)[2];

		template <typename U>
		static constexpr yes Check(decltype(&U::Release));
		template <typename>
		static constexpr no Check(...);

		static constexpr bool value = sizeof(Check<T>(0)) == sizeof(yes);
	};

	// We have to use this instead of std::unique_ptr because we are unable to access the underlying pointer type
	// in std::unique_ptr, which we'll need for IID_PPV_ARGS.
	template <typename T, typename Deleter = std::default_delete<T>>
	struct ResourcePtrBase
	{
		using pointer = T*;
		using element_type = T;
		using deleter_type = Deleter;

	private:
		pointer internalPtr = nullptr;
		deleter_type deleter;

	public:
		constexpr ResourcePtrBase() noexcept = default;
		constexpr ResourcePtrBase(nullptr_t) noexcept {}
		explicit ResourcePtrBase(pointer ptr) noexcept : internalPtr(ptr) {}
		ResourcePtrBase(pointer ptr, Deleter& deleteFunc) noexcept : internalPtr(ptr), deleter(deleteFunc) {}
		ResourcePtrBase(const ResourcePtrBase&) = delete;
		ResourcePtrBase(ResourcePtrBase&& other) noexcept
		{
			std::swap(internalPtr, other.internalPtr);
			std::swap(deleter, other.deleter);
		}

		~ResourcePtrBase()
		{
			if (internalPtr)
			{
				deleter(internalPtr);
			}
		}

		ResourcePtrBase& operator=(const ResourcePtrBase&) = delete;
		ResourcePtrBase& operator=(ResourcePtrBase&& other) noexcept
		{
			if (this != std::addressof(other))
			{
				Reset(other.Release());
			}

			return *this;
		}
		ResourcePtrBase& operator=(nullptr_t) noexcept { internalPtr = nullptr; return *this; }

		pointer Release() noexcept
		{
			return std::exchange(internalPtr, nullptr);
		}

		void Reset(pointer ptr = pointer{}) noexcept
		{
			auto* const oldPtr = std::exchange(internalPtr, ptr);
			if (oldPtr)
			{
				deleter(oldPtr);
			}
		}

		pointer Get() const noexcept
		{
			return internalPtr;
		}

		explicit operator bool() const noexcept
		{
			return static_cast<bool>(internalPtr);
		}

		typename std::add_lvalue_reference_t<T> operator*() const
		{
			return *internalPtr;
		}

		pointer operator->() const noexcept
		{
			return internalPtr;
		}

		std::add_pointer_t<pointer> Indirect() noexcept
		{
			return &internalPtr;
		}
	};

	template <typename T>
	struct ReleasableDelete
	{
		constexpr ReleasableDelete() noexcept = default;

		void operator()(T* ptr) const
		{
			ptr->Release();
		}
	};
}

template <typename T>
using ResourcePtr = Detail::ResourcePtrBase<T, std::conditional_t<Detail::Releasable<T>::value, Detail::ReleasableDelete<T>, std::default_delete<T>>>;