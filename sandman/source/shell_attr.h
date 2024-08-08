#pragma once

#include "common/forward_alias.h"
#include "common/box.h"
#include "common/enum.h"

#include <cstdint>
#include <curses.h>
#include <limits>
#include <utility>

namespace Shell {

	enum struct ColorIndex : std::uint_least8_t
	{
		Black   = 0u,
		Red     = 1u,
		Green   = 2u,
		Yellow  = 3u,
		Blue    = 4u,
		Magenta = 5u,
		Cyan    = 6u,
		White   = 7u,
	};

	struct Attr
	{
		using Value = chtype;

		// `Attr::Value` is a type that should be large enough to hold at least `chtype`
		static_assert(std::numeric_limits<chtype>::max() <= std::numeric_limits<Attr::Value>::max());
		static_assert(std::numeric_limits<chtype>::min() >= std::numeric_limits<Attr::Value>::min());

		Value m_Value;

		[[nodiscard]] constexpr explicit Attr(Value const attributes) : m_Value{ attributes } {}

		// The default constructor is implicit.
		[[nodiscard]] constexpr Attr() : Attr(Value{A_NORMAL}) {};

		[[nodiscard]] constexpr Attr operator|(Attr const attributes) const
		{
			return Attr(this->m_Value bitor attributes.m_Value);
		}

		struct ColorPair;
		struct ForegroundColor;
		struct BackgroundColor;
		template <typename...> struct Wrapper;

		template <typename... ParamsT>
		Wrapper(Attr const, Common::Forward<ParamsT>...) -> Wrapper<Common::Forward<ParamsT>...>;

		template <typename... ParamsT> [[nodiscard]] constexpr
		Wrapper<Common::Forward<ParamsT>...> operator()(Common::Forward<ParamsT>... args) const
		{
			static_assert(sizeof...(args) > 0u,
							  "Calling this function without any arguments is probably a mistake.");
			return Wrapper(*this, std::forward<ParamsT>(args)...);
		}

	};

	struct ForegroundColorIndex : Common::Box<ColorIndex> { using Box::Box; };
	struct BackgroundColorIndex : Common::Box<ColorIndex> { using Box::Box; };

	// Not making assumptions about what the Curses color macros
	// are defined as, so this function serves as a well defined mapping
	// from numeric constants to the Curses color macros.
	constexpr signed short int GetColorID(ColorIndex const color)
	{
		switch (color)
		{
			case ColorIndex::Black  : return COLOR_BLACK  ;
			case ColorIndex::Red    : return COLOR_RED    ;
			case ColorIndex::Green  : return COLOR_GREEN  ;
			case ColorIndex::Yellow : return COLOR_YELLOW ;
			case ColorIndex::Blue   : return COLOR_BLUE   ;
			case ColorIndex::Magenta: return COLOR_MAGENTA;
			case ColorIndex::Cyan   : return COLOR_CYAN   ;
			case ColorIndex::White  : return COLOR_WHITE  ;

			default: return {};
		}
	}

	inline constexpr std::array kColorList{
		ColorIndex::Black  ,
		ColorIndex::Red    ,
		ColorIndex::Green  ,
		ColorIndex::Yellow ,
		ColorIndex::Blue   ,
		ColorIndex::Magenta,
		ColorIndex::Cyan   ,
		ColorIndex::White  ,
	};

	constexpr Attr GetColorPair(ForegroundColorIndex const foregroundColor,
										 BackgroundColorIndex const backgroundColor)
	{
		using Common::Enum::IntCast;
		using ColorPairID = decltype(GetColorID(std::declval<ColorIndex>()));

		ColorPairID const column{ IntCast(foregroundColor.m_Value) };
		ColorPairID const row{ IntCast(backgroundColor.m_Value) };

		// Check that it's okay to downcast the `std::size_t` from `size()` to `ColorPairID`.
		static_assert(kColorList.size() <= 8u and 8u <= std::numeric_limits<ColorPairID>::max());

		ColorPairID const colorPairIndex{
			// Needs to be static cast to a `ColorPairID`
			// because operations on `short` integral types
			// will implicitly promote to non `short` types.
			static_cast<ColorPairID>(row * ColorPairID{ kColorList.size() } + column)
		};

		return Attr(Attr::Value{ COLOR_PAIR(colorPairIndex) });
	}

	template <typename... ObjectsT>
	struct [[nodiscard]] Attr::Wrapper
	{
		std::tuple<ObjectsT...> m_Objects;
		Attr m_Attributes;

		template <typename... ParamsT> [[nodiscard]]
		constexpr explicit Wrapper(Attr const attributes, Common::Forward<ParamsT>... args)
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
		ColorIndex m_ColorIndex;

		[[nodiscard]] constexpr ForegroundColor operator|(Attr const attributes) const
		{
			return { this->m_Ancillary | attributes, m_ColorIndex };
		}

		[[nodiscard]] constexpr Attr BuildAttr() const
		{
			ForegroundColorIndex const foregroundColor(m_ColorIndex);
			BackgroundColorIndex const backgroundColor(ColorIndex::Black);
			Attr const colorPair{ GetColorPair(foregroundColor, backgroundColor) };
			return m_Ancillary | colorPair;
		}

		template <typename... ParamsT>
		[[nodiscard]] constexpr Wrapper<Common::Forward<ParamsT>...>
			operator()(Common::Forward<ParamsT>... args) const
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
		ColorIndex m_ColorIndex;

		[[nodiscard]] constexpr BackgroundColor operator|(Attr const attributes) const
		{
			return { this->m_Ancillary | attributes, m_ColorIndex };
		}

		[[nodiscard]] constexpr Attr BuildAttr() const
		{
			ForegroundColorIndex const foregroundColor(ColorIndex::White);
			BackgroundColorIndex const backgroundColor(m_ColorIndex);
			Attr const colorPair{ GetColorPair(foregroundColor, backgroundColor) };
			return m_Ancillary | colorPair;
		}

		template <typename... ParamsT>
		[[nodiscard]] constexpr Wrapper<Common::Forward<ParamsT>...>
			operator()(Common::Forward<ParamsT>... args) const
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
		std::underlying_type_t<ColorIndex> m_ColorIndexPair;

		[[nodiscard]] constexpr ColorPair operator|(Attr const attributes) const
		{
			return { this->m_Ancillary | attributes, m_ColorIndexPair };
		}

		[[nodiscard]] constexpr Attr BuildAttr() const
		{
			ForegroundColorIndex const foregroundColor(
				static_cast<ColorIndex>(m_ColorIndexPair bitand 0b000111));
			BackgroundColorIndex const backgroundColor(
				static_cast<ColorIndex>((m_ColorIndexPair bitand 0b111000) >> 3u));
			Attr const colorPair{ GetColorPair(foregroundColor, backgroundColor) };
			return m_Ancillary | colorPair;
		}

		template <typename... ParamsT>
		[[nodiscard]] constexpr Wrapper<Common::Forward<ParamsT>...>
			operator()(Common::Forward<ParamsT>... args) const
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

		std::underlying_type_t<ColorIndex> const colorIndexPair{
			static_cast<std::underlying_type_t<ColorIndex>>(
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

	inline namespace CharacterAttributeConstants
	{

		using Fg = ForegroundColorIndex;
		using Bg = BackgroundColorIndex;

		// NOLINTBEGIN(readability-identifier-naming)

		inline constexpr Attr
			Normal     (A_NORMAL                   ),
			Highlight  (A_STANDOUT                 ),
			Underline  (A_UNDERLINE                ),
			FlipColor  (A_REVERSE                  ),
			Blink      (A_BLINK                    ),
			Dim        (A_DIM                      ),
			Bold       (A_BOLD                     ),
			Invisible  (A_INVIS                    ),
			Italic     (A_ITALIC                   );

		inline constexpr Attr::ForegroundColor
			Black      {Normal, ColorIndex::Black  },
			Red        {Normal, ColorIndex::Red    },
			Green      {Normal, ColorIndex::Green  },
			Yellow     {Normal, ColorIndex::Yellow },
			Blue       {Normal, ColorIndex::Blue   },
			Magenta    {Normal, ColorIndex::Magenta},
			Cyan       {Normal, ColorIndex::Cyan   },
			White      {Normal, ColorIndex::White  };

		inline constexpr Attr::BackgroundColor
			BackBlack  {Normal, ColorIndex::Black  },
			BackRed    {Normal, ColorIndex::Red    },
			BackGreen  {Normal, ColorIndex::Green  },
			BackYellow {Normal, ColorIndex::Yellow },
			BackBlue   {Normal, ColorIndex::Blue   },
			BackMagenta{Normal, ColorIndex::Magenta},
			BackCyan   {Normal, ColorIndex::Cyan   },
			BackWhite  {Normal, ColorIndex::White  };

		// NOLINTEND(readability-identifier-naming)
	}

} // namespace Shell
