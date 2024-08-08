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

		// Default constructor is implicit.
		[[nodiscard]] constexpr Attr() : Attr(Value{A_NORMAL}) {};

		[[nodiscard]] constexpr Attr operator|(Attr const attributes) const
		{
			return Attr(this->m_Value bitor attributes.m_Value);
		}

		struct ColorPair;
		struct ForegroundColor;
		struct BackgroundColor;
		struct Toggler;
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

	struct Attr::Toggler : Common::Box<bool>
	{
		Attr m_Attributes;
		[[nodiscard]] constexpr explicit Toggler(Attr const attributes, bool const b)
			: Box{b}, m_Attributes(attributes) {}
	};

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
		Attr m_Attributes;
		ColorIndex m_ColorIndex;

		[[nodiscard]] constexpr ForegroundColor operator|(Attr const attributes) const
		{
			return { this->m_Attributes | attributes, m_ColorIndex };
		}
	};

	[[nodiscard]] constexpr
	Attr::ForegroundColor operator|(Attr const attributes, Attr::ForegroundColor const color)
	{
		return color | attributes;
	}

	struct Attr::BackgroundColor
	{
		Attr m_Attributes;
		ColorIndex m_ColorIndex;

		[[nodiscard]] constexpr BackgroundColor operator|(Attr const attributes) const
		{
			return { this->m_Attributes | attributes, m_ColorIndex };
		}
	};

	[[nodiscard]] constexpr
	Attr::BackgroundColor operator|(Attr const attributes, Attr::BackgroundColor const color)
	{
		return color | attributes;
	}

	struct Attr::ColorPair
	{
		Attr m_Attributes;
		std::underlying_type_t<ColorIndex> m_ColorIndexPair;

		[[nodiscard]] constexpr ColorPair operator|(Attr const attributes) const
		{
			return { this->m_Attributes | attributes, m_ColorIndexPair };
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

		return { foregroundColor.m_Attributes | backgroundColor.m_Attributes, colorIndexPair };
	}

	[[nodiscard]] constexpr
	Attr::ColorPair operator|(Attr::BackgroundColor const backgroundColor,
									  Attr::ForegroundColor const foregroundColor)
	{
		return foregroundColor | backgroundColor;
	}
}
