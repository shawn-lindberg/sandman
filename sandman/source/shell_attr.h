#pragma once

#include "common/box.h"
#include "common/enum.h"

#include <cstdint>
#include <curses.h>
#include <limits>
#include <utility>
#include <array>
#include <string_view>

namespace Shell
{

	// Represents a bundle of character attributes for the Curses library.
	struct Attr
	{
		// Using `chtype` instead of `attr_t` because
		// `attr_t` seems to be an extension to Curses.
		using Value = chtype;

		// `Attr::Value` is a type that should be large enough to hold at least `chtype`
		static_assert(std::numeric_limits<chtype>::max() <= std::numeric_limits<Attr::Value>::max());
		static_assert(std::numeric_limits<chtype>::min() >= std::numeric_limits<Attr::Value>::min());

		Value m_Value;

		[[nodiscard]] constexpr explicit Attr(Value const attributes = Value{ A_NORMAL })
			: m_Value{ attributes } {}

		static_assert(std::is_integral_v<Value>);

		template <typename IntT, typename = std::enable_if_t<std::is_integral_v<IntT>>>
		[[nodiscard]] constexpr explicit Attr(IntT const) = delete;

		// Combine this object with another attributes object.
		[[nodiscard]] constexpr Attr operator|(Attr const attributes) const
		{
			return Attr(this->m_Value bitor attributes.m_Value);
		}

		struct ColorPair;
		struct ForegroundColor;
		struct BackgroundColor;
		template <typename...> struct Wrapper;

		// Deduction guide.
		//
		// Deduce `Wrapper` type from arguments in `Wrapper` constructor.
		template <typename... ParamsT>
		Wrapper(Attr const, ParamsT&&...) -> Wrapper<ParamsT&&...>;

		// Wrap objects.
		template <typename... ParamsT> [[nodiscard]] constexpr
		Wrapper<ParamsT&&...> operator()(ParamsT&&... args) const
		{
			static_assert(sizeof...(args) > 0u,
							  "Calling this function without any arguments is probably a mistake.");
			return Wrapper(*this, std::forward<ParamsT>(args)...);
		}

	};

	namespace ColorMatrix
	{
		// Color numeric constants.
		enum struct Index : std::uint_least8_t
		{
			kBlack   = 0u,
			kRed     = 1u,
			kGreen   = 2u,
			kYellow  = 3u,
			kBlue    = 4u,
			kMagenta = 5u,
			kCyan    = 6u,
			kWhite   = 7u,
		};

		// This should be the same type that Curses `init_pair` takes as parameters.
		using CursesColorID = short signed int;

		// Even though the Curses color macros may simply be defined
		// as integers zero though seven, not making assumptions
		// about what the Curses color macros are defined as,
		// so this function serves as a well defined mapping
		// from numeric constants to the Curses color macros.
		constexpr CursesColorID GetColorID(Index const color)
		{
			switch (color)
			{
				case Index::kBlack  : return CursesColorID{COLOR_BLACK  };
				case Index::kRed    : return CursesColorID{COLOR_RED    };
				case Index::kGreen  : return CursesColorID{COLOR_GREEN  };
				case Index::kYellow : return CursesColorID{COLOR_YELLOW };
				case Index::kBlue   : return CursesColorID{COLOR_BLUE   };
				case Index::kMagenta: return CursesColorID{COLOR_MAGENTA};
				case Index::kCyan   : return CursesColorID{COLOR_CYAN   };
				case Index::kWhite  : return CursesColorID{COLOR_WHITE  };

				default: return CursesColorID{};
			}
		}

		// A list of color indices to make it easier to loop over them.
		inline constexpr std::array kList
		{
			Index::kBlack  ,
			Index::kRed    ,
			Index::kGreen  ,
			Index::kYellow ,
			Index::kBlue   ,
			Index::kMagenta,
			Index::kCyan   ,
			Index::kWhite  ,
		};

		// Gets the name of a color index. This is mainly for debugging.
		constexpr std::string_view GetName(Index const color)
		{
			using namespace std::string_view_literals;

			switch (color)
			{
				case Index::kBlack  : return "Black"   ""sv;
				case Index::kRed    : return "Red"     ""sv;
				case Index::kGreen  : return "Green"   ""sv;
				case Index::kYellow : return "Yellow"  ""sv;
				case Index::kBlue   : return "Blue"    ""sv;
				case Index::kMagenta: return "Magenta" ""sv;
				case Index::kCyan   : return "Cyan"    ""sv;
				case Index::kWhite  : return "White"   ""sv;

				default: return "null"sv;
			}
		}

