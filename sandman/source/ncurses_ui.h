#pragma once

#include "char_buffer.h"

#include <array>
#include <cstdint>
#include <type_traits>
#include <mutex>

// This is the standard include directive for NCurses
// as noted in the "SYNOPSIS" section of the manual page `man 3NCURSES ncurses`.
#include <curses.h>

namespace Common::Enum
{
	template <typename EnumT>
	[[gnu::always_inline]] constexpr std::enable_if_t<std::is_enum_v<EnumT>, std::underlying_type_t<EnumT>>
	IntCast(EnumT const p_Value)
	{
		return static_cast<std::underlying_type_t<EnumT>>(p_Value);
	}
}

/// @brief This namespace serves to encapsulate state and functionality
/// relevant to the usage of NCurses.
///
namespace NCurses
{
	using namespace std::string_view_literals;
	using namespace Common;

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
		template <char t_Name, typename CharT=int>
		inline constexpr std::enable_if_t<std::is_integral_v<CharT>, CharT> Ctrl{ t_Name bitand 0x1F };
	}

	enum struct ColorIndex : int
	{
		NONE    = 0,
		BLACK   = 1,
		RED     = 2,
		GREEN   = 3,
		YELLOW  = 4,
		BLUE    = 5,
		MAGENTA = 6,
		CYAN    = 7,
		WHITE   = 8,
	};

	struct Attr { int m_Value; bool m_Flag; };

	template <ColorIndex t_ColorIndex, typename... ParamsT>
	struct Color
	{
		static_assert(t_ColorIndex != ColorIndex::NONE);
		static_assert(t_ColorIndex >= ColorIndex{1} and t_ColorIndex <= ColorIndex{8});
		static constexpr int s_AttributeValue{ COLOR_PAIR(Enum::IntCast(t_ColorIndex)) };
		static constexpr Attr On{s_AttributeValue, true};
		static constexpr Attr Off{s_AttributeValue, false};
		std::tuple<ParamsT const&...> objects;
		[[nodiscard]] explicit Color(ParamsT const&... args): objects(args...) {}
	};

	template <typename T> inline constexpr bool IsColor{false};

	template <ColorIndex t_ColorIndex, typename... ParamsT>
	inline constexpr bool IsColor<Color<t_ColorIndex, ParamsT...>>{true};

	template <typename... ParamsT> [[gnu::always_inline]] [[nodiscard]] inline auto Black  (ParamsT const&... args) { return Color<ColorIndex::BLACK  , ParamsT...>(args...); }
	template <typename... ParamsT> [[gnu::always_inline]] [[nodiscard]] inline auto Red    (ParamsT const&... args) { return Color<ColorIndex::RED    , ParamsT...>(args...); }
	template <typename... ParamsT> [[gnu::always_inline]] [[nodiscard]] inline auto Green  (ParamsT const&... args) { return Color<ColorIndex::GREEN  , ParamsT...>(args...); }
	template <typename... ParamsT> [[gnu::always_inline]] [[nodiscard]] inline auto Yellow (ParamsT const&... args) { return Color<ColorIndex::YELLOW , ParamsT...>(args...); }
	template <typename... ParamsT> [[gnu::always_inline]] [[nodiscard]] inline auto Blue   (ParamsT const&... args) { return Color<ColorIndex::BLUE   , ParamsT...>(args...); }
	template <typename... ParamsT> [[gnu::always_inline]] [[nodiscard]] inline auto Magenta(ParamsT const&... args) { return Color<ColorIndex::MAGENTA, ParamsT...>(args...); }
	template <typename... ParamsT> [[gnu::always_inline]] [[nodiscard]] inline auto Cyan   (ParamsT const&... args) { return Color<ColorIndex::CYAN   , ParamsT...>(args...); }
	template <typename... ParamsT> [[gnu::always_inline]] [[nodiscard]] inline auto White  (ParamsT const&... args) { return Color<ColorIndex::WHITE  , ParamsT...>(args...); }

	namespace LoggingWindow {
		
		void Refresh();

		void Write(Attr const p_CharacterAttribute);
		void Write(chtype const p_Character);
		void Write(char const* const p_String);
		void Write(std::string_view const p_String);

		template <typename BoolT>
		std::enable_if_t<std::is_same_v<BoolT, bool>, void> Write(BoolT const p_Boolean)
		{
			if (p_Boolean == true)
				Write("true");
			else
				Write("false");
		}

		template <typename T, typename... ParamsT>
		[[gnu::always_inline]] inline void Print(T const p_Object, ParamsT const... p_Arguments)
		{
			Write(p_Object);
			if constexpr (sizeof...(p_Arguments) > 0u) Print(p_Arguments...);
			else Refresh();
		}

		template <typename... ParamsT>
		[[gnu::always_inline]] inline void Println(ParamsT const... p_Arguments)
		{
			Print(p_Arguments..., '\n');
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
		inline constexpr int CURSOR_START_Y{ 1 }, CURSOR_START_X{ 2 };

		// The input window will have a height of 3.
		inline constexpr int ROW_COUNT{ 3 };

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

		using Buffer = CharBuffer<char, 128u>;
		Buffer const& GetBuffer();

		/// @brief Get keyboard input.
		///
		/// @param p_KeyboardInputBuffer (input/output) The input buffer.
		/// @param p_KeyboardInputBufferSize (input/output) How much of the input buffer is in use.
		/// @param p_KeyboardInputBufferCapacity The capacity of the input buffer.
		///
		/// @returns `true` if the "quit" command was processed, `false` otherwise.
		///
		bool ProcessKey();
	}

} // namespace NCurses

namespace Common
{
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

} // namespace Common
