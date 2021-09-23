// Copyright (c) 2019-2021 Andrew Depke

#pragma once

constexpr std::wstring Str2WideStr(const std::string& str)
{
	return std::wstring{ str.cbegin(), str.cend() };
}