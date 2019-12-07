// Copyright (c) 2019 Andrew Depke

template <typename... Ts>
struct Visitor : Ts...
{
	using Ts::operator()...;
};

template <typename... Ts>
Visitor(Ts...) -> Visitor<Ts...>;