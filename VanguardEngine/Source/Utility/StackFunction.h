// Copyright (c) 2019 Andrew Depke

#pragma once

#include <cstdlib>
#include <utility>
#include <new>

template <typename>
struct StackFunction;

template <typename Result, typename... ArgTypes>
struct StackFunction<Result(ArgTypes...)>
{
	static constexpr size_t Size = 32;

private:
	char Buffer[Size];

	struct Concept
	{
		virtual Result operator()(ArgTypes...) const = 0;
		virtual ~Concept() = default;
	};

	template <typename Functor>
	struct Model final : public Concept
	{
		Functor Internal;

		Model(Functor&& InFunctor) : Internal(InFunctor) {}

		virtual Result operator()(ArgTypes&&... In) const override
		{
			return Internal(std::forward<ArgTypes>(In)...);
		}
	};

public:
	template <typename Functor>
	StackFunction(Functor&& InFunctor)
	{
		static_assert(sizeof(InFunctor) <= Size, "Kernel functor cannot exceed set size");

		new(Buffer) Model<Functor>{ std::forward<Functor>(InFunctor) };
	}

	StackFunction(const StackFunction&) = delete;
	StackFunction(StackFunction&& Other) noexcept
	{
		std::swap(Buffer, Other.Buffer);
	}

	StackFunction& operator=(const StackFunction&) = delete;
	StackFunction& operator=(StackFunction&&) noexcept = delete;

	template <typename... Ts>
	Result operator()(Ts&&... Args) const
	{
		return (reinterpret_cast<const Concept*>(Buffer))->operator()(std::forward<Ts>(Args)...);
	}

	~StackFunction()
	{
		(reinterpret_cast<Concept*>(Buffer))->~Concept();
	}
};