#include "ncurses_ui.h"

#include "command.h"
#include "logger.h"

#include <cstring>
#include <vector>

namespace NCurses
{

	// This window is where messages from the logger are written to.
	static WINDOW* s_LoggingWindow = nullptr;

	WINDOW* GetLoggingWindow()
	{
		return s_LoggingWindow;
	}

	// This window is where user input is echoed to.
	static WINDOW* s_InputWindow = nullptr;

	WINDOW* GetInputWindow()
	{
		return s_InputWindow;
	}

	// Configure a window with "sensible" defaults.
	[[gnu::nonnull]] static void ConfigureWindowDefaults(WINDOW* const p_Window)
	{
		// output options: `man 'outopts(3NCURSES)'`
		{
			// Don't make the next call to `wrefresh` clear and redraw the screen completely.
			clearok(p_Window, FALSE);

			// Terminal's insert line & delete line features are okay to use.
			idlok(p_Window, TRUE);

			// Terminal's insert character & delete character features are okay to use.
			idcok(p_Window, TRUE);

			// Every change to the window will not cause a refresh to the physical screen.
			immedok(p_Window, FALSE);

			// The UI seems to be less glitchy when this is set to true.
			leaveok(p_Window, TRUE);

			// Don't scroll when cursor is moved off the edge of the window.
			scrollok(p_Window, FALSE);
		}

		// input options: `man 'inopts(3NCURSES)'`
		{
			// Don't flush everything on interrupts.
			intrflush(p_Window, FALSE);

			// Enable functon keys (F1, F2, ...), arrow keys, etc.
			keypad(p_Window, TRUE);

			// Make `getch` non-blocking.
			nodelay(p_Window, TRUE);

			// Don't set a timer to wait for the next character when interpreting an escape sequence.
			notimeout(p_Window, TRUE);
		}
	}

	void Initialize()
	{
		// Initialize NCurses. This function exits the program on error!
		// This initializes the standard screen `stdscr` window.
		initscr();

		// Input options: `man 'inopts(3NCURSES)'`.
		// These options in particular do not take a window pointer.
		{
			// Don't have to wait for newlines in order to get characters.
			// Control characters are treated normally.
			// Control characters on Linux include Ctrl+Z (suspend), Ctrl+C (quit), etc.
			//
			// Note: it is not recomended to call `raw` or `noraw` after setting `cbreak` or `nocbreak`
			// according to "NOTES" section of `man 'inopts(3NCURSES)'`.
			cbreak();

			// Do not echo the characters typed by the user into the terminal by default.
			// Echo of the user input should be implemented manually for this project.
			noecho();

			// No newlines; translate newline characters to carriage return characters.
			nonl();
		}

		// Configure standard screen window.
		ConfigureWindowDefaults(stdscr);

		// The input window will have a height of 3.
		static constexpr int s_InputWindowRowCount{ 3 };

		// Configure logging window.
		{
			s_LoggingWindow = newwin(
				// height (line count)
				LINES - s_InputWindowRowCount,
				// width
				COLS,
				// upper corner y
				0,
				// left-hand corner x
				0);

			ConfigureWindowDefaults(s_LoggingWindow);

			// Scroll when cursor is moved off the edge of the window.
			scrollok(s_LoggingWindow, TRUE);
		}

		// Configure input window.
		{
			s_InputWindow = newwin(
				// height (line count)
				s_InputWindowRowCount,
				// width
				COLS,
				// upper corner y
				LINES - s_InputWindowRowCount,
				// left-hand corner x
				0);

			ConfigureWindowDefaults(s_InputWindow);

			// Draw a border on the window.
			box(s_InputWindow,
				 // use default vertical character
				 0,
				 // use default horizontal character
				 0);

			// Move the cursor to the corner.
			wmove(s_InputWindow, INPUT_WINDOW_CURSOR_START_Y, INPUT_WINDOW_CURSOR_START_X);
		}

		// Clear the screen.
		clear();
	}

	void Uninitialize()
	{
		delwin(s_LoggingWindow);
		s_LoggingWindow = nullptr;

		delwin(s_InputWindow);
		s_InputWindow = nullptr;

		endwin();
	}

	bool ProcessKeyboardInput(char* p_KeyboardInputBuffer, unsigned int& p_KeyboardInputBufferSize,
									  unsigned int const p_KeyboardInputBufferCapacity)
	{

		// Pointer to the user input window.
		auto const l_Window = GetInputWindow();

		// This is the offset for where to place the cursor after a character is typed.
		//
		// As the user types, this increases.
		// When the buffer is flushed or the user presses "enter", this resets to zero.
		static int s_InputWindowCursorOffsetX{ 0 };

		// Try to get keyboard commands.

		int const l_InputKey{ mvwgetch(l_Window, INPUT_WINDOW_CURSOR_START_Y,
												 INPUT_WINDOW_CURSOR_START_X + s_InputWindowCursorOffsetX) };

		if ((l_InputKey == ERR) || (isascii(l_InputKey) == false))
		{
			return false;
		}

		// Increment the offset of the cursor when a valid character is received.
		s_InputWindowCursorOffsetX += 1;

		// Get the character. This conversion is safe because we checked if the character is ASCII.
		char const l_NextChar{ static_cast<char>(l_InputKey) };

		switch (l_NextChar)
		{
			case '\r':
				// Move the cursor back to the start of the input region.
				wmove(l_Window, INPUT_WINDOW_CURSOR_START_Y, INPUT_WINDOW_CURSOR_START_X);

				// Clear the line.
				wclrtoeol(l_Window);

				// Redraw the border.
				box(l_Window,
					 // Use default vertical character.
					 0,
					 // Use default horizontal character.
					 0);

				// Reset the offset.
				s_InputWindowCursorOffsetX = 0;
				break;

			case '\n':
				LoggerAddMessage("Unexpectedly got a newline character from user input.");
				return false;

			default:
				// Echo the character. This is why `noecho` was called; we are echoing manually.
				waddch(l_Window, l_NextChar);
				break;
		}

		// Accumulate characters until we get a terminating character or we run out of space.
		if ((l_NextChar != '\r') && (p_KeyboardInputBufferSize < (p_KeyboardInputBufferCapacity - 1)))
		{
			p_KeyboardInputBuffer[p_KeyboardInputBufferSize] = l_NextChar;
			p_KeyboardInputBufferSize++;
			return false;
		}

		// Terminate the command.
		p_KeyboardInputBuffer[p_KeyboardInputBufferSize] = '\0';

		// Echo the command back.
		LoggerAddMessage("Keyboard command input: \"%s\"", p_KeyboardInputBuffer);

		// Parse a command.

		// Tokenize the string.
		std::vector<CommandToken> l_CommandTokens;
		CommandTokenizeString(l_CommandTokens, p_KeyboardInputBuffer);

		// Parse command tokens.
		CommandParseTokens(l_CommandTokens);

		// Prepare for a new command.
		p_KeyboardInputBufferSize = 0;

		if (std::strcmp(p_KeyboardInputBuffer, "quit") == 0)
		{
			return true;
		}

		return false;
	}

} // namespace NCurses
