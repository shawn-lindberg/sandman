#include "logger.h"

template <typename... ParamsT>
[[gnu::always_inline]] inline void ::Logger::WriteLine(ParamsT const&... args)
{
	using namespace std::string_view_literals;

	std::lock_guard const lock(ms_Mutex);
	ms_GlobalLogger.Write(
		NCurses::Cyan(std::put_time(Common::GetLocalTime(), "%Y/%m/%d %H:%M:%S %Z"), " | "sv),
		args..., '\n');
}

template <NCurses::ColorIndex kColor, std::size_t kLogStringBufferCapacity>
bool ::Logger::FormatWriteLine(char const* format, std::va_list argumentList)
{
	char logStringBuffer[kLogStringBufferCapacity];

	std::vsnprintf(logStringBuffer, kLogStringBufferCapacity, format, argumentList);

	if constexpr (kColor == NCurses::ColorIndex::None)
	{
		WriteLine(logStringBuffer);
	}
	else
	{
		WriteLine(NCurses::Color<kColor, char const*>(logStringBuffer));
	}

	return true;
}

template <NCurses::ColorIndex kColor, std::size_t kLogStringBufferCapacity>
bool ::Logger::FormatWriteLine(char const* format, ...)
{
	std::va_list argumentList;
	va_start(argumentList, format);
	bool const result{ FormatWriteLine<kColor, kLogStringBufferCapacity>(format, argumentList) };
	va_end(argumentList);
	return result;
}

template <typename... ParamsT>
void ::Logger::InterpolateWriteLine(std::string_view const formatString, ParamsT const&... args)
{
	using namespace std::string_view_literals;

	std::lock_guard const lock(ms_Mutex);
	ms_GlobalLogger.Write(
		NCurses::Cyan(std::put_time(Common::GetLocalTime(), "%Y/%m/%d %H:%M:%S %Z"), " | "sv));

	ms_GlobalLogger.InterpolateWrite<'$'>(formatString, args...);

	ms_GlobalLogger.Write('\n');
}
