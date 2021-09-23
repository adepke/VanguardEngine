// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <spdlog/sinks/base_sink.h>
#include <spdlog/details/null_mutex.h>

template <typename Mutex>
class TracySink : public spdlog::sinks::base_sink<Mutex>
{
protected:
	virtual void sink_it_(const spdlog::details::log_msg& msg) override
	{
		spdlog::memory_buf_t formatted;
		spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
		const auto msgFormatted = fmt::to_string(formatted);

		switch (msg.level)
		{
		case spdlog::level::warn: TracyMessageC(msgFormatted.c_str(), msgFormatted.size(), tracy::Color::Yellow); break;
		case spdlog::level::err: TracyMessageC(msgFormatted.c_str(), msgFormatted.size(), 0xff5555); break;
		case spdlog::level::critical: TracyMessageC(msgFormatted.c_str(), msgFormatted.size(), tracy::Color::Red); break;
		default: TracyMessage(msgFormatted.c_str(), msgFormatted.size()); break;
		}
	}

	virtual void flush_() override {}
};

// Tracy will handle multi-threaded messages internally.
using TracySink_mt = TracySink<spdlog::details::null_mutex>;