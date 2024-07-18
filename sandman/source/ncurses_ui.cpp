#include "ncurses_ui.h"

#include "command.h"
#include "logger.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <vector>

namespace NCurses
{
	using namespace std::string_view_literals;
	using namespace Common;

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

			// `leaveok` controls cursor placement after a call to the `wrefresh` subroutine.
			// Make sure to put the the physical cursor of the terminal
			// back at the location of the logical window cursor.
			leaveok(p_Window, TRUE);

			// Don't scroll when cursor is moved off the edge of the window.
			scrollok(p_Window, FALSE);
		}

		// input options: `man 'inopts(3NCURSES)'`
		{
			// Don't flush everything on interrupts.
			intrflush(p_Window, FALSE);

			// Enable functon keys (F1, F2, ...), arrow keys, backspace, etc.
			keypad(p_Window, TRUE);

			// Make `getch` non-blocking.
			nodelay(p_Window, TRUE);

			// Set to `FALSE` so that the timer can expire after a character
			// that could be the beginning of a function key is received. Otherwise,
			// the terminal may hang after pressing the escape key until another key is pressed.
			//
			// Source: "Keypad mode" section of `man 'getch(3NCURSES)'`.
			notimeout(p_Window, FALSE);
		}
	}

	namespace LoggingWindow
	{
		// This window is where messages from the logger are written to.
		static WINDOW* s_Window = nullptr;

		void WriteLine(char const* const string)
		{
			waddstr(s_Window, string);
			waddch(s_Window, '\n');
			wrefresh(s_Window);
		}

		WINDOW* Get()
		{
			return s_Window;
		}

		static void Initialize()
		{
			s_Window = newwin(
				// height (line count)
				LINES - InputWindow::ROW_COUNT,
				// width
				COLS,
				// upper corner y
				0,
				// left-hand corner x
				0);

			ConfigureWindowDefaults(s_Window);

			// Scroll when cursor is moved off the edge of the window.
			scrollok(s_Window, TRUE);
		}
	}

	namespace InputWindow
	{

		// This window is where user input is echoed to.
		static WINDOW* s_Window = nullptr;

		WINDOW* Get()
		{
			return s_Window;
		}

		[[gnu::always_inline]] inline static void HighlightChar(int const p_Position)
		{
			chtype const l_Character{ mvwinch(s_Window, CURSOR_START_Y, CURSOR_START_X + p_Position) };
			mvwaddch(s_Window, CURSOR_START_Y, CURSOR_START_X + p_Position, l_Character | A_STANDOUT);
		}

		static void Initialize() {
			s_Window = newwin(
				// height (line count)
				ROW_COUNT,
				// width
				COLS,
				// upper corner y
				LINES - ROW_COUNT,
				// left-hand corner x
				0);

			ConfigureWindowDefaults(s_Window);

			// Draw a border on the window.
			box(s_Window,
				 // use default vertical character
				 0,
				 // use default horizontal character
				 0);

			// Move the cursor to the corner.
			wmove(s_Window, CURSOR_START_Y, CURSOR_START_X);

			HighlightChar(0u);
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

			// Hide the physical (visible) cursor.
			curs_set(0);
		}

		// Configure standard screen window.
		ConfigureWindowDefaults(stdscr);

		// Configure logging window.
		LoggingWindow::Initialize();

		// Configure input window.
		InputWindow::Initialize();

		// Clear the screen.
		clear();
	}

	void Uninitialize()
	{
		delwin(LoggingWindow::s_Window);
		LoggingWindow::s_Window = nullptr;

		delwin(InputWindow::s_Window);
		InputWindow::s_Window = nullptr;

		endwin();
	}

	namespace InputWindow
	{
		// User keyboard input is stored here.
		static Buffer s_Buffer(
			Buffer::OnStringUpdateListener{[](std::size_t const p_Index, char const p_Character) -> void
			{
				mvwaddch(s_Window, CURSOR_START_Y, CURSOR_START_X + p_Index, p_Character);
			}},
			Buffer::OnClearListener{[]() -> void
			{
				// Move the cursor back to the start of the input region.
				wmove(s_Window, CURSOR_START_Y, CURSOR_START_X);

				// "Window clear to end of line": clear the line.
				wclrtoeol(s_Window);

				// Redraw the border.
				box(s_Window,
						// Use default vertical character.
						0,
						// Use default horizontal character.
						0);
			}}
		);

		Buffer const& GetBuffer()
		{
			return s_Buffer;
		}

		// The position for where to insert and remove in the input buffer.
		static std::uint_fast8_t s_Cursor{ 0u };

		static_assert(std::is_unsigned_v<decltype(s_Cursor)> and
						  std::numeric_limits<decltype(s_Cursor)>::max() >= s_Buffer.MAX_STRING_LENGTH,
						  "The input buffer cursor must be able to represent "
						  "all valid positions in the input buffer string for insertion, "
						  "including the exclusive end position where the current null character is.");

		static_assert(std::numeric_limits<decltype(s_Cursor)>::max() <= std::numeric_limits<int>::max(),
						  "NCurses uses `int` for window positions, so the cursor should never be "
						  "a value that can't be a valid window position.");

		static bool HandleSubmitString()
		{
			// Echo the command back.
			LoggerAddMessage("Keyboard command input: \"%s\"", s_Buffer.GetData().data());

			// Parse a command.
			{
				// Tokenize the string.
				std::vector<CommandToken> l_CommandTokens;
				CommandTokenizeString(l_CommandTokens, s_Buffer.GetData().data());

				// Parse command tokens.
				CommandParseTokens(l_CommandTokens);
			}

			static std::unordered_map<std::string_view, bool (*)()> const s_StringDispatch
			{
				{ "quit"sv, []() constexpr -> bool { return true; } }
			};

			// Handle dispatch.
			{
				auto const dispatchHandle(s_StringDispatch.find(s_Buffer.View()));

				s_Buffer.Clear(); HighlightChar(s_Cursor = 0u);

				return dispatchHandle != s_StringDispatch.end() ? dispatchHandle->second() : false;
			}
		}

	} // namespace InputWindow

	bool InputWindow::ProcessKey()
	{
		// Get one input key from the terminal, if any.
		int const l_InputKey{ wgetch(s_Window) };

		switch (l_InputKey)
		{
			// No input.
			case ERR:
				return false;

			// "Ctrl+D", EOT (End of Transmission), should gracefully quit.
			case Key::Ctrl<'D'>:
				return true;

			case KEY_LEFT:
				// If the curser has space to move left, move it left.
				if (s_Cursor > 0u) HighlightChar(--s_Cursor);
				return false;

			case KEY_RIGHT:
				// If the cursor has space to move right, including the position of the null character, move right.
				if (s_Cursor < s_Buffer.GetStringLength()) HighlightChar(++s_Cursor);
				return false;

			case KEY_BACKSPACE:
				// If successfully removed a character, move the cursor left.
				if (s_Buffer.Remove(s_Cursor - 1u)) HighlightChar(--s_Cursor);
				break;

			// User is submitting the line.
			case '\r':
				return HandleSubmitString();

			case '\n':
				LoggerAddMessage("Unexpectedly got a newline character from user input.");
				return false;

			// These "Ctrl" characters are usually handled by the terminal,
			// so they are usually not sent to the program.
			case Key::Ctrl<'C'>:
			case Key::Ctrl<'Z'>:
				// (Most likely unreachable.)
				LoggerAddMessage("Unexpectedly got a `Ctrl` character (%d) from user input", l_InputKey);
				return false;
		}

		bool const l_InputKeyIsPrintable{ IsASCII(l_InputKey) and std::isprint<char>(l_InputKey, std::locale::classic()) };

		// If successfully inserted into the buffer, move the cursor to the right.
		if (l_InputKeyIsPrintable and s_Buffer.Insert(s_Cursor, l_InputKey)) HighlightChar(++s_Cursor);

		return false;
	}

} // namespace NCurses
