#include "logger.h"

#include "common/get_local_time.h"

#include <iomanip>

template <typename... ParamsT>
[[gnu::always_inline]] inline void ::Logger::WriteLine(ParamsT&&... args)
{
	using namespace std::string_view_literals;

	{
		std::lock_guard const lock(ms_Mutex);

		if (std::tm const* const localTime{ ::Common::GetLocalTime() }; localTime != nullptr)
		{
			ms_Logger.Write(Shell::Cyan(std::put_time(localTime, "%Y/%m/%d %H:%M:%S %Z | ")),
								 std::forward<ParamsT>(args)..., '\n');
		}
		else
		{
			ms_Logger.Write(Shell::Cyan("(missing local time) | "sv),
								 std::forward<ParamsT>(args)..., '\n');
		}
	}

	if (ms_Logger.HasScreenEchoEnabled())
	{
		::Shell::Lock const lock;
		::Shell::LoggingWindow::ClearAllAttributes();
		::Shell::LoggingWindow::Refresh();
	}
}

template <auto kAttributes, std::size_t kStringBufferCapacity>
bool ::Logger::WriteFormattedLine(char const* const format, std::va_list argumentList)
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
		WriteLine((*kAttributes)(logStringBuffer));
	}
	else
	{
		static_assert(std::is_same_v<AttributesT, Shell::Attr::Value>);
		WriteLine(Shell::Attr(kAttributes)(logStringBuffer));
	}

	return true;
}

template <auto kAttributes, std::size_t kStringBufferCapacity>
bool ::Logger::WriteFormattedLine(char const* const format, ...)
{
	std::va_list argumentList;
	va_start(argumentList, format);
	bool const result{ WriteFormattedLine<kAttributes, kStringBufferCapacity>(format, argumentList) };
	va_end(argumentList);
	return result;
}
