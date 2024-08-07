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

	if (ms_GlobalLogger.HasScreenEchoEnabled())
	{
		::Shell::LoggingWindow::ClearAttributes();
		::Shell::LoggingWindow::Refresh();
	}
}

template <Shell::Attr::Value kAttributes, std::size_t kStringBufferCapacity>
bool ::Logger::FormatWriteLine(char const* format, std::va_list argumentList)
{
	char logStringBuffer[kStringBufferCapacity];

	std::vsnprintf(logStringBuffer, kStringBufferCapacity, format, argumentList);

	static constexpr Shell::Attr kAttributeCallable(kAttributes);

	WriteLine(kAttributeCallable(logStringBuffer));

	return true;
}

template <Shell::Attr::Value kAttributes, std::size_t kStringBufferCapacity>
bool ::Logger::FormatWriteLine(char const* format, ...)
{
	std::va_list argumentList;
	va_start(argumentList, format);
	bool const result{ FormatWriteLine<kAttributes, kStringBufferCapacity>(format, argumentList) };
	va_end(argumentList);
	return result;
}
