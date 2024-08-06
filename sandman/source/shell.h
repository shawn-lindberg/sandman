#pragma once

#include "common/char_buffer.h"
#include "common/enum.h"
#include "common/forward_alias.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <type_traits>

// This is the standard include directive for NCurses
// as noted in the "SYNOPSIS" section of the manual page `man 3NCURSES ncurses`.
#include <curses.h>

/// @brief This namespace serves to encapsulate state and functionality
/// relevant to the usage of NCurses. The `NCurses` namespace assumes full control over the Ncurses
/// library, so it is not recommended to interact with the NCurses library without using this
/// `NCurses` namespace.
///
namespace Shell
{
	using namespace std::string_view_literals;

	class Lock
	{
		private: std::lock_guard<std::mutex> m_Lock;
		public: [[nodiscard]] explicit Lock();
	};

	/// @brief Initialize NCurses state.
	///
	/// @attention Only call this function once. Call this function successfully before
	/// calling any other functions in the `NCurses` namespace.
	///
	void Initialize();

	/// @brief Uninitialize NCurses state.
	///
	/// @attention Only call this function once.
	/// Only call this function after a successfull call to `NCurses::Initialize`.
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

	enum struct ColorIndex : int
	{
		None    = 0,
		Black   = 1,
		Red     = 2,
		Green   = 3,
		Yellow  = 4,
		Blue    = 5,
		Magenta = 6,
		Cyan    = 7,
		White   = 8,
	};

	struct CharacterAttribute { int m_Value; bool m_Flag; };

	template <ColorIndex kColorIndex, typename... ObjectsT>
	struct Color
	{
		static_assert(kColorIndex != ColorIndex::None);
		static_assert(kColorIndex >= ColorIndex{ 1 } and kColorIndex <= ColorIndex{ 8 });

		static constexpr int kAttributeValue{ COLOR_PAIR(Common::Enum::IntCast(kColorIndex)) };
		static constexpr CharacterAttribute kOn{ kAttributeValue, true };
		static constexpr CharacterAttribute kOff{ kAttributeValue, false };

		std::tuple<ObjectsT...> m_Objects;

		template <typename... ParamsT>
		[[nodiscard]] explicit constexpr Color(Common::Forward<ParamsT>... args) : m_Objects(std::forward<ParamsT>(args)...) {}

		template <typename... ParamsT>
		[[deprecated("Prefer to use a color factory function.")]]
		static constexpr auto Make(Common::Forward<ParamsT>... args)
		{
			return Color<kColorIndex, Common::Forward<ParamsT>...>(std::forward<ParamsT>(args)...);
		}
	};

	// Deduction guide: deduce from forwarding reference arguments.
	template <ColorIndex kColorIndex, typename... ParamsT>
	Color(Common::Forward<ParamsT>...) -> Color<kColorIndex, Common::Forward<ParamsT>...>;

	// Function-like constant.
	// NOLINTBEGIN(readability-identifier-naming)

	template <typename T> inline constexpr bool IsColor{ false };
	template <ColorIndex kColorIndex, typename... ObjectsT>
	inline constexpr bool IsColor<Color<kColorIndex, ObjectsT...>>{ true };

	// NOLINTEND(readability-identifier-naming)

	inline namespace ColorFactoryFunctions {
		template <typename... ObjectsT> [[gnu::always_inline]] [[nodiscard]] constexpr auto Black  (Common::Forward<ObjectsT>... args) { return Color<ColorIndex::Black  , ObjectsT...>(std::forward<ObjectsT>(args)...); }
		template <typename... ObjectsT> [[gnu::always_inline]] [[nodiscard]] constexpr auto Red    (Common::Forward<ObjectsT>... args) { return Color<ColorIndex::Red    , ObjectsT...>(std::forward<ObjectsT>(args)...); }
		template <typename... ObjectsT> [[gnu::always_inline]] [[nodiscard]] constexpr auto Green  (Common::Forward<ObjectsT>... args) { return Color<ColorIndex::Green  , ObjectsT...>(std::forward<ObjectsT>(args)...); }
		template <typename... ObjectsT> [[gnu::always_inline]] [[nodiscard]] constexpr auto Yellow (Common::Forward<ObjectsT>... args) { return Color<ColorIndex::Yellow , ObjectsT...>(std::forward<ObjectsT>(args)...); }
		template <typename... ObjectsT> [[gnu::always_inline]] [[nodiscard]] constexpr auto Blue   (Common::Forward<ObjectsT>... args) { return Color<ColorIndex::Blue   , ObjectsT...>(std::forward<ObjectsT>(args)...); }
		template <typename... ObjectsT> [[gnu::always_inline]] [[nodiscard]] constexpr auto Magenta(Common::Forward<ObjectsT>... args) { return Color<ColorIndex::Magenta, ObjectsT...>(std::forward<ObjectsT>(args)...); }
		template <typename... ObjectsT> [[gnu::always_inline]] [[nodiscard]] constexpr auto Cyan   (Common::Forward<ObjectsT>... args) { return Color<ColorIndex::Cyan   , ObjectsT...>(std::forward<ObjectsT>(args)...); }
		template <typename... ObjectsT> [[gnu::always_inline]] [[nodiscard]] constexpr auto White  (Common::Forward<ObjectsT>... args) { return Color<ColorIndex::White  , ObjectsT...>(std::forward<ObjectsT>(args)...); }
	}

