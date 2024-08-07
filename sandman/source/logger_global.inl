#include "logger.h"

#include "common/get_local_time.h"

#include <iomanip>

template <typename... ParamsT>
[[gnu::always_inline]] inline void ::Logger::WriteLine(Common::Forward<ParamsT>... args)
{
	using namespace std::string_view_literals;

	std::lock_guard const lock(ms_Mutex);
	ms_GlobalLogger.Write(
		Shell::Cyan(std::put_time(::Common::GetLocalTime(), "%Y/%m/%d %H:%M:%S %Z"), " | "sv),
		std::forward<ParamsT>(args)..., '\n');
}

template <Shell::ColorIndex kColor, std::size_t kLogStringBufferCapacity>
bool ::Logger::FormatWriteLine(char const* format, std::va_list argumentList)
{
	char logStringBuffer[kLogStringBufferCapacity];

	std::vsnprintf(logStringBuffer, kLogStringBufferCapacity, format, argumentList);

	WriteLine(Shell::Color<kColor, char const*>(logStringBuffer));

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
