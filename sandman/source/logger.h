#pragma once

#include <cstdarg>
#include <sstream>
#include <chrono>
#include <array>
#include <iomanip>
#include <mutex>
#include <fstream>
#include <string>

#include "ncurses_ui.h"

// Types
//

// Functions
//

// Initialize the logger.
//
// p_LogFileName:	File name of the log for output.
//
// returns:		True if successful, false otherwise.
//
// bool LoggerInitialize(char const* p_LogFileName);

// Uninitialize the logger.
//
// void LoggerUninitialize();

// Set whether to echo messages to the screen as well.
//
// p_LogToScreen:	Whether to echo messages to the screen.
//
// void LoggerEchoToScreen(bool p_LogToScreen);

// Add a message to the log.
//
// p_Format:	Standard printf format string.
// ...:			Standard printf arguments.
//
// returns:		True if successful, false otherwise.
//
// [[gnu::format(printf, 1, 2)]] bool Logger::FormatWriteLine(char const* p_Format, ...);

// Add a message to the log (va_list version).
//
// p_Format:		Standard printf format string.
// p_Arguments:	Standard printf arguments.
//
// returns:		True if successful, false otherwise.
//
// [[gnu::format(printf, 1, 0)]] bool Logger::FormatWriteLine(char const* p_Format,
// 																	 std::va_list& p_Arguments);

/// @brief Log an empty line. (The logger still prints the date and time.)
/// 
/// @note `Logger::FormatWriteLine("")` triggers a warning `zero-length gnu_printf format string
/// [-Werror=format-zero-length]`; this function accomplishes the same without triggering a warning.
/// 
/// @return `true` if successful, `false` otherwise
///
// [[gnu::always_inline]] inline bool Logger::WriteLine()
// {
// 	return Logger::FormatWriteLine("%s", "");
// }

/// @brief Log a variable amount of arguments.
/// 
/// @note This function uses an output string stream `std::ostringstream`
/// to convert its arguments to a string which is passed to `Logger::FormatWriteLine`.
/// So, this function can be used as a quick print statement when debugging,
/// or as shorthand for when you would like to make use of `std::ostringstream`
/// to format a string and log it.
/// 
/// @param p_Arguments arguments that are printed using the logger
/// 
/// @return `true` if successful, `false` otherwise
/// 
/// @attention This function constructs and destroys a `std::ostringstream` every call.
///
// template <typename... ParamsT>
// [[gnu::always_inline]] inline bool LoggerPrintArgs(ParamsT const&... p_Arguments)
// {
// 	return Logger::FormatWriteLine("%s", (std::ostringstream() << ... << p_Arguments).str().c_str());
// }

namespace Logger
{
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
			s_Buffer << p_FirstArg;

			if constexpr (sizeof...(p_Args) > 0u)
			{
				Write(p_Args...);
			}
			else
			{
				auto const string(s_Buffer.str());

				if (g_ScreenEcho)
				{
					NCurses::LoggingWindow::Write(string);
					NCurses::LoggingWindow::Refresh();
				}

				s_File << string;
				s_Buffer.str("");
				s_Buffer.clear();
			}
		}

		template <typename... ParamsT>
		friend void WriteLine(ParamsT const&...);

		friend bool Initialize(char const* const);

		friend void Uninitialize();
	};

	bool Initialize(char const* const p_LogFileName);

	void Uninitialize();

	template <typename... ParamsT>
	[[gnu::always_inline]] inline void WriteLine(ParamsT const&... p_Args)
	{
		auto const l_TimePoint(std::chrono::system_clock::now());

		auto const l_ArithmeticTimeValue{ std::chrono::system_clock::to_time_t(l_TimePoint) };
		static_assert(std::is_arithmetic_v<decltype(l_ArithmeticTimeValue)>);

		std::lock_guard const l_Lock(Self::s_Mutex);
		Self::s_Buffer << std::put_time(std::localtime(&l_ArithmeticTimeValue), "%Y/%m/%d %H:%M:%S %Z");
		Self::Write(' ', p_Args..., '\n');
	}

	[[gnu::format(printf, 1, 2)]] bool FormatWriteLine(char const* p_Format, ...);
}
