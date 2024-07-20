#pragma once

#include <cstdarg>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <mutex>
#include <fstream>
#include <string>

#include "ncurses_ui.h"

namespace Common
{
	[[gnu::always_inline]] inline auto GetLocalTime()
	{
		auto const l_TimePoint(std::chrono::system_clock::now());

		auto const l_ArithmeticTimeValue{ std::chrono::system_clock::to_time_t(l_TimePoint) };
		static_assert(std::is_arithmetic_v<decltype(l_ArithmeticTimeValue)>);

		return std::localtime(&l_ArithmeticTimeValue);
	} 
}

namespace Logger
{
	using namespace std::string_view_literals;

	extern bool g_ScreenEcho;

	class Self final
	{
		static std::mutex s_Mutex;
		static std::ofstream s_File;
		static std::ostringstream s_Buffer;

		Self() = delete; ~Self() = delete;

		template <typename T, typename... ParamsT>
		[[gnu::always_inline]] static inline void Write(T const& p_FirstArg, ParamsT const&... p_Args)
		{
			if constexpr (std::is_same_v<T, NCurses::Attr>)
			{
				auto const string(s_Buffer.str());
				if (g_ScreenEcho) NCurses::LoggingWindow::Write(string);
				s_File << string;

				if (g_ScreenEcho) NCurses::LoggingWindow::Write(p_FirstArg);

				s_Buffer.str("");
			}
			else
			{
				s_Buffer << p_FirstArg;
			}

			if constexpr (sizeof...(p_Args) > 0u)
			{
				Write(p_Args...);
			}
			else
			{
				auto const string(s_Buffer.str());

				if (g_ScreenEcho) NCurses::LoggingWindow::Print(string);

				s_File << string;
				s_Buffer.str("");
			}
		}

		template <typename... ParamsT>
		friend void WriteLine(ParamsT const&...);

		template <typename Toggler_T, typename... ParamsT>
		friend void WriteLine(ParamsT const&...);

		friend bool Initialize(char const* const);

		friend void Uninitialize();
	};

	bool Initialize(char const* const p_LogFileName);

	void Uninitialize();

	template <typename... ParamsT>
	[[gnu::always_inline]] inline void WriteLine(ParamsT const&... p_Args)
	{
		std::lock_guard const l_Lock(Self::s_Mutex);
		Self::Write(
			NCurses::Cyan::On,
			std::put_time(Common::GetLocalTime(), "%Y/%m/%d %H:%M:%S %Z"),
			" | "sv,
			NCurses::Cyan::Off,
			p_Args...,
			'\n'
		);
	}

	template <typename TogglerT, typename... ParamsT>
	[[gnu::always_inline]] inline void
	WriteLine(ParamsT const&... p_Args)
	{
		std::lock_guard const l_Lock(Self::s_Mutex);
		Self::Write(
			NCurses::Cyan::On,
			std::put_time(Common::GetLocalTime(), "%Y/%m/%d %H:%M:%S %Z"),
			" | "sv,
			NCurses::Cyan::Off,
			TogglerT::On,
			p_Args...,
			'\n',
			TogglerT::Off
		);
	}

	template <typename TogglerT = std::nullptr_t>
	[[gnu::format(printf, 1, 0)]] bool FormatWriteLine(char const* p_Format, std::va_list l_ArgumentList)
	{
		static constexpr std::size_t s_LogStringBufferCapacity{ 2048u };
		char l_LogStringBuffer[s_LogStringBufferCapacity];

		std::vsnprintf(l_LogStringBuffer, s_LogStringBufferCapacity, p_Format, l_ArgumentList);

		if constexpr (not std::is_null_pointer_v<TogglerT>)
			WriteLine<TogglerT>(l_LogStringBuffer);
		else
			WriteLine(l_LogStringBuffer);

		return true;
	}

	template <typename TogglerT=std::nullptr_t>
	[[gnu::format(printf, 1, 2)]] bool FormatWriteLine(char const* p_Format, ...)
	{
		std::va_list l_ArgumentList;
		va_start(l_ArgumentList, p_Format);
		bool const l_Result{ FormatWriteLine<TogglerT>(p_Format, l_ArgumentList) };
		va_end(l_ArgumentList);
		return l_Result;
	}
}
