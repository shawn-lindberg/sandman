#pragma once

namespace Common
{
	/// Utility type that is implicitly `constexpr` convertible to a value specified by the
	/// template parameter.
	template <auto kValue>
	struct Implicitly
	{
		[[gnu::always_inline]] [[nodiscard]] constexpr operator decltype(kValue)() const
		{
			return kValue;
		}
	};
}