#pragma once

#include <type_traits>

namespace Common
{

	/// Shorthand for `static_cast<std::underlying_type_t<EnumT>>(enumValue)`.
	template <typename EnumT>
	[[gnu::always_inline]] [[nodiscard]] constexpr std::enable_if_t<std::is_enum_v<EnumT>,
																						 std::underlying_type_t<EnumT>>
		IntCast(EnumT const enumValue)
	{
		static_assert(std::is_integral_v<std::underlying_type_t<EnumT>>);
		return static_cast<std::underlying_type_t<EnumT>>(enumValue);
	}
}