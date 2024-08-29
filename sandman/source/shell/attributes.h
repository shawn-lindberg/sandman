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
		constexpr AttributeBundle GetPair(ForegroundIndex const foregroundColor,
									  BackgroundIndex const backgroundColor)
		{
			CursesColorID const column{ Common::IntCast(foregroundColor.m_Value) };
			CursesColorID const row{ Common::IntCast(backgroundColor.m_Value) };

			static_assert(kList.size() <= std::numeric_limits<int>::max(),
							  "Check that it's okay to downcast the `std::size_t` from `size()` to `int`");

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
		ColorMatrix::ForegroundIndex m_ColorIndex;

		// Combine an `Attr::ForegroundColor` with an `Attr`.
		[[nodiscard]] constexpr ForegroundColor operator|(AttributeBundle const attributes) const
		{
			return { this->m_Ancillary | attributes, m_ColorIndex };
		}

		[[nodiscard]] constexpr AttributeBundle BuildAttr() const
		{
			using namespace ColorMatrix;

			AttributeBundle const colorPair
			{
				GetPair(ForegroundIndex(m_ColorIndex), BackgroundIndex(Index::kBlack))
			};

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
		ColorMatrix::BackgroundIndex m_ColorIndex;

		[[nodiscard]] constexpr BackgroundColor operator|(AttributeBundle const attributes) const
		{
			return { this->m_Ancillary | attributes, m_ColorIndex };
		}

		[[nodiscard]] constexpr AttributeBundle BuildAttr() const
		{
			using namespace ColorMatrix;

			AttributeBundle const colorPair
			{
				GetPair(ForegroundIndex(Index::kWhite), BackgroundIndex(m_ColorIndex))
			};

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
		ColorMatrix::ForegroundIndex m_ForegroundColor;
		ColorMatrix::BackgroundIndex m_BackgroundColor;

		[[nodiscard]] constexpr ColorPair operator|(AttributeBundle const attributes) const
		{
			return { this->m_Ancillary | attributes, m_ForegroundColor, m_BackgroundColor };
		}

		[[nodiscard]] constexpr AttributeBundle BuildAttr() const
		{
			using namespace ColorMatrix;

			AttributeBundle const colorPair{ GetPair(m_ForegroundColor, m_BackgroundColor) };

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
		inline constexpr AttributeBundle::ForegroundColor Black  {Normal, ColorMatrix::ForegroundIndex{ColorMatrix::Index::kBlack  }};
		inline constexpr AttributeBundle::ForegroundColor Red    {Normal, ColorMatrix::ForegroundIndex{ColorMatrix::Index::kRed    }};
		inline constexpr AttributeBundle::ForegroundColor Green  {Normal, ColorMatrix::ForegroundIndex{ColorMatrix::Index::kGreen  }};
		inline constexpr AttributeBundle::ForegroundColor Yellow {Normal, ColorMatrix::ForegroundIndex{ColorMatrix::Index::kYellow }};
		inline constexpr AttributeBundle::ForegroundColor Blue   {Normal, ColorMatrix::ForegroundIndex{ColorMatrix::Index::kBlue   }};
		inline constexpr AttributeBundle::ForegroundColor Magenta{Normal, ColorMatrix::ForegroundIndex{ColorMatrix::Index::kMagenta}};
		inline constexpr AttributeBundle::ForegroundColor Cyan   {Normal, ColorMatrix::ForegroundIndex{ColorMatrix::Index::kCyan   }};
		inline constexpr AttributeBundle::ForegroundColor White  {Normal, ColorMatrix::ForegroundIndex{ColorMatrix::Index::kWhite  }};

		// Background color.
		inline constexpr AttributeBundle::BackgroundColor BackBlack  {Normal, ColorMatrix::BackgroundIndex{ColorMatrix::Index::kBlack  }};
		inline constexpr AttributeBundle::BackgroundColor BackRed    {Normal, ColorMatrix::BackgroundIndex{ColorMatrix::Index::kRed    }};
		inline constexpr AttributeBundle::BackgroundColor BackGreen  {Normal, ColorMatrix::BackgroundIndex{ColorMatrix::Index::kGreen  }};
		inline constexpr AttributeBundle::BackgroundColor BackYellow {Normal, ColorMatrix::BackgroundIndex{ColorMatrix::Index::kYellow }};
		inline constexpr AttributeBundle::BackgroundColor BackBlue   {Normal, ColorMatrix::BackgroundIndex{ColorMatrix::Index::kBlue   }};
		inline constexpr AttributeBundle::BackgroundColor BackMagenta{Normal, ColorMatrix::BackgroundIndex{ColorMatrix::Index::kMagenta}};
		inline constexpr AttributeBundle::BackgroundColor BackCyan   {Normal, ColorMatrix::BackgroundIndex{ColorMatrix::Index::kCyan   }};
		inline constexpr AttributeBundle::BackgroundColor BackWhite  {Normal, ColorMatrix::BackgroundIndex{ColorMatrix::Index::kWhite  }};
	}

} // namespace Shell
