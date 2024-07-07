#include "ncurses_context.h"

namespace NCurses
{

	static WINDOW* s_loggingWindow = nullptr;
	WINDOW* GetLoggingWindow()
	{
		return s_loggingWindow;
	}

	static WINDOW* s_inputWindow = nullptr;
	WINDOW* GetInputWindow()
	{
		return s_inputWindow;
	}

	static void DefaultConfigureWindow(WINDOW* const p_window) {
		/* output options: `man 'outopts(3NCURSES)'` */
		{
			// Don't make the next call to `wrefresh` clear and redraw the screen completely.
			clearok(p_window, FALSE);

			// Terminal's insert line & delete line features are okay to use.
			idlok(p_window, TRUE);

			// Terminal's insert character & delete character features are okay to use.
			idcok(p_window, TRUE);

			// Every change to the window will not cause a refresh to the physical screen.
			immedok(p_window, FALSE);

			// Leave the hardware cursor wherever the update happens to leave it, saving time.
			#pragma message "`leaveok`?"
			leaveok(p_window, FALSE);

			// Don't scroll when cursor is moved off the edge of the window.
			scrollok(p_window, FALSE);
		}

		/* input options: `man 'inopts(3NCURSES)'` */
		{
			// Don't flush everything on interrupts.
			intrflush(p_window, FALSE);

			// Enable functon keys (F1, F2, ...), arrow keys, etc.
			keypad(p_window, TRUE);

			// Make `getch` non-blocking.
			nodelay(p_window, TRUE);

			// Don't set a timer to wait for the next character when interpreting an escape sequence.
			notimeout(p_window, TRUE);
		}
	}

}

void NCurses::Initialize()
{
	// Initialize NCurses.
	initscr();

	/* input options that don't take a window pointer: `man 'inopts(3NCURSES)'` */
	{
		/*
			Don't have to wait for newlines in order to qget characters.
			Control characters are treated normally.
			Control characters on Linux include Ctrl+Z (suspend), Ctrl+C (quit), etc.

			It is not recomended to call `raw` or `noraw` after setting `cbreak` or `nocbreak`
			according; see "NOTES" section of `man 'inopts(3NCURSES)'`.
		*/
		cbreak();

		// Echo the characters typed by the user into the terminal.
		#pragma message "`echo`?"
		noecho();

		// No newlines; translate newline characters to carriage return characters.
		nonl();
	}

	// Configure standard screen window.
	DefaultConfigureWindow(stdscr);

	int static constexpr s_InputWindowRowCount{3};

	/* configure logging window */
	{
		s_loggingWindow = newwin(
			/* height (line count) */ LINES - s_InputWindowRowCount,
			/* width */ COLS,
			/* upper corner y */ 0,
			/* left-hand corner x */ 0
		);

		DefaultConfigureWindow(s_loggingWindow);

		// Scroll when cursor is moved off the edge of the window.
		scrollok(s_loggingWindow, TRUE);
	}

	/* configure input window */
	{
		s_inputWindow = newwin(
			/* height (line count) */ s_InputWindowRowCount,
			/* width */ COLS,
			/* upper corner y */ LINES - s_InputWindowRowCount,
			/* left-hand corner x */ 0
		);

		DefaultConfigureWindow(s_inputWindow);

		box(s_inputWindow, 0/* use default vertical character */, 0/* use default horizontal character */);
		wmove(s_inputWindow, 1, 2);
	}
}

void NCurses::Uninitialize()
{
	delwin(s_loggingWindow);
	delwin(s_inputWindow);
	endwin();
}
