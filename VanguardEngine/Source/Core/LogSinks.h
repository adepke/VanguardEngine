// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Editor/Editor.h>  // Careful with this include.

#include <spdlog/sinks/base_sink.h>
#include <spdlog/details/null_mutex.h>

#include <limits>

template <typename Mutex>
class TracySink : public spdlog::sinks::base_sink<Mutex>
{
protected:
	virtual void sink_it_(const spdlog::details::log_msg& msg) override
	{
#if ENABLE_PROFILING
		spdlog::memory_buf_t formatted;
		spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
		const auto msgFormatted = fmt::to_string(formatted);

		if (msgFormatted.size() >= std::numeric_limits<uint16_t>::max())
		{
			const std::string messageTooLong = "Log message too large, refer to other log source for actual log.";
			TracyMessageC(messageTooLong.c_str(), messageTooLong.size(), tracy::Color::Cyan);
			return;
		}

		switch (msg.level)
		{
		case spdlog::level::warn: TracyMessageC(msgFormatted.c_str(), msgFormatted.size(), tracy::Color::Yellow); break;
		case spdlog::level::err: TracyMessageC(msgFormatted.c_str(), msgFormatted.size(), 0xff5555); break;
		case spdlog::level::critical: TracyMessageC(msgFormatted.c_str(), msgFormatted.size(), tracy::Color::Red); break;
		default: TracyMessage(msgFormatted.c_str(), msgFormatted.size()); break;
		}
#endif
	}

	virtual void flush_() override {}
};

// Tracy will handle multi-threaded messages internally.
using TracySink_mt = TracySink<spdlog::details::null_mutex>;

template <typename Mutex>
class EditorSink : public spdlog::sinks::base_sink<Mutex>
{
protected:
	virtual void sink_it_(const spdlog::details::log_msg& msg) override
	{
		spdlog::memory_buf_t formatted;
		spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
		const auto msgFormatted = fmt::to_string(formatted);

		Editor::Get().LogMessage(msgFormatted);
	}

	virtual void flush_() override {}
};

using EditorSink_mt = EditorSink<std::mutex>;