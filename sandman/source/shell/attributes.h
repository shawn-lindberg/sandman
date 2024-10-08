#pragma once

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

		Value m_value;

		[[nodiscard]] constexpr explicit AttributeBundle(Value const attributes = Value{ A_NORMAL })
			: m_value{ attributes } {}

		static_assert(std::is_integral_v<Value>);

		template <typename IntT, typename = std::enable_if_t<std::is_integral_v<IntT>>>
		[[nodiscard]] constexpr explicit AttributeBundle(IntT const) = delete;

		// Combine this object with another attributes object.
		[[nodiscard]] constexpr AttributeBundle operator|(AttributeBundle const attributes) const
		{
			return AttributeBundle(this->m_value bitor attributes.m_value);
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
		using Index = std::uint_least8_t;

		inline constexpr Index kBlack      = 0u;
		inline constexpr Index kRed        = 1u;
		inline constexpr Index kGreen      = 2u;
		inline constexpr Index kYellow     = 3u;
		inline constexpr Index kBlue       = 4u;
		inline constexpr Index kMagenta    = 5u;
		inline constexpr Index kCyan       = 6u;
		inline constexpr Index kWhite      = 7u;
		inline constexpr Index kColorCount = 8u;

		// This should be the same type that Curses `init_pair` takes as parameters.
		using CursesColorID = short signed int;

		// Careful, as this has a `std::string_view` which is a non-owning string type.
		struct [[nodiscard]] Record {
			CursesColorID cursesColorID;
			std::string_view name;
		};

		// Even though the Curses color macros may simply be defined
		// as integers zero though seven, not making assumptions
		// about what the Curses color macros are defined as,
		// so this function serves as a well defined mapping
		// from numeric constants to the Curses color macros.
		inline constexpr std::array kColorDatabase
		{
			Record{CursesColorID{COLOR_BLACK  }, "Black"   ""},
			Record{CursesColorID{COLOR_RED    }, "Red"     ""},
			Record{CursesColorID{COLOR_GREEN  }, "Green"   ""},
			Record{CursesColorID{COLOR_YELLOW }, "Yellow"  ""},
			Record{CursesColorID{COLOR_BLUE   }, "Blue"    ""},
			Record{CursesColorID{COLOR_MAGENTA}, "Magenta" ""},
			Record{CursesColorID{COLOR_CYAN   }, "Cyan"    ""},
			Record{CursesColorID{COLOR_WHITE  }, "White"   ""},
		};
		static_assert(kColorDatabase.size() == kColorCount);

		// Gets a Curses color ID from the color database.
		// If the color index is out-of-bounds, returns the default Curses color ID argument.
		[[nodiscard]] constexpr inline CursesColorID
			getCursesColorIDOrDefault(Index const colorIndex, CursesColorID const defaultCursesColorID)
		{
			static_assert(std::is_unsigned_v<Index> and std::is_integral_v<Index>);

			if (colorIndex >= kColorCount)
			{
				return defaultCursesColorID;
			}

			return kColorDatabase[colorIndex].cursesColorID;
		}

		// Get an attribute value that has the foreground color and background color set.
		constexpr AttributeBundle GetPair(Index const foregroundColor, Index const backgroundColor)
		{
			CursesColorID const column{ getCursesColorIDOrDefault(foregroundColor, COLOR_WHITE) };
			CursesColorID const row   { getCursesColorIDOrDefault(backgroundColor, COLOR_BLACK) };

			static_assert(kColorCount <= std::numeric_limits<int>::max(),
							  "Check that it's okay to downcast the `std::size_t` from `size()` to `int`");

			// `COLOR_PAIR` takes an `int` as its argument.
			int const colorPairIndex{
				// Needs to be static cast to a `CursesColorID`
				// because operations on `short` integral types
				// will implicitly promote to non `short` types.
				static_cast<int>(
					int{ row } * int{ kColorCount } + int{ column }
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

		std::tuple<ObjectsT...> m_objects;
		AttributeBundle m_attributes;

		template <typename... ParametersT> [[nodiscard]]
		constexpr explicit ObjectBundle(AttributeBundle const attributes, ParametersT&&... args)
			: m_objects(std::forward<ParametersT>(args)...), m_attributes{ attributes } {}
	};

	template <typename>
	inline constexpr bool IsObjectBundle{ false };

	template <typename... ObjectsT>
	inline constexpr bool IsObjectBundle<AttributeBundle::ObjectBundle<ObjectsT...>>{ true };

	class AttributeBundle::ForegroundColor
	{
	public:

		AttributeBundle m_ancillary;
		ColorMatrix::Index m_colorIndex;

		// Combine an `Attr::ForegroundColor` with an `Attr`.
		[[nodiscard]] constexpr ForegroundColor operator|(AttributeBundle const attributes) const
		{
			return { this->m_ancillary | attributes, m_colorIndex };
		}

		[[nodiscard]] constexpr AttributeBundle BuildAttr() const
		{
			using namespace ColorMatrix;

			AttributeBundle const colorPair(GetPair(m_colorIndex, ColorMatrix::kBlack));

			return m_ancillary | colorPair;
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

		AttributeBundle m_ancillary;
		ColorMatrix::Index m_colorIndex;

		[[nodiscard]] constexpr BackgroundColor operator|(AttributeBundle const attributes) const
		{
			return { this->m_ancillary | attributes, m_colorIndex };
		}

		[[nodiscard]] constexpr AttributeBundle BuildAttr() const
		{
			using namespace ColorMatrix;

			AttributeBundle const colorPair(GetPair(ColorMatrix::kWhite, m_colorIndex));

			return m_ancillary | colorPair;
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

		AttributeBundle m_ancillary;
		ColorMatrix::Index m_foregroundColor;
		ColorMatrix::Index m_backgroundColor;

		[[nodiscard]] constexpr ColorPair operator|(AttributeBundle const attributes) const
		{
			return { this->m_ancillary | attributes, m_foregroundColor, m_backgroundColor };
		}

		[[nodiscard]] constexpr AttributeBundle BuildAttr() const
		{
			using namespace ColorMatrix;

			AttributeBundle const colorPair(GetPair(m_foregroundColor, m_backgroundColor));

			return m_ancillary | colorPair;
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
		return { foregroundColor.m_ancillary | backgroundColor.m_ancillary,
					foregroundColor.m_colorIndex, backgroundColor.m_colorIndex };
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
		inline constexpr AttributeBundle::ForegroundColor Black  {Normal, ColorMatrix::kBlack  };
		inline constexpr AttributeBundle::ForegroundColor Red    {Normal, ColorMatrix::kRed    };
		inline constexpr AttributeBundle::ForegroundColor Green  {Normal, ColorMatrix::kGreen  };
		inline constexpr AttributeBundle::ForegroundColor Yellow {Normal, ColorMatrix::kYellow };
		inline constexpr AttributeBundle::ForegroundColor Blue   {Normal, ColorMatrix::kBlue   };
		inline constexpr AttributeBundle::ForegroundColor Magenta{Normal, ColorMatrix::kMagenta};
		inline constexpr AttributeBundle::ForegroundColor Cyan   {Normal, ColorMatrix::kCyan   };
		inline constexpr AttributeBundle::ForegroundColor White  {Normal, ColorMatrix::kWhite  };

		// Background color.
		inline constexpr AttributeBundle::BackgroundColor BackBlack  {Normal, ColorMatrix::kBlack  };
		inline constexpr AttributeBundle::BackgroundColor BackRed    {Normal, ColorMatrix::kRed    };
		inline constexpr AttributeBundle::BackgroundColor BackGreen  {Normal, ColorMatrix::kGreen  };
		inline constexpr AttributeBundle::BackgroundColor BackYellow {Normal, ColorMatrix::kYellow };
		inline constexpr AttributeBundle::BackgroundColor BackBlue   {Normal, ColorMatrix::kBlue   };
		inline constexpr AttributeBundle::BackgroundColor BackMagenta{Normal, ColorMatrix::kMagenta};
		inline constexpr AttributeBundle::BackgroundColor BackCyan   {Normal, ColorMatrix::kCyan   };
		inline constexpr AttributeBundle::BackgroundColor BackWhite  {Normal, ColorMatrix::kWhite  };
	}

} // namespace Shell
