// Copyright (c) 2019-2022 Andrew Depke

#pragma once

constexpr std::wstring Str2WideStr(const std::string& str)
{
	return std::wstring{ str.cbegin(), str.cend() };
}