		struct ForegroundIndex : Common::Box<Index> { using Box::Box; };
		struct BackgroundIndex : Common::Box<Index> { using Box::Box; };

		// Get an attribute value that has the foreground color and background color set.
		constexpr Attr GetPair(ForegroundIndex const foregroundColor,
									  BackgroundIndex const backgroundColor)
		{
			using Common::Enum::IntCast;

			CursesColorID const column{ IntCast(foregroundColor.m_Value) };
			CursesColorID const row{ IntCast(backgroundColor.m_Value) };

			// Check that it's okay to downcast the `std::size_t` from `size()` to `int`.
			static_assert(kList.size() <= 8u and 8u <= std::numeric_limits<int>::max());

			// `COLOR_PAIR` takes an `int` as its argument.
			int const colorPairIndex{
				// Needs to be static cast to a `CursesColorID`
				// because operations on `short` integral types
				// will implicitly promote to non `short` types.
				static_cast<int>(
					int{ row } * int{ kList.size() } + int{ column }
					// Add an offset of 1 because color pair 0 is reserved as the default color pair.
					+ int{ 1 }
				)
			};

			return Attr(Attr::Value{ COLOR_PAIR(colorPairIndex) });
		}


	} // namespace ColorMatrix

	// Object wrapper.
	template <typename... ObjectsT>
	struct [[nodiscard]] Attr::Wrapper
	{
		std::tuple<ObjectsT...> m_Objects;
		Attr m_Attributes;

		template <typename... ParamsT> [[nodiscard]]
		constexpr explicit Wrapper(Attr const attributes, ParamsT&&... args)
			: m_Objects(std::forward<ParamsT>(args)...), m_Attributes{ attributes } {}
	};

	// NOLINTBEGIN(readability-identifier-naming)

	template <typename>
	inline constexpr bool IsAttrWrapper{ false };

	template <typename... ObjectsT>
	inline constexpr bool IsAttrWrapper<Attr::Wrapper<ObjectsT...>>{ true };

	// NOLINTEND(readability-identifier-naming)

	struct Attr::ForegroundColor
	{
		Attr m_Ancillary;
		ColorMatrix::Index m_ColorIndex;

		// Combine an `Attr::ForegroundColor` with an `Attr`.
		[[nodiscard]] constexpr ForegroundColor operator|(Attr const attributes) const
		{
			return { this->m_Ancillary | attributes, m_ColorIndex };
		}

		[[nodiscard]] constexpr Attr BuildAttr() const
		{
			using namespace ColorMatrix;

			Attr const colorPair
			{
				GetPair(ForegroundIndex(m_ColorIndex), BackgroundIndex(Index::kBlack))
			};

			return m_Ancillary | colorPair;
		}

		template <typename... ParamsT>
		[[nodiscard]] constexpr Wrapper<ParamsT&&...> operator()(ParamsT&&... args) const
		{
			static_assert(sizeof...(args) > 0u,
							  "Calling this function without any arguments is probably a mistake.");

			return Wrapper(this->BuildAttr(), std::forward<ParamsT>(args)...);
		}
	};

	[[nodiscard]] constexpr
	Attr::ForegroundColor operator|(Attr const attributes, Attr::ForegroundColor const color)
	{
		return color | attributes;
	}

	struct Attr::BackgroundColor
	{
		Attr m_Ancillary;
		ColorMatrix::Index m_ColorIndex;

		[[nodiscard]] constexpr BackgroundColor operator|(Attr const attributes) const
		{
			return { this->m_Ancillary | attributes, m_ColorIndex };
		}

		[[nodiscard]] constexpr Attr BuildAttr() const
		{
			using namespace ColorMatrix;

			Attr const colorPair
			{
				GetPair(ForegroundIndex(Index::kWhite), BackgroundIndex(m_ColorIndex))
			};

			return m_Ancillary | colorPair;
		}

		template <typename... ParamsT>
		[[nodiscard]] constexpr Wrapper<ParamsT&&...> operator()(ParamsT&&... args) const
		{
			static_assert(sizeof...(args) > 0u,
							  "Calling this function without any arguments is probably a mistake.");

			return Wrapper(this->BuildAttr(), std::forward<ParamsT>(args)...);
		}
	};

	[[nodiscard]] constexpr
	Attr::BackgroundColor operator|(Attr const attributes, Attr::BackgroundColor const color)
	{
		return color | attributes;
	}

	struct Attr::ColorPair
	{
		Attr m_Ancillary;

