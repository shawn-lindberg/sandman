#pragma once

#include "common/string.h"
#include "common/enum.h"
#include "common/forward_alias.h"
#include "common/box.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <type_traits>
#include <utility>

// This is the standard include directive for NCurses
// as noted in the "SYNOPSIS" section of the manual page `man 3NCURSES ncurses`.
#include <curses.h>

/// @brief This namespace serves to encapsulate state and functionality
/// relevant to the Shell user interface, and the usage of NCurses.
/// The `Shell` namespace assumes full control over the NCurses library,
/// so it is not recommended to interact with the NCurses library without going through
/// this `Shell` namespace.
///
namespace Shell
{
	using namespace std::string_view_literals;

	class [[nodiscard]] Lock
	{
		private: std::lock_guard<std::mutex> m_Lock;
		public: [[nodiscard]] explicit Lock();
	};

	/// @brief Initialize NCurses state and other logical state for managing the shell.
	///
	/// @attention Only call this function once. Call this function successfully before
	/// calling any other functions in the `Shell` namespace.
	///
	void Initialize();

	/// @brief Uninitialize NCurses state and other logical state.
	///
	/// @attention Only call this function once.
	/// Only call this function after a successfull call to `Shell::Initialize`.
	/// This does not necessarily clear the screen.
	///
	/// @note This frees the windows used by NCurses.
	///
	void Uninitialize();

	namespace Key
	{
		template <char kName, typename CharT = int>
		// Function-like constant.
		// NOLINTNEXTLINE(readability-identifier-naming)
		inline constexpr std::enable_if_t<std::is_integral_v<CharT>, CharT> Ctrl{kName bitand 0x1F};
	}

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

	// I don't make assumptions about what the Curses color macros
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

	struct ForegroundColor : Common::Box<ColorIndex>
	{
		[[nodiscard]] constexpr explicit ForegroundColor(ColorIndex const color) : Box{color} {};
	};

	struct BackgroundColor : Common::Box<ColorIndex>
	{
		[[nodiscard]] constexpr explicit BackgroundColor(ColorIndex const color) : Box{color} {};
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
		[[nodiscard]] constexpr Attr() : Attr(Value{0u}) {};

[[nodiscard]] constexpr Attr operator|(Attr const attributes) const
		{
			return Attr(this->m_Value | attributes.m_Value);
		}

				struct [[nodiscard]] Toggler;

		template <typename...> struct [[nodiscard]] Wrapper;

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

	struct [[nodiscard]] Attr::Toggler : Common::Box<bool>
	{
		Attr m_Attributes;
		constexpr explicit Toggler(Attr const attributes, bool const b)
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

	constexpr Attr GetColorPair(ForegroundColor const foregroundColor,
										 BackgroundColor const backgroundColor)
	{
		using Common::Enum::IntCast;
		using ColorPairID = decltype(GetColorID(std::declval<ColorIndex>()));

		ColorPairID const column{ IntCast(static_cast<ColorIndex>(foregroundColor)) };
		ColorPairID const row{ IntCast(static_cast<ColorIndex>(backgroundColor)) };

		// Check that it's okay to downcast the `std::size_t` from `size()` to `ColorPairID`.
		static_assert(kColorList.size() <= 8u and 8u <= std::numeric_limits<ColorPairID>::max());

		ColorPairID const colorPairIndex{
			// Needs to be static cast to a `ColorPairID`
			// because operations on `short` integral types
			// will implicitly convert to non `short` types.
			static_cast<ColorPairID>(
				row * ColorPairID{ kColorList.size() } + column
			)
		};

		return Attr(Attr::Value{ COLOR_PAIR(colorPairIndex) });
	}

	inline namespace CharacterAttributeConstants
	{
		using Fg = ForegroundColor;
		using Bg = BackgroundColor;

		// NOLINTBEGIN(readability-identifier-naming)

		inline constexpr Attr
			Normal     (A_NORMAL                                                      ),
			Highlight  (A_STANDOUT                                                    ),
			Underline  (A_UNDERLINE                                                   ),
			FlipColor  (A_REVERSE                                                     ),
			Blink      (A_BLINK                                                       ),
			Dim        (A_DIM                                                         ),
			Bold       (A_BOLD                                                        ),
			Invisible  (A_INVIS                                                       ),
			Italic     (A_ITALIC                                                      ),
			Black      (GetColorPair(Fg{ColorIndex::Black  }, Bg{ColorIndex::Black  })),
			Red        (GetColorPair(Fg{ColorIndex::Red    }, Bg{ColorIndex::Black  })),
			Green      (GetColorPair(Fg{ColorIndex::Green  }, Bg{ColorIndex::Black  })),
			Yellow     (GetColorPair(Fg{ColorIndex::Yellow }, Bg{ColorIndex::Black  })),
			Blue       (GetColorPair(Fg{ColorIndex::Blue   }, Bg{ColorIndex::Black  })),
			Magenta    (GetColorPair(Fg{ColorIndex::Magenta}, Bg{ColorIndex::Black  })),
			Cyan       (GetColorPair(Fg{ColorIndex::Cyan   }, Bg{ColorIndex::Black  })),
			White      (GetColorPair(Fg{ColorIndex::White  }, Bg{ColorIndex::Black  })),
			BackBlack  (GetColorPair(Fg{ColorIndex::Black  }, Bg{ColorIndex::Black  })),
			BackRed    (GetColorPair(Fg{ColorIndex::Black  }, Bg{ColorIndex::Red    })),
			BackGreen  (GetColorPair(Fg{ColorIndex::Black  }, Bg{ColorIndex::Green  })),
			BackYellow (GetColorPair(Fg{ColorIndex::Black  }, Bg{ColorIndex::Yellow })),
			BackBlue   (GetColorPair(Fg{ColorIndex::Black  }, Bg{ColorIndex::Blue   })),
			BackMagenta(GetColorPair(Fg{ColorIndex::Black  }, Bg{ColorIndex::Magenta})),
			BackCyan   (GetColorPair(Fg{ColorIndex::Black  }, Bg{ColorIndex::Cyan   })),
			BackWhite  (GetColorPair(Fg{ColorIndex::Black  }, Bg{ColorIndex::White  }));

		// NOLINTEND(readability-identifier-naming)
	}

