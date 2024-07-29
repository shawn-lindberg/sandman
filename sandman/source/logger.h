#pragma once

#include <cstdarg>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>

#include "ncurses_ui.h"

class Logger
{

private:

	std::ostringstream m_Buffer;
	std::ostream& m_outputStream;
	bool m_ScreenEcho{ false };

public:

	[[nodiscard]] Logger(std::ostream& outputStream): m_Buffer(), m_outputStream(outputStream) {}

	template <typename T, typename... ParamsT>
	[[gnu::always_inline]] inline void Write(T const& p_FirstArg, ParamsT const&... p_Args);

	template <char t_InterpolationIndicator>
	void InterpolateWrite(std::string_view const p_FormatString);

	template <char t_InterpolationIndicator, typename T, typename... ParamsT>
	void InterpolateWrite(std::string_view p_FormatString, T const& p_FirstArg,
								 ParamsT const&... p_Args);

private:

	static std::mutex s_Mutex;
	static std::ofstream s_File;
	static Logger s_GlobalLogger;

public:

	[[gnu::always_inline]] [[nodiscard]] inline static bool& GetScreenEchoFlag()
	{
		return s_GlobalLogger.m_ScreenEcho;
	}

	[[nodiscard]] static bool Initialize(char const* const p_LogFileName);

	static void Uninitialize();

	template <typename... ParamsT>
	[[gnu::always_inline]] inline static void WriteLine(ParamsT const&... p_Args);

	template <NCurses::ColorIndex t_Color = NCurses::ColorIndex::NONE,
				 std::size_t t_LogStringBufferCapacity = 2048u>
	[[gnu::format(printf, 1, 0)]] static bool FormatWriteLine(char const* p_Format,
																				 std::va_list l_ArgumentList);

	template <NCurses::ColorIndex t_Color = NCurses::ColorIndex::NONE,
				 std::size_t t_LogStringBufferCapacity = 2048u>
	[[gnu::format(printf, 1, 2)]] static bool FormatWriteLine(char const* p_Format, ...);

	template <typename... ParamsT>
	static void InterpolateWriteLine(std::string_view const p_FormatString,
												ParamsT const&... p_Args);
};

#include "logger.inl"
#include "logger_global.inl"