		// Bit manipulations rely on the assumption that this assertion is true.
		static_assert([]() constexpr -> bool {
			ColorMatrix::Index maxColorIndex{ColorMatrix::kList.at(0u)};

			for (ColorMatrix::Index const colorIndex : ColorMatrix::kList)
			{
				maxColorIndex = std::max(colorIndex, maxColorIndex);
			}

			return ::Common::Enum::IntCast(maxColorIndex) == 0b111u;
		}());

		// Two color pair indices are packed into this value.
		std::underlying_type_t<ColorMatrix::Index> m_ColorIndexPair;

		[[nodiscard]] constexpr ColorPair operator|(Attr const attributes) const
		{
			return { this->m_Ancillary | attributes, m_ColorIndexPair };
		}

		[[nodiscard]] constexpr Attr BuildAttr() const
		{
			using namespace ColorMatrix;

			ForegroundIndex const foregroundColor(
				static_cast<Index>(m_ColorIndexPair bitand 0b000111u));

			BackgroundIndex const backgroundColor(
				static_cast<Index>((m_ColorIndexPair bitand 0b111000u) >> 3u));

			Attr const colorPair{ GetPair(foregroundColor, backgroundColor) };

			return m_Ancillary | colorPair;
		}

		template <typename... ParamsT>
		[[nodiscard]] constexpr Wrapper<ParamsT&&...> operator()(ParamsT&&... args) const
		{
			static_assert(sizeof...(args) > 0u,
							  "Calling this function without any arguments is probably a mistake.");

			return Wrapper(this->BuildAttr(), std::forward<ParamsT>(args)...);
		}
	};

	[[nodiscard]] constexpr
	Attr::ColorPair operator|(Attr const attributes, Attr::ColorPair const colorPair)
	{
		return colorPair | attributes;
	}

	[[nodiscard]] constexpr
	Attr::ColorPair operator|(Attr::ForegroundColor const foregroundColor,
									  Attr::BackgroundColor const backgroundColor)
	{
		using Common::Enum::IntCast;

		std::underlying_type_t<ColorMatrix::Index> const colorIndexPair
		{
			static_cast<std::underlying_type_t<ColorMatrix::Index>>
			(
				IntCast(foregroundColor.m_ColorIndex) bitor
				(IntCast(backgroundColor.m_ColorIndex) << 3u)
			)
		};

		return { foregroundColor.m_Ancillary | backgroundColor.m_Ancillary, colorIndexPair };
	}

	[[nodiscard]] constexpr
	Attr::ColorPair operator|(Attr::BackgroundColor const backgroundColor,
									  Attr::ForegroundColor const foregroundColor)
	{
		return foregroundColor | backgroundColor;
	}

	inline namespace CharacterAttributeObjectWrapperFactories
	{

		// NOLINTBEGIN(readability-identifier-naming)

		// Character attribute.
		inline constexpr Attr
			Normal     (A_NORMAL                           ),
			Highlight  (A_STANDOUT                         ),
			Underline  (A_UNDERLINE                        ),
			FlipColor  (A_REVERSE                          ),
			Blink      (A_BLINK                            ),
			Dim        (A_DIM                              ),
			Bold       (A_BOLD                             ),
			Invisible  (A_INVIS                            ),
			Italic     (A_ITALIC                           );

		// Foreground color.
		inline constexpr Attr::ForegroundColor
			Black      {Normal, ColorMatrix::Index::kBlack  },
			Red        {Normal, ColorMatrix::Index::kRed    },
			Green      {Normal, ColorMatrix::Index::kGreen  },
			Yellow     {Normal, ColorMatrix::Index::kYellow },
			Blue       {Normal, ColorMatrix::Index::kBlue   },
			Magenta    {Normal, ColorMatrix::Index::kMagenta},
			Cyan       {Normal, ColorMatrix::Index::kCyan   },
			White      {Normal, ColorMatrix::Index::kWhite  };

		// Background color.
		inline constexpr Attr::BackgroundColor
			BackBlack  {Normal, ColorMatrix::Index::kBlack  },
			BackRed    {Normal, ColorMatrix::Index::kRed    },
			BackGreen  {Normal, ColorMatrix::Index::kGreen  },
			BackYellow {Normal, ColorMatrix::Index::kYellow },
			BackBlue   {Normal, ColorMatrix::Index::kBlue   },
			BackMagenta{Normal, ColorMatrix::Index::kMagenta},
			BackCyan   {Normal, ColorMatrix::Index::kCyan   },
			BackWhite  {Normal, ColorMatrix::Index::kWhite  };

		// NOLINTEND(readability-identifier-naming)
	}

} // namespace Shell
