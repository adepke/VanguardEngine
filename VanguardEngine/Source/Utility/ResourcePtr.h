// Copyright (c) 2019 Andrew Depke

#pragma once

#include <type_traits>
#include <memory>

namespace Detail
{
	template <typename T>
	struct Releasable
	{
		typedef char(&Yes)[1];
		typedef char(&No)[2];

		template <typename U>
		static constexpr Yes Check(decltype(&U::Release));
		template <typename>
		static constexpr No Check(...);

		static constexpr bool Value = sizeof(Check<T>(0)) == sizeof(Yes);
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
		pointer Internal = nullptr;
		deleter_type Delete;

	public:
		constexpr ResourcePtrBase() noexcept = default;
		constexpr ResourcePtrBase(nullptr_t) noexcept {}
		explicit ResourcePtrBase(pointer Ptr) noexcept : Internal(Ptr) {}
		ResourcePtrBase(pointer Ptr, Deleter& Del) noexcept : Internal(Ptr), Delete(Del) {}
		ResourcePtrBase(const ResourcePtrBase&) = delete;
		ResourcePtrBase(ResourcePtrBase&& Other) noexcept
		{
			std::swap(Internal, Other.Internal);
			std::swap(Delete, Other.Delete);
		}

		~ResourcePtrBase()
		{
			if (Internal)
			{
				Delete(Internal);
			}
		}

		ResourcePtrBase& operator=(const ResourcePtrBase&) = delete;
		ResourcePtrBase& operator=(ResourcePtrBase&& Other) noexcept = default;
		ResourcePtrBase& operator=(nullptr_t) noexcept { Internal = nullptr; }

		pointer Release() noexcept
		{
			return std::exchange(Internal, nullptr);
		}

		void Reset(pointer Ptr = pointer{}) noexcept
		{
			auto* const OldPtr = std::exchange(Internal, Ptr);
			if (OldPtr)
			{
				Delete(OldPtr);
			}
		}

		pointer Get() const noexcept
		{
			return Internal;
		}

		explicit operator bool() const noexcept
		{
			return static_cast<bool>(Internal);
		}

		typename std::add_lvalue_reference_t<T> operator*() const
		{
			return *Internal;
		}

		pointer operator->() const noexcept
		{
			return Internal;
		}

		std::add_pointer_t<pointer> Indirect() noexcept
		{
			return &Internal;
		}
	};

	// Had issues getting this to compile, related to default constructing a lambda (not available until C++20). Can't be
	// decayed to a function pointer either because of the auto type. Using ReleasableDelete as a replacement.
	//constexpr auto ReleasableDeleter = [](auto* Ptr) { Ptr->Release(); delete Ptr; };

	template <typename T>
	struct ReleasableDelete
	{
		constexpr ReleasableDelete() noexcept = default;

		void operator()(T* Ptr) const
		{
			Ptr->Release();
			delete Ptr;
		}
	};

	template <typename T, bool>
	struct ResourcePtrInternal : public ResourcePtrBase<T>
	{
		using BaseType = ResourcePtrBase<T>;

		using BaseType::BaseType;
	};

	template <typename T>
	struct ResourcePtrInternal<T, true> : ResourcePtrBase<T, ReleasableDelete<T>>
	{
		using BaseType = ResourcePtrBase<T, ReleasableDelete<T>>;

		constexpr ResourcePtrInternal() noexcept : BaseType(nullptr) {}
		ResourcePtrInternal(T* Ptr) noexcept : BaseType(Ptr) {}

		using BaseType::BaseType;
	};
}

template <typename T>
using ResourcePtr = Detail::ResourcePtrInternal<T, Detail::Releasable<T>::Value>;