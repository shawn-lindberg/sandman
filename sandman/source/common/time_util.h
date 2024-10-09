#pragma once

#include <chrono>

namespace Common
{
	/// @warning This function may not be thread-safe because
	/// it calls `std::localtime` which may not be thread-safe.
	/// @return Pointer to a static internal `std::tm` object on success, or null pointer otherwise.
	/// The structure may be shared between `std::gmtime`, `std::localtime`, and `std::ctime`, and
	/// may be overwritten on each invocation.
	[[nodiscard]] inline std::tm* GetLocalTime()
	{
		auto const timePoint(std::chrono::system_clock::now());

		std::time_t const arithmeticTimeValue{ std::chrono::system_clock::to_time_t(timePoint) };
		static_assert(std::is_arithmetic_v<decltype(arithmeticTimeValue)>);

		return std::localtime(&arithmeticTimeValue);
	}
}
