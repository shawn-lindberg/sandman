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
	[[gnu::always_inline]] inline std::tm* GetLocalTime()
	{
		auto const l_TimePoint(std::chrono::system_clock::now());

		auto const l_ArithmeticTimeValue{ std::chrono::system_clock::to_time_t(l_TimePoint) };
		static_assert(std::is_arithmetic_v<decltype(l_ArithmeticTimeValue)>);

		return std::localtime(&l_ArithmeticTimeValue);
	}
} // namespace Common

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
			if constexpr (NCurses::IsColor<T>)
			{
				std::apply(
					[](auto const&... args) { Write(T::On, args..., T::Off); }, p_FirstArg.objects);
			}
			else if constexpr (std::is_same_v<T, NCurses::Attr>)
			{
				std::string const l_String(s_Buffer.str());
				if (g_ScreenEcho)
				{
					NCurses::LoggingWindow::Write(l_String);
				}
				s_File << l_String;

				if (g_ScreenEcho)
				{
					NCurses::LoggingWindow::Write(p_FirstArg);
				}

				// Clear buffer.
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

				if (g_ScreenEcho)
				{
					NCurses::LoggingWindow::Print(string);
				}

				s_File << string;
				s_Buffer.str("");
			}
		}

		template <char t_InterpolationIndicator>
		static void InterpolateWrite(std::string_view const p_FormatString)
		{
			bool l_EscapingCharacter{ false };
			for (char const c : p_FormatString)
			{
				switch (c)
				{
					case t_InterpolationIndicator:
						if (l_EscapingCharacter)
						{
							l_EscapingCharacter = false, Self::Write(c);
						}
						else
						{
							Self::Write("`null`"sv);
						}
						break;
					case '\0':
						l_EscapingCharacter = true;
						break;
					default:
						Self::Write(c);
						break;
				}
			}
		}

		template <char t_InterpolationIndicator, typename T, typename... ParamsT>
		static void InterpolateWrite(std::string_view p_FormatString, T const& p_FirstArg,
										ParamsT const&... p_Args)
		{
			bool l_EscapingCharacter{ false };
			for (std::string_view::size_type l_Index{ 0u }; l_Index < p_FormatString.size(); ++l_Index)
			{
				char const c{ p_FormatString[l_Index] };
				switch (c)
				{
					case t_InterpolationIndicator:
						if (l_EscapingCharacter)
						{
							l_EscapingCharacter = false;
							Self::Write(c);
						}
						else
						{
							Self::Write(p_FirstArg);
							p_FormatString.remove_prefix(++l_Index);
							return InterpolateWrite<t_InterpolationIndicator>(p_FormatString, p_Args...);
						}
						break;
					case '\0':
						l_EscapingCharacter = true;
						break;
					default:
						Self::Write(c);
						break;
				}
			}
		}

		template <typename... ParamsT>
		friend void WriteLine(ParamsT const&...);

		template <typename Toggler_T, typename... ParamsT>
		friend void WriteLine(ParamsT const&...);

		template <typename... ParamsT>
		friend void InterpolateWriteLine(std::string_view const, ParamsT const&...);

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
			NCurses::Cyan(std::put_time(Common::GetLocalTime(), "%Y/%m/%d %H:%M:%S %Z"), " | "sv),
			p_Args...,
			'\n');
	}

	template <NCurses::ColorIndex t_Color = NCurses::ColorIndex::NONE,
				 std::size_t t_LogStringBufferCapacity = 2048u>
	[[gnu::format(printf, 1, 0)]] bool FormatWriteLine(char const* p_Format,
																		std::va_list l_ArgumentList)
	{
		char l_LogStringBuffer[t_LogStringBufferCapacity];

		std::vsnprintf(l_LogStringBuffer, t_LogStringBufferCapacity, p_Format, l_ArgumentList);

		if constexpr (t_Color == NCurses::ColorIndex::NONE)
		{
			WriteLine(l_LogStringBuffer);
		}
		else
		{
			WriteLine(NCurses::Color<t_Color, char const*>(l_LogStringBuffer));
		}

		return true;
	}

	template <NCurses::ColorIndex t_Color = NCurses::ColorIndex::NONE,
				 std::size_t t_LogStringBufferCapacity = 2048u>
	[[gnu::format(printf, 1, 2)]] bool FormatWriteLine(char const* p_Format, ...)
	{
		std::va_list l_ArgumentList;
		va_start(l_ArgumentList, p_Format);
		bool const l_Result{ FormatWriteLine<t_Color, t_LogStringBufferCapacity>(p_Format,
																										 l_ArgumentList) };
		va_end(l_ArgumentList);
		return l_Result;
	}

	template <typename... ParamsT>
	void InterpolateWriteLine(std::string_view const p_FormatString, ParamsT const&... p_Args)
	{
		std::lock_guard const l_Lock(Self::s_Mutex);
		Self::Write(
			NCurses::Cyan(std::put_time(Common::GetLocalTime(), "%Y/%m/%d %H:%M:%S %Z"), " | "sv));

		Self::InterpolateWrite<'$'>(p_FormatString, p_Args...);

		Self::Write('\n');
	}

} // namespace Logger
