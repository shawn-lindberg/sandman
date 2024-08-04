#include "logger.h"

template <typename... ParamsT>
[[gnu::always_inline]] inline void ::Logger::WriteLine(Common::Forward<ParamsT>... args)
{
	using namespace std::string_view_literals;

	std::lock_guard const lock(ms_Mutex);
	ms_GlobalLogger.Write(
		Shell::Cyan(std::put_time(Common::GetLocalTime(), "%Y/%m/%d %H:%M:%S %Z"), " | "sv),
		std::forward<ParamsT>(args)..., '\n');
}

template <Shell::ColorIndex kColor, std::size_t kLogStringBufferCapacity>
bool ::Logger::FormatWriteLine(char const* format, std::va_list argumentList)
{
	char logStringBuffer[kLogStringBufferCapacity];

	std::vsnprintf(logStringBuffer, kLogStringBufferCapacity, format, argumentList);

	if constexpr (kColor == Shell::ColorIndex::None)
	{
		WriteLine(logStringBuffer);
	}
	else
	{
		WriteLine(Shell::Color<kColor, char const*>(logStringBuffer));
	}

	return true;
}

template <Shell::ColorIndex kColor, std::size_t kLogStringBufferCapacity>
bool ::Logger::FormatWriteLine(char const* format, ...)
{
	std::va_list argumentList;
	va_start(argumentList, format);
	bool const result{ FormatWriteLine<kColor, kLogStringBufferCapacity>(format, argumentList) };
	va_end(argumentList);
	return result;
}

template <typename... ParamsT>
void ::Logger::InterpolateWriteLine(std::string_view const formatString, Common::Forward<ParamsT>... args)
{
	using namespace std::string_view_literals;

	std::lock_guard const lock(ms_Mutex);
	ms_GlobalLogger.Write(
		Shell::Cyan(std::put_time(Common::GetLocalTime(), "%Y/%m/%d %H:%M:%S %Z"), " | "sv));

	ms_GlobalLogger.InterpolateWrite(formatString, std::forward<ParamsT>(args)...);

	ms_GlobalLogger.Write('\n');
}
