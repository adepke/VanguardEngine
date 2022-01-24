// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <cstdlib>
#include <utility>
#include <new>

template <typename>
struct StackFunction;

template <typename Result, typename... ArgTypes>
struct StackFunction<Result(ArgTypes...)>
{
	static constexpr size_t size = 32;

private:
	char buffer[size];

	struct Concept
	{
		virtual Result operator()(ArgTypes...) const = 0;
		virtual ~Concept() = default;
	};

	template <typename Functor>
	struct Model final : public Concept
	{
		Functor internal;

		Model(Functor&& functor) : internal(functor) {}

		virtual Result operator()(ArgTypes&&... args) const override
		{
			return internal(std::forward<ArgTypes>(args)...);
		}
	};

public:
	template <typename Functor>
	StackFunction(Functor&& functor)
	{
		static_assert(sizeof(functor) <= size, "Kernel functor cannot exceed set size");

		new(buffer) Model<Functor>{ std::forward<Functor>(functor) };
	}

	StackFunction(const StackFunction&) = delete;
	StackFunction(StackFunction&& other) noexcept
	{
		std::swap(buffer, other.buffer);
	}

	StackFunction& operator=(const StackFunction&) = delete;
	StackFunction& operator=(StackFunction&&) noexcept = delete;

	template <typename... Ts>
	Result operator()(Ts&&... args) const
	{
		return (reinterpret_cast<const Concept*>(buffer))->operator()(std::forward<Ts>(args)...);
	}

	~StackFunction()
	{
		(reinterpret_cast<Concept*>(buffer))->~Concept();
	}
};