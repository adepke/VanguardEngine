// Copyright (c) 2019-2022 Andrew Depke

#include <Core/CommandLine.h>
#include <Core/Globals.h>
#include <Core/Misc.h>

#include <string>
#include <algorithm>
#include <cwchar>
#include <cwctype>

void ParseCommandLineOptions(const std::vector<std::wstring>& args)
{
	auto& options = GCommandLineOptions;
	options = CommandLineOptions{};

	// Skip the executable.
	for (size_t i = 1; i < args.size(); ++i)
	{
		const std::wstring& arg = args[i];

		// Consumes and returns the next token as the value for the current flag.
		const auto NextValue = [&args, &i, &options](std::wstring& out) -> bool
		{
			if (i + 1 >= args.size())
			{
				options.valid = false;
				return false;
			}
			out = args[++i];
			return true;
		};

		if (arg == VGText("--headless"))
		{
			options.headless = true;
		}
		else if (arg == VGText("--output"))
		{
			std::wstring value;
			if (NextValue(value))
			{
				options.output = std::filesystem::path{ value };
			}
		}
		else if (arg == VGText("--delay"))
		{
			std::wstring value;
			if (NextValue(value))
			{
				wchar_t* end = nullptr;
				const unsigned long parsed = std::wcstoul(value.c_str(), &end, 10);
				if (end == value.c_str() || end == nullptr || *end != L'\0')
				{
					options.valid = false;
				}
				else
				{
					options.delayFrames = static_cast<uint32_t>(parsed);
				}
			}
		}
		else if (arg == VGText("--scene"))
		{
			std::wstring value;
			if (NextValue(value))
			{
				options.scene = std::filesystem::path{ value };
			}
		}
	}

	// Check output is a PNG.
	if (options.output.has_value())
	{
		if (options.output->extension().wstring() != VGText(".png"))
		{
			options.valid = false;
		}
	}

	// Headless requires an ouput path.
	if (options.headless && !options.output.has_value())
	{
		options.valid = false;
	}

	options.valid = true;
}

