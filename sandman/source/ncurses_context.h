#pragma once

#include <type_traits>

/*
	This is the standard include directive for NCurses
	as noted in the "SYNOPSIS" section of the manual page `man 3NCURSES ncurses`.
*/
#include <curses.h>

inline namespace Sandman {}

/**
 * @brief This namespace serves to encapsulate state and functionality
 * relevant to the usage of NCurses.
 */
namespace Sandman::NCurses
{

	/**
	 * @brief Get the pointer to the logging window.
	 *
	 * @attention Do not call this function before having called `NCurses::Initialize` successfully.
	 *
	 * @return NCurses window pointer
	 *
	 * @warning If `NCurses::Initialize` has not been called successfully, this function likely
	 * returns `nullptr`. Otherwise, the pointer returned by this function is valid until
	 * `NCurses::Uninitialize` is called.
	 *
	 * @note The logging window is the region on the terminal where the logger outputs characters to.
	 * After `NCurses::Initialize` is called successfully, this function always returns the same
	 * pointer.
	 *
	 */
	WINDOW* GetLoggingWindow();

	/**
	 * @brief Get the pointer to the input window.
	 *
	 * @attention Do not call this function before having called `NCurses::Initialize` successfully.
	 *
	 * @return NCurses window pointer
	 *
	 * @warning If `NCurses::Initialize` has not been called successfully, this function likely
	 * returns returns `nullptr`. Otherwise, the pointer returned by this function is valid until
	 * `NCurses::Uninitialize` is called.
	 *
	 * @note The input window is the region on the terminal where the user input is echoed to.
	 * After `NCurses::Initialize` is called successfully, this function always returns the same
	 * pointer.
	 *
	 */
	WINDOW* GetInputWindow();

	/**
	 * @brief The starting location of the cursor for the input window.
	 */
	inline constexpr int INPUT_WINDOW_CURSOR_START_Y{ 1 }, INPUT_WINDOW_CURSOR_START_X{ 2 };

	/**
	 * @brief Initialize NCurses state.
	 *
	 * @attention Only call this function once. Call this function successfully before
	 * calling any other functions in the `NCurses` namespace.
	 *
	 */
	void Initialize();

	/**
	 * @brief Uninitialize NCurses state.
	 *
	 * @attention Only call this function once.
	 * Only call this function after a sucessfull call to `NCurses::Initialize`.
	 * This does not necessarily clear the screen.
	 *
	 * @note This frees the windows used by NCurses.
	 *
	 */
	void Uninitialize();

} // namespace NCurses
