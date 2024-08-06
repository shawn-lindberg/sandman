#pragma once

#include <chrono>

namespace Common
{
	[[gnu::always_inline]] [[nodiscard]] inline std::tm* GetLocalTime()
	{
		auto const timePoint(std::chrono::system_clock::now());

		auto const arithmeticTimeValue{ std::chrono::system_clock::to_time_t(timePoint) };
		static_assert(std::is_arithmetic_v<decltype(arithmeticTimeValue)>);

		return std::localtime(&arithmeticTimeValue);
	}
}
