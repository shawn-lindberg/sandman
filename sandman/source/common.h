#pragma once

#include <chrono>
#include <iomanip>
#include <type_traits>

namespace Common
{
	template <typename T>
	class NonNull;

	template <typename ReturnT, typename... ParamsT>
	class NonNull<ReturnT (*)(ParamsT...)>
	{
		public:
			using FunctionT = ReturnT (*)(ParamsT...);
			static ReturnT Simulacrum(ParamsT...) { return ReturnT(); }

		private:
			FunctionT m_Function;

		public:
			[[nodiscard]] constexpr NonNull(): m_Function{Simulacrum} {}

			[[nodiscard]] constexpr NonNull(FunctionT const p_FunctionPointer):
				m_Function{p_FunctionPointer != nullptr ? p_FunctionPointer : Simulacrum} {}

			[[gnu::always_inline]] [[nodiscard]] constexpr operator FunctionT() const
			{
				return m_Function;
			}
	};

	inline constexpr std::uint_least8_t ASCII_MIN{ 0u }, ASCII_MAX{ 127u };

	/// @returns `true` if `p_Character` is a value that fits into the ASCII character set, `false`
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
		IsASCII(CharT const p_Character)
	{
		if constexpr (std::is_signed_v<CharT>)
		{
			return p_Character >= ASCII_MIN and p_Character <= ASCII_MAX;
		}
		else
		{
			static_assert(std::is_unsigned_v<CharT>);
			return p_Character <= ASCII_MAX;
		}
	}

	namespace Enum
	{
		template <typename EnumT>
		[[gnu::always_inline]] [[nodiscard]] constexpr std::enable_if_t<std::is_enum_v<EnumT>,
																		std::underlying_type_t<EnumT>>
			IntCast(EnumT const p_Value)
		{
			static_assert(std::is_integral_v<std::underlying_type_t<EnumT>>);
			return static_cast<std::underlying_type_t<EnumT>>(p_Value);
		}
	}

	[[gnu::always_inline]] [[nodiscard]] inline std::tm* GetLocalTime()
	{
		auto const l_TimePoint(std::chrono::system_clock::now());

		auto const l_ArithmeticTimeValue{ std::chrono::system_clock::to_time_t(l_TimePoint) };
		static_assert(std::is_arithmetic_v<decltype(l_ArithmeticTimeValue)>);

		return std::localtime(&l_ArithmeticTimeValue);
	}

} // namespace Common
