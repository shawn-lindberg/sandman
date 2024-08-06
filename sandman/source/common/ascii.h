#pragma once

#include <cstdint>
#include <type_traits>

namespace Common::ASCII
{
	inline constexpr std::uint_least8_t kMin{ 0u }, kMax{ 127u };

	/// @returns `true` if `character` is a value that fits into the ASCII character set, `false`
	/// otherwise.
	///
	/// @note This is an alternative to the POSIX function `isascii`. `isascii` is deprecated
	/// and depends on the locale. See `STANDARDS` section of `man 'isalpha(3)'`: "POSIX.1-2008 marks
	/// `isascii` as obsolete, noting that it cannot be used portably in a localized application."
	/// Furthermore, `isascii` is part of the POSIX standard, but is not part of the C or C++
	/// standard library.
	///
	template <typename CharT>
	[[gnu::always_inline]] [[nodiscard]] constexpr std::enable_if_t<std::is_integral_v<CharT>, bool>
		Match(CharT const character)
	{
		if constexpr (std::is_signed_v<CharT>)
		{
			return character >= kMin and character <= kMax;
		}
		else
		{
			static_assert(std::is_unsigned_v<CharT>);
			return character <= kMax;
		}
	}
}