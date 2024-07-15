#pragma once

#include <cstdint>
#include <type_traits>

// This is the standard include directive for NCurses
// as noted in the "SYNOPSIS" section of the manual page `man 3NCURSES ncurses`.
#include <curses.h>

/// @brief This namespace serves to encapsulate state and functionality
/// relevant to the usage of NCurses.
///
namespace NCurses
{

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
	[[nodiscard]] WINDOW* GetLoggingWindow();

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
	[[nodiscard]] WINDOW* GetInputWindow();

	/// @brief The starting location of the cursor for the input window.
	///
	inline constexpr int INPUT_WINDOW_CURSOR_START_Y{ 1 }, INPUT_WINDOW_CURSOR_START_X{ 2 };

	/// @brief Initialize NCurses state.
	///
	/// @attention Only call this function once. Call this function successfully before
	/// calling any other functions in the `NCurses` namespace.
	///
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

	/// @brief Get keyboard input.
	///
	/// @param p_KeyboardInputBuffer (input/output) The input buffer.
	/// @param p_KeyboardInputBufferSize (input/output) How much of the input buffer is in use.
	/// @param p_KeyboardInputBufferCapacity The capacity of the input buffer.
	///
	/// @returns `true` if the "quit" command was processed, `false` otherwise.
	///
	bool ProcessKeyboardInput(char* p_KeyboardInputBuffer, unsigned int& p_KeyboardInputBufferSize,
									  unsigned int const p_KeyboardInputBufferCapacity);

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
	template <typename IntT>
	[[gnu::always_inline]] [[nodiscard]] constexpr std::enable_if_t<std::is_integral_v<IntT>, bool>
		IsASCII(IntT const p_Character)
	{
		if constexpr (std::is_signed_v<IntT>)
		{
			return p_Character >= ASCII_MIN and p_Character <= ASCII_MAX;
		}
		else
		{
			static_assert(std::is_unsigned_v<IntT>);
			return p_Character <= ASCII_MAX;
		}
	}

} // namespace NCurses
