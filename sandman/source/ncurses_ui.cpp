#include "ncurses_ui.h"

#include "command.h"
#include "logger.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <vector>
#include <csignal>

namespace NCurses
{
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

	namespace CharAttribute
	{
		[[gnu::always_inline]] inline static void Add(WINDOW* const p_Window,
																	 int const p_Y, int const p_X,
																	 chtype const p_Attributes)
		{
			chtype const l_Character{ mvwinch(p_Window, p_Y, p_X) };
			mvwaddch(p_Window, p_Y, p_X, l_Character | p_Attributes);
		}

		[[gnu::always_inline]] inline static void Remove(WINDOW* const p_Window,
																		 int const p_Y, int const p_X,
																		 chtype const p_Attributes)
		{
			chtype const l_Character{ mvwinch(p_Window, p_Y, p_X) };
			mvwaddch(p_Window, p_Y, p_X, l_Character & ~p_Attributes);
		}

		using Operation = decltype(Add);

	}

	namespace LoggingWindow
	{
		// This window is where messages from the logger are written to.
		static WINDOW* s_Window = nullptr;

		void Put(Attr const p_CharacterAttribute)
		{
			if (p_CharacterAttribute.m_Flag)
			{
				wattron(s_Window, p_CharacterAttribute.m_Value);
			}
			else
			{
				wattroff(s_Window, p_CharacterAttribute.m_Value);
			}
		}

		void Put(WindowAction const p_Action)
		{
			switch (p_Action)
			{
				case WindowAction::REFRESH: wrefresh(s_Window); return;
			}
		}

		void Put(std::string_view const p_String)
		{
			for (char const l_Character : p_String) waddch(s_Window, l_Character);
		}

		void Put(chtype const p_Character)
		{
			waddch(s_Window, p_Character);
		}

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

		template <CharAttribute::Operation t_Operation>
		[[gnu::always_inline]] inline static void HighlightChar(int const p_Position)
		{
			t_Operation(s_Window, CURSOR_START_Y, CURSOR_START_X + p_Position, A_STANDOUT);
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

			HighlightChar<CharAttribute::Add>(0u);
		}
	}

	namespace
	{
		static std::sig_atomic_t volatile s_ShouldResize{false};

		extern "C" void WindowChangeSignalHandler(int const p_Signal)
		{
			static_cast<void>(p_Signal);
			s_ShouldResize = true;
		}
	}

	inline static void HandleResize() {
		endwin();
		refresh();
	}

	void Initialize()
	{
		// Initialize NCurses. This function exits the program on error!
		// This initializes the standard screen `stdscr` window.
		initscr();

		if (has_colors() == TRUE)
		{
			start_color();

			init_pair(Enum::IntCast(ColorIndex::BLACK  ), COLOR_BLACK  , COLOR_BLACK);
			init_pair(Enum::IntCast(ColorIndex::RED    ), COLOR_RED    , COLOR_BLACK);
			init_pair(Enum::IntCast(ColorIndex::GREEN  ), COLOR_GREEN  , COLOR_BLACK);
			init_pair(Enum::IntCast(ColorIndex::YELLOW ), COLOR_YELLOW , COLOR_BLACK);
			init_pair(Enum::IntCast(ColorIndex::BLUE   ), COLOR_BLUE   , COLOR_BLACK);
			init_pair(Enum::IntCast(ColorIndex::MAGENTA), COLOR_MAGENTA, COLOR_BLACK);
			init_pair(Enum::IntCast(ColorIndex::CYAN   ), COLOR_CYAN   , COLOR_BLACK);
			init_pair(Enum::IntCast(ColorIndex::WHITE  ), COLOR_WHITE  , COLOR_BLACK);
		}
		else
		{
			// The terminal does not support color; do something.
		}

		#ifdef SIGWINCH
		if (std::signal(SIGWINCH, WindowChangeSignalHandler) == SIG_ERR)
		{
			// Failed to register the signal handler; do something.
		}
		#endif

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
			Buffer::OnStringUpdateListener{[](Buffer::Data::size_type const p_Index, Buffer::Data::value_type const p_Character) -> void
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
			}},
			Buffer::OnDecrementStringLengthListener{[](Buffer::Data::size_type const p_NewStringLength) -> void
			{
				mvwaddch(s_Window, CURSOR_START_Y, CURSOR_START_X + p_NewStringLength, ' ');
			}}
		);

		Buffer const& GetBuffer()
		{
			return s_Buffer;
		}

		// The position for where to insert and remove in the input buffer.
		using FastCursor = std::uint_fast8_t;
		static FastCursor s_Cursor{ 0u };

		static_assert(std::is_unsigned_v<FastCursor> and
						  std::numeric_limits<FastCursor>::max() >= s_Buffer.MAX_STRING_LENGTH,
						  "The input buffer cursor must be able to represent "
						  "all valid positions in the input buffer string for insertion, "
						  "including the exclusive end position where the current null character is.");

		static_assert(std::numeric_limits<FastCursor>::max() <= std::numeric_limits<int>::max(),
						  "NCurses uses `int` for window positions, so the cursor should never be "
						  "a value that can't be a valid window position.");

		using Right = std::plus<FastCursor>;
		using Left = std::minus<FastCursor>;
		
		template <typename DirectionT>
		[[gnu::always_inline]] inline static
		std::enable_if_t<std::is_same_v<DirectionT, Left> or std::is_same_v<DirectionT, Right>, void>
		BumpCursor()
		{
			static constexpr DirectionT s_Next{};
			HighlightChar<CharAttribute::Remove>(s_Cursor);
			HighlightChar<CharAttribute::Add>(s_Cursor = s_Next(s_Cursor, static_cast<FastCursor>(1u)));
		}

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
				{ "quit"sv, []() constexpr -> bool { return true; } },
				{ ""sv, []() -> bool {
					redrawwin(LoggingWindow::s_Window);
					redrawwin(InputWindow::s_Window);

					wrefresh(LoggingWindow::s_Window);
					wrefresh(InputWindow::s_Window);

					// Reset cursor, just to be safe.
					s_Cursor = 0u;

					return false;
				} }
			};

			// Handle dispatch.
			{
				auto const dispatchHandle(s_StringDispatch.find(s_Buffer.View()));

				s_Buffer.Clear(); HighlightChar<CharAttribute::Add>(s_Cursor = 0u);

				return dispatchHandle != s_StringDispatch.end() ? dispatchHandle->second() : false;
			}
		}

	} // namespace InputWindow

	bool InputWindow::ProcessKey()
	{
		if (s_ShouldResize)
		{
			LoggerPrintArgs("[RESIZE]");
			s_ShouldResize = false;
		}

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
				if (s_Cursor > 0u) BumpCursor<Left>();
				return false;

			case KEY_RIGHT:
				// If the cursor has space to move right, including the position of the null character, move right.
				if (s_Cursor < s_Buffer.GetStringLength()) BumpCursor<Right>();
				return false;

			case KEY_BACKSPACE:
				// If successfully removed a character, move the cursor left.
				if (s_Buffer.Remove(s_Cursor - 1u)) BumpCursor<Left>();
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
		if (l_InputKeyIsPrintable and s_Buffer.Insert(s_Cursor, l_InputKey)) BumpCursor<Right>();

		return false;
	}

} // namespace NCurses
