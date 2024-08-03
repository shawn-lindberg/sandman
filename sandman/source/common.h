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

			[[nodiscard]] constexpr NonNull(FunctionT const functionPointer):
				m_Function{functionPointer != nullptr ? functionPointer : Simulacrum} {}

			[[gnu::always_inline]] [[nodiscard]] constexpr operator FunctionT() const
			{
				return m_Function;
			}
	};

	inline constexpr std::uint_least8_t kASCIIMin{ 0u }, kASCIIMax{ 127u };

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
		IsASCII(CharT const character)
	{
		if constexpr (std::is_signed_v<CharT>)
		{
			return character >= kASCIIMin and character <= kASCIIMax;
		}
		else
		{
			static_assert(std::is_unsigned_v<CharT>);
			return character <= kASCIIMax;
		}
	}

	namespace Enum
	{
		template <typename EnumT>
		[[gnu::always_inline]] [[nodiscard]] constexpr std::enable_if_t<std::is_enum_v<EnumT>,
																		std::underlying_type_t<EnumT>>
			IntCast(EnumT const value)
		{
			static_assert(std::is_integral_v<std::underlying_type_t<EnumT>>);
			return static_cast<std::underlying_type_t<EnumT>>(value);
		}
	}

	[[gnu::always_inline]] [[nodiscard]] inline std::tm* GetLocalTime()
	{
		auto const timePoint(std::chrono::system_clock::now());

		auto const arithmeticTimeValue{ std::chrono::system_clock::to_time_t(timePoint) };
		static_assert(std::is_arithmetic_v<decltype(arithmeticTimeValue)>);

		return std::localtime(&arithmeticTimeValue);
	}

	template <typename T>
	using Forward = T&&;

	template <typename T, T kValue>
	struct Implicitly { [[gnu::always_inline]] constexpr operator T() const { return kValue; } };

} // namespace Common
