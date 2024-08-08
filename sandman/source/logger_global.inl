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

template <auto kAttributes, std::size_t kStringBufferCapacity>
bool ::Logger::FormatWriteLine(char const* format, std::va_list argumentList)
{
	char logStringBuffer[kStringBufferCapacity];

	std::vsnprintf(logStringBuffer, kStringBufferCapacity, format, argumentList);

	using AttributesT = std::decay_t<decltype(kAttributes)>;

	if constexpr (std::is_null_pointer_v<AttributesT>)
	{
		WriteLine(logStringBuffer);
	}
	else if constexpr (std::is_pointer_v<AttributesT>)
	{
		static_assert(kAttributes != nullptr);
		WriteLine(kAttributes->operator()(logStringBuffer));
	}
	else
	{
		static_assert(std::is_same_v<AttributesT, Shell::Attr::Value>);
		WriteLine(Shell::Attr(kAttributes)(logStringBuffer));
	}

	return true;
}

template <auto kAttributes, std::size_t kStringBufferCapacity>
bool ::Logger::FormatWriteLine(char const* format, ...)
{
	std::va_list argumentList;
	va_start(argumentList, format);
	bool const result{ FormatWriteLine<kAttributes, kStringBufferCapacity>(format, argumentList) };
	va_end(argumentList);
	return result;
}