	inline namespace ColorStringLiterals
	{
		[[gnu::always_inline]] [[nodiscard]] constexpr auto operator""_black  (char const* const string, std::size_t const) { return Color<ColorIndex::Black  , char const*>(string); }
		[[gnu::always_inline]] [[nodiscard]] constexpr auto operator""_red    (char const* const string, std::size_t const) { return Color<ColorIndex::Red    , char const*>(string); }
		[[gnu::always_inline]] [[nodiscard]] constexpr auto operator""_green  (char const* const string, std::size_t const) { return Color<ColorIndex::Green  , char const*>(string); }
		[[gnu::always_inline]] [[nodiscard]] constexpr auto operator""_yellow (char const* const string, std::size_t const) { return Color<ColorIndex::Yellow , char const*>(string); }
		[[gnu::always_inline]] [[nodiscard]] constexpr auto operator""_blue   (char const* const string, std::size_t const) { return Color<ColorIndex::Blue   , char const*>(string); }
		[[gnu::always_inline]] [[nodiscard]] constexpr auto operator""_magenta(char const* const string, std::size_t const) { return Color<ColorIndex::Magenta, char const*>(string); }
		[[gnu::always_inline]] [[nodiscard]] constexpr auto operator""_cyan   (char const* const string, std::size_t const) { return Color<ColorIndex::Cyan   , char const*>(string); }
		[[gnu::always_inline]] [[nodiscard]] constexpr auto operator""_white  (char const* const string, std::size_t const) { return Color<ColorIndex::White  , char const*>(string); }
	}

	namespace LoggingWindow {

		void Refresh();

		void Write(CharacterAttribute const characterAttribute);
		void Write(chtype const character);
		void Write(char const* const string);
		void Write(std::string_view const string);

		template <typename BoolT>
		std::enable_if_t<std::is_same_v<BoolT, bool>, void> Write(BoolT const booleanValue)
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

		template <typename T, typename... ParamsT>
		[[gnu::always_inline]] inline void Print(T const object, ParamsT const... arguments)
		{
			Write(object);
			if constexpr (sizeof...(arguments) > 0u)
			{
				Print(arguments...);
			}
			else
			{
				Refresh();
			}
		}

		template <typename... ParamsT>
		[[gnu::always_inline]] inline void Println(ParamsT const... arguments)
		{
			Print(arguments..., '\n');
		}

		/// @brief Get the pointer to the logging window.
		///
		/// @attention Do not call this function before having called `NCurses::Initialize` successfully.
		///
		/// @return NCurses window pointer
		///
		/// @warning If `NCurses::Initialize` has not been called successfully, this function likely
		/// returns `nullptr`. Otherwise, the pointer returned by this function is valid until
		/// `NCurses::Uninitialize` is called.
		///
		/// @note The logging window is the region on the terminal where the logger outputs characters
		/// to. After `NCurses::Initialize` is called successfully, this function always returns the same
		/// pointer.
		///
		[[deprecated("Prefer using other functions to write to this window.")]] [[nodiscard]] WINDOW* Get();
	}

	namespace InputWindow
	{
		/// @brief The starting location of the cursor for the input window.
		///
		inline constexpr int kCursorStartY{ 1 }, kCursorStartX{ 2 };

		// The input window will have a height of 3.
		inline constexpr int kRowCount{ 3 };

		/// @brief Get the pointer to the input window.
		///
		/// @attention Do not call this function before having called `NCurses::Initialize` successfully.
		///
		/// @return NCurses window pointer
		///
		/// @warning If `NCurses::Initialize` has not been called successfully, this function likely
		/// returns returns `nullptr`. Otherwise, the pointer returned by this function is valid until
		/// `NCurses::Uninitialize` is called.
		///
		/// @note The input window is the region on the terminal where the user input is echoed to.
		/// After `NCurses::Initialize` is called successfully, this function always returns the same
		/// pointer.
		///
		[[deprecated]] [[nodiscard]] WINDOW* Get();

		using Buffer = Common::CharBuffer<char, 128u>;
		Buffer const& GetBuffer();

		/// @brief Process a single key input from the user, if any.
		///
		/// @returns `true` if the "quit" command was processed, `false` otherwise.
		///
		bool ProcessSingleUserKey();
	}

}
