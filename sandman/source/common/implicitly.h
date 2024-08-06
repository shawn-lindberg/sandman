#pragma once

namespace Common
{
	template <auto kValue>
	struct Implicitly
	{
		[[gnu::always_inline]] [[nodiscard]] constexpr operator decltype(kValue)() const
		{
			return kValue;
		}
	};
}