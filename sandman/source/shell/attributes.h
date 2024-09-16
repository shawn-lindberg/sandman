#pragma once

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
	class AttributeBundle
	{
	public:

		// Using `chtype` instead of `attr_t` because
		// `attr_t` seems to be an extension to Curses.
		using Value = chtype;

		// `Attr::Value` is a type that should be large enough to hold at least `chtype`
		static_assert(std::numeric_limits<chtype>::max() <= std::numeric_limits<AttributeBundle::Value>::max());
		static_assert(std::numeric_limits<chtype>::min() >= std::numeric_limits<AttributeBundle::Value>::min());

		Value m_Value;

		[[nodiscard]] constexpr explicit AttributeBundle(Value const attributes = Value{ A_NORMAL })
			: m_Value{ attributes } {}

		static_assert(std::is_integral_v<Value>);

		template <typename IntT, typename = std::enable_if_t<std::is_integral_v<IntT>>>
		[[nodiscard]] constexpr explicit AttributeBundle(IntT const) = delete;

		// Combine this object with another attributes object.
		[[nodiscard]] constexpr AttributeBundle operator|(AttributeBundle const attributes) const
		{
			return AttributeBundle(this->m_Value bitor attributes.m_Value);
		}

		class ColorPair;
		class ForegroundColor;
		class BackgroundColor;
		template <typename...> class ObjectBundle;

		// Deduction guide.
		//
		// Deduce `ObjectBundle` type from arguments in `ObjectBundle` constructor.
		template <typename... ParametersT>
		ObjectBundle(AttributeBundle const, ParametersT&&...) -> ObjectBundle<ParametersT&&...>;

		// Wrap objects.
		template <typename... ParametersT> [[nodiscard]] constexpr
		ObjectBundle<ParametersT&&...> operator()(ParametersT&&... args) const
		{
			static_assert(sizeof...(args) > 0u,
							  "Calling this function without any arguments is probably a mistake.");
			return ObjectBundle(*this, std::forward<ParametersT>(args)...);
		}

	};

	namespace ColorMatrix
	{
		// Color numeric constants.
		enum struct Key : std::uint_least8_t
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
		constexpr CursesColorID GetColorID(Key const color)
		{
			switch (color)
			{
				case Key::kBlack  : return CursesColorID{COLOR_BLACK  };
				case Key::kRed    : return CursesColorID{COLOR_RED    };
				case Key::kGreen  : return CursesColorID{COLOR_GREEN  };
				case Key::kYellow : return CursesColorID{COLOR_YELLOW };
				case Key::kBlue   : return CursesColorID{COLOR_BLUE   };
				case Key::kMagenta: return CursesColorID{COLOR_MAGENTA};
				case Key::kCyan   : return CursesColorID{COLOR_CYAN   };
				case Key::kWhite  : return CursesColorID{COLOR_WHITE  };

				default: return CursesColorID{};
			}
		}

		// A list of color indices to make it easier to loop over them.
		inline constexpr std::array kColorList
		{
			Key::kBlack  ,
			Key::kRed    ,
			Key::kGreen  ,
			Key::kYellow ,
			Key::kBlue   ,
			Key::kMagenta,
			Key::kCyan   ,
			Key::kWhite  ,
		};

		// Gets the name of a color index. This is mainly for debugging.
		constexpr std::string_view GetName(Key const color)
		{
			switch (color)
			{
				using namespace std::string_view_literals;

				case Key::kBlack  : return "Black"   ""sv;
				case Key::kRed    : return "Red"     ""sv;
				case Key::kGreen  : return "Green"   ""sv;
				case Key::kYellow : return "Yellow"  ""sv;
				case Key::kBlue   : return "Blue"    ""sv;
				case Key::kMagenta: return "Magenta" ""sv;
				case Key::kCyan   : return "Cyan"    ""sv;
				case Key::kWhite  : return "White"   ""sv;

				default: return "null"sv;
			}
		}

		// Get an attribute value that has the foreground color and background color set.
		constexpr AttributeBundle GetPair(Key const foregroundColor, Key const backgroundColor)
		{
			CursesColorID const column{ GetColorID(foregroundColor) };
			CursesColorID const row   { GetColorID(backgroundColor) };

			static_assert(kColorList.size() <= std::numeric_limits<int>::max(),
							  "Check that it's okay to downcast the `std::size_t` from `size()` to `int`");

			// `COLOR_PAIR` takes an `int` as its argument.
			int const colorPairIndex{
				// Needs to be static cast to a `CursesColorID`
				// because operations on `short` integral types
				// will implicitly promote to non `short` types.
				static_cast<int>(
					int{ row } * int{ kColorList.size() } + int{ column }
					// Add an offset of 1 because color pair 0 is reserved as the default color pair.
					+ int{ 1 }
				)
			};

			return AttributeBundle(AttributeBundle::Value{ COLOR_PAIR(colorPairIndex) });
		}


	} // namespace ColorMatrix

	// Object wrapper.
	template <typename... ObjectsT>
	class [[nodiscard]] AttributeBundle::ObjectBundle
	{
	public:

		std::tuple<ObjectsT...> m_Objects;
		AttributeBundle m_Attributes;

		template <typename... ParametersT> [[nodiscard]]
		constexpr explicit ObjectBundle(AttributeBundle const attributes, ParametersT&&... args)
			: m_Objects(std::forward<ParametersT>(args)...), m_Attributes{ attributes } {}
	};

	// NOLINTBEGIN(readability-identifier-naming)

	template <typename>
	inline constexpr bool IsObjectBundle{ false };

	template <typename... ObjectsT>
	inline constexpr bool IsObjectBundle<AttributeBundle::ObjectBundle<ObjectsT...>>{ true };

	// NOLINTEND(readability-identifier-naming)

	class AttributeBundle::ForegroundColor
	{
	public:

		AttributeBundle m_Ancillary;
		ColorMatrix::Key m_ColorIndex;

		// Combine an `Attr::ForegroundColor` with an `Attr`.
		[[nodiscard]] constexpr ForegroundColor operator|(AttributeBundle const attributes) const
		{
			return { this->m_Ancillary | attributes, m_ColorIndex };
		}

		[[nodiscard]] constexpr AttributeBundle BuildAttr() const
		{
			using namespace ColorMatrix;

			AttributeBundle const colorPair(GetPair(m_ColorIndex, Key::kBlack));

			return m_Ancillary | colorPair;
		}

		template <typename... ParametersT>
		[[nodiscard]] constexpr ObjectBundle<ParametersT&&...> operator()(ParametersT&&... args) const
		{
			static_assert(sizeof...(args) > 0u,
							  "Calling this function without any arguments is probably a mistake.");

			return ObjectBundle(this->BuildAttr(), std::forward<ParametersT>(args)...);
		}
	};

	[[nodiscard]] constexpr
	AttributeBundle::ForegroundColor operator|(AttributeBundle const attributes, AttributeBundle::ForegroundColor const color)
	{
		return color | attributes;
	}

	class AttributeBundle::BackgroundColor
	{
	public:

		AttributeBundle m_Ancillary;
		ColorMatrix::Key m_ColorIndex;

		[[nodiscard]] constexpr BackgroundColor operator|(AttributeBundle const attributes) const
		{
			return { this->m_Ancillary | attributes, m_ColorIndex };
		}

		[[nodiscard]] constexpr AttributeBundle BuildAttr() const
		{
			using namespace ColorMatrix;

			AttributeBundle const colorPair(GetPair(Key::kWhite, m_ColorIndex));

			return m_Ancillary | colorPair;
		}

		template <typename... ParametersT>
		[[nodiscard]] constexpr ObjectBundle<ParametersT&&...> operator()(ParametersT&&... args) const
		{
			static_assert(sizeof...(args) > 0u,
							  "Calling this function without any arguments is probably a mistake.");

			return ObjectBundle(this->BuildAttr(), std::forward<ParametersT>(args)...);
		}
	};

	[[nodiscard]] constexpr
	AttributeBundle::BackgroundColor operator|(AttributeBundle const attributes, AttributeBundle::BackgroundColor const color)
	{
		return color | attributes;
	}

	class AttributeBundle::ColorPair
	{
	public:

		AttributeBundle m_Ancillary;
		ColorMatrix::Key m_ForegroundColor;
		ColorMatrix::Key m_BackgroundColor;

		[[nodiscard]] constexpr ColorPair operator|(AttributeBundle const attributes) const
		{
			return { this->m_Ancillary | attributes, m_ForegroundColor, m_BackgroundColor };
		}

		[[nodiscard]] constexpr AttributeBundle BuildAttr() const
		{
			using namespace ColorMatrix;

			AttributeBundle const colorPair(GetPair(m_ForegroundColor, m_BackgroundColor));

			return m_Ancillary | colorPair;
		}

		template <typename... ParametersT>
		[[nodiscard]] constexpr ObjectBundle<ParametersT&&...> operator()(ParametersT&&... args) const
		{
			static_assert(sizeof...(args) > 0u,
							  "Calling this function without any arguments is probably a mistake.");

			return ObjectBundle(this->BuildAttr(), std::forward<ParametersT>(args)...);
		}
	};

	[[nodiscard]] constexpr AttributeBundle::ColorPair
		operator|(AttributeBundle const attributes, AttributeBundle::ColorPair const colorPair)
	{
		return colorPair | attributes;
	}

	[[nodiscard]] constexpr AttributeBundle::ColorPair
		operator|(AttributeBundle::ForegroundColor const foregroundColor,
					 AttributeBundle::BackgroundColor const backgroundColor)
	{
		return { foregroundColor.m_Ancillary | backgroundColor.m_Ancillary,
					foregroundColor.m_ColorIndex, backgroundColor.m_ColorIndex };
	}

	[[nodiscard]] constexpr
	AttributeBundle::ColorPair operator|(AttributeBundle::BackgroundColor const backgroundColor,
									  AttributeBundle::ForegroundColor const foregroundColor)
	{
		return foregroundColor | backgroundColor;
	}

	inline namespace CharacterAttributeObjectWrapperFactories
	{
		// Character attribute.
		inline constexpr AttributeBundle Normal   (A_NORMAL   );
		inline constexpr AttributeBundle Highlight(A_STANDOUT );
		inline constexpr AttributeBundle Underline(A_UNDERLINE);
		inline constexpr AttributeBundle FlipColor(A_REVERSE  );
		inline constexpr AttributeBundle Blink    (A_BLINK    );
		inline constexpr AttributeBundle Dim      (A_DIM      );
		inline constexpr AttributeBundle Bold     (A_BOLD     );
		inline constexpr AttributeBundle Invisible(A_INVIS    );
		inline constexpr AttributeBundle Italic   (A_ITALIC   );

		// Foreground color.
		inline constexpr AttributeBundle::ForegroundColor Black  {Normal, ColorMatrix::Key::kBlack  };
		inline constexpr AttributeBundle::ForegroundColor Red    {Normal, ColorMatrix::Key::kRed    };
		inline constexpr AttributeBundle::ForegroundColor Green  {Normal, ColorMatrix::Key::kGreen  };
		inline constexpr AttributeBundle::ForegroundColor Yellow {Normal, ColorMatrix::Key::kYellow };
		inline constexpr AttributeBundle::ForegroundColor Blue   {Normal, ColorMatrix::Key::kBlue   };
		inline constexpr AttributeBundle::ForegroundColor Magenta{Normal, ColorMatrix::Key::kMagenta};
		inline constexpr AttributeBundle::ForegroundColor Cyan   {Normal, ColorMatrix::Key::kCyan   };
		inline constexpr AttributeBundle::ForegroundColor White  {Normal, ColorMatrix::Key::kWhite  };

		// Background color.
		inline constexpr AttributeBundle::BackgroundColor BackBlack  {Normal, ColorMatrix::Key::kBlack  };
		inline constexpr AttributeBundle::BackgroundColor BackRed    {Normal, ColorMatrix::Key::kRed    };
		inline constexpr AttributeBundle::BackgroundColor BackGreen  {Normal, ColorMatrix::Key::kGreen  };
		inline constexpr AttributeBundle::BackgroundColor BackYellow {Normal, ColorMatrix::Key::kYellow };
		inline constexpr AttributeBundle::BackgroundColor BackBlue   {Normal, ColorMatrix::Key::kBlue   };
		inline constexpr AttributeBundle::BackgroundColor BackMagenta{Normal, ColorMatrix::Key::kMagenta};
		inline constexpr AttributeBundle::BackgroundColor BackCyan   {Normal, ColorMatrix::Key::kCyan   };
		inline constexpr AttributeBundle::BackgroundColor BackWhite  {Normal, ColorMatrix::Key::kWhite  };
	}

} // namespace Shell
