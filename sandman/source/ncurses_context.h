#pragma once

#include <type_traits>

/*
	This is the standard include directive for NCurses
	as noted in the "SYNOPSIS" section of the manual page `man 3NCURSES ncurses`.
*/
#include <curses.h>

namespace NCurses
{

	WINDOW* GetLoggingWindow();

	WINDOW* GetInputWindow();

	void Initialize();

	void Uninitialize();

} // namespace NCurses