	namespace LoggingWindow {

		void Refresh();

		void Write(chtype const character);

		void Write(char const* const string);

		template <typename CharT>
		std::enable_if_t<std::is_same_v<CharT, char> or std::is_same_v<CharT, chtype>, void>
			Write(std::basic_string_view<CharT> const string)
		{
			for (CharT const character : string)
			{
				Write(static_cast<chtype>(character));
			}
		}

		[[gnu::always_inline]] inline void Write(bool const booleanValue)
		{
			if (booleanValue == true)
			{
				Write("true");
			}
			else
			{
				Write("false");
			}
		}

		void Write(Attr const attributes) = delete;

		template <typename IntT>
		std::enable_if_t<std::is_integral_v<IntT>, void> Write(IntT const) = delete;

		void PushAttributes(Attr const attributes);

		void PopAttributes();

		void ClearAttributes();

		template <auto kAttributes=nullptr, typename T, typename... ParamsT>
		[[gnu::always_inline]] inline void Print(T const object, ParamsT const... arguments)
		{
			using AttributesT = std::decay_t<decltype(kAttributes)>;

			if constexpr (std::is_same_v<AttributesT, Attr const*>)
			{
				static_assert(kAttributes != nullptr);
				PushAttributes(*kAttributes);
			}
			else if constexpr (not std::is_same_v<AttributesT, std::nullptr_t>)
			{
				PushAttributes(Attr(kAttributes));
			}

			Write(object);

			if constexpr (sizeof...(arguments) > 0u)
			{
				return Print(arguments...);
			}
			else
			{
				ClearAttributes();
				Refresh();
			}
		}

		template <auto kAttributes=nullptr, typename... ParamsT>
		[[gnu::always_inline]] inline void Println(ParamsT const... arguments)
		{
			Print<kAttributes>(arguments..., chtype{'\n'});
		}

		/// @brief Get the pointer to the logging window.
		///
		/// @attention Do not call this function before having called `Shell::Initialize`
		/// successfully.
		///
		/// @return NCurses window pointer
		///
		/// @warning If `Shell::Initialize` has not been called successfully, this function likely
		/// returns `nullptr`. Otherwise, the pointer returned by this function is valid until
		/// `Shell::Uninitialize` is called.
		///
		/// @note The logging window is the region on the terminal where the logger outputs characters
		/// to. After `Shell::Initialize` is called successfully, this function always returns the
		/// same pointer.
		///
		[[deprecated("Manage this window through other functions.")]] [[nodiscard]] WINDOW* Get();

	} // namespace LoggingWindow

	namespace InputWindow
	{
		/// @brief The starting location of the cursor for the input window.
		///
		inline constexpr int kCursorStartY{ 1 }, kCursorStartX{ 2 };

		// The input window will have a height of 3.
		inline constexpr int kRowCount{ 3 };

		/// @brief Get the pointer to the input window.
		///
		/// @attention Do not call this function before having called `Shell::Initialize` successfully.
		///
		/// @return NCurses window pointer
		///
		/// @warning If `Shell::Initialize` has not been called successfully, this function likely
		/// returns returns `nullptr`. Otherwise, the pointer returned by this function is valid until
		/// `Shell::Uninitialize` is called.
		///
		/// @note The input window is the region on the terminal where the user input is echoed to.
		/// After `Shell::Initialize` is called successfully, this function always returns the same
		/// pointer.
		///
		[[deprecated("Manage this window through other functions.")]] [[nodiscard]] WINDOW* Get();

		using Buffer = Common::String<char, 1u << 7u>;
		Buffer const& GetBuffer();

		/// @brief Process a single key input from the user, if any.
		///
		/// @returns `true` if the "quit" command was processed, `false` otherwise.
		///
		bool ProcessSingleUserKey();
	}

}
