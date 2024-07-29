#include "logger.h"

template <typename... ParamsT>
[[gnu::always_inline]] inline void ::Logger::WriteLine(ParamsT const&... p_Args)
{
	using namespace std::string_view_literals;

	std::lock_guard const l_Lock(s_Mutex);
	s_GlobalLogger.Write(
		NCurses::Cyan(std::put_time(Common::GetLocalTime(), "%Y/%m/%d %H:%M:%S %Z"), " | "sv),
		p_Args..., '\n');
}

template <NCurses::ColorIndex t_Color, std::size_t t_LogStringBufferCapacity>
bool ::Logger::FormatWriteLine(char const* p_Format,
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

template <NCurses::ColorIndex t_Color, std::size_t t_LogStringBufferCapacity>
bool ::Logger::FormatWriteLine(char const* p_Format, ...)
{
	std::va_list l_ArgumentList;
	va_start(l_ArgumentList, p_Format);
	bool const l_Result{ FormatWriteLine<t_Color, t_LogStringBufferCapacity>(p_Format,
																									 l_ArgumentList) };
	va_end(l_ArgumentList);
	return l_Result;
}

template <typename... ParamsT>
void ::Logger::InterpolateWriteLine(std::string_view const p_FormatString, ParamsT const&... p_Args)
{
	using namespace std::string_view_literals;

	std::lock_guard const l_Lock(s_Mutex);
	s_GlobalLogger.Write(
		NCurses::Cyan(std::put_time(Common::GetLocalTime(), "%Y/%m/%d %H:%M:%S %Z"), " | "sv));

	s_GlobalLogger.InterpolateWrite<'$'>(p_FormatString, p_Args...);

	s_GlobalLogger.Write('\n');
}
