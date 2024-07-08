#include "ncurses_context.h"

namespace NCurses
{

	// This window is where messages from the logger are written to.
	static WINDOW* s_loggingWindow = nullptr;

	WINDOW* GetLoggingWindow()
	{
		return s_loggingWindow;
	}

	// This window is where user input is echoed to.
	static WINDOW* s_inputWindow = nullptr;

	WINDOW* GetInputWindow()
	{
		return s_inputWindow;
	}

	static void DefaultConfigureWindow(WINDOW* const p_window)
	{
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

} // namespace NCurses

void NCurses::Initialize()
{
	// Initialize NCurses. This function exits the program on error!
	// This initializes the standard screen `stdscr` window.
	initscr();

	/* input options: `man 'inopts(3NCURSES)'` */
	// There options in particular do not take a window pointer.
	{
		/*
			Don't have to wait for newlines in order to get characters.
			Control characters are treated normally.
			Control characters on Linux include Ctrl+Z (suspend), Ctrl+C (quit), etc.

			Note: it is not recomended to call `raw` or `noraw` after setting `cbreak` or `nocbreak`
			according to "NOTES" section of `man 'inopts(3NCURSES)'`.
		*/
		cbreak();

		/*
			Do not echo the characters typed by the user into the terminal.

			Echo of the user input should be implemented manually for this project.
		*/
		noecho();

		// No newlines; translate newline characters to carriage return characters.
		nonl();
	}

	// Configure standard screen window.
	DefaultConfigureWindow(stdscr);

	// The input window will have a height of 3.
	int static constexpr s_InputWindowRowCount{ 3 };

	/* configure logging window */
	{
		s_loggingWindow = newwin(
			/* height (line count) */ LINES - s_InputWindowRowCount,
			/* width */ COLS,
			/* upper corner y */ 0,
			/* left-hand corner x */ 0);

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
			/* left-hand corner x */ 0);

		DefaultConfigureWindow(s_inputWindow);

		// Draw a border on the window.
		box(s_inputWindow, 0 /* use default vertical character */,
			 0 /* use default horizontal character */);

		// Move the cursor to the corner
		wmove(s_inputWindow, INPUT_WINDOW_CURSOR_START_Y, INPUT_WINDOW_CURSOR_START_X);
	}

	// Clear the screen.
	clear();
}

void NCurses::Uninitialize()
{
	delwin(s_loggingWindow);
	delwin(s_inputWindow);
	endwin();
}
