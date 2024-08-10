#include "shell.h"

#include "command.h"
#include "logger.h"
#include "common/stack.h"
#include "common/ascii.h"
#include "common/string.h"

#include <algorithm>
#include <csignal>
#include <cstring>
#include <limits>
#include <locale>
#include <unordered_map>
#include <vector>

namespace Shell
{
	namespace
	{
		static std::mutex s_Mutex;
	}

	Lock::Lock(): m_Lock(s_Mutex) {};

	// Configure a window with "sensible" defaults.
	[[gnu::nonnull]] static void ConfigureWindowDefaults(WINDOW* const window)
	{
		// output options: `man 'outopts(3NCURSES)'`
		{
			// Don't make the next call to `wrefresh` clear and redraw the screen completely.
			clearok(window, FALSE);

			// Terminal's insert line & delete line features are okay to use.
			idlok(window, TRUE);

			// Terminal's insert character & delete character features are okay to use.
			idcok(window, TRUE);

			// Every change to the window will not cause a refresh to the physical screen.
			immedok(window, FALSE);

			// `leaveok` controls cursor placement after a call to the `wrefresh` subroutine.
			// Make sure to put the the physical cursor of the terminal
			// back at the location of the logical window cursor.
			leaveok(window, TRUE);

			// Don't scroll when cursor is moved off the edge of the window.
			scrollok(window, FALSE);
		}

		// input options: `man 'inopts(3NCURSES)'`
		{
			// Don't flush everything on interrupts.
			intrflush(window, FALSE);

			// Enable functon keys (F1, F2, ...), arrow keys, backspace, etc.
			keypad(window, TRUE);

			// Make `getch` non-blocking.
			nodelay(window, TRUE);

			// Set to `FALSE` so that the timer can expire after a character
			// that could be the beginning of a function key is received. Otherwise,
			// the terminal may hang after pressing the escape key until another key is pressed.
			//
			// Source: "Keypad mode" section of `man 'getch(3NCURSES)'`.
			notimeout(window, FALSE);
		}
	}

	template <bool kFlag>
	[[gnu::always_inline]] inline static void SetCharAttr(WINDOW* const window, int const y,
																			int const x, Attr::Value const attributes)
	{
		if constexpr (kFlag)
		{
			chtype const character{ mvwinch(window, y, x) };
			mvwaddch(window, y, x, character bitor attributes);
		}
		else
		{
			chtype const character{ mvwinch(window, y, x) };
			mvwaddch(window, y, x, character bitand compl(attributes));
		}
	}

	namespace LoggingWindow
	{
		// This window is where messages from the logger are written to.
		static WINDOW* s_Window = nullptr;

		static Common::Stack<Attr, kMaxAttributeObjectCount> s_AttributeStack;

		void Refresh() { wrefresh(s_Window); }

		void Write(chtype const character) { waddch(s_Window, character); }

		void Write(char const* const string) { waddstr(s_Window, string); }

		[[nodiscard]] bool PushAttributes(Attr const attributes)
		{
			if (s_AttributeStack.Push(attributes))
			{
				wattrset(s_Window, attributes.m_Value);
				return true;
			}

			return false;
		}

		void PopAttributes()
		{
			s_AttributeStack.Pop();

			if (Attr const* const attributes(s_AttributeStack.GetTop()); attributes != nullptr)
			{
				wattrset(s_Window, attributes->m_Value);
			}
			else
			{
				wattrset(s_Window, Normal.m_Value);
			}
		}

		void ClearAllAttributes()
		{
			s_AttributeStack.Clear();
			wattrset(s_Window, Normal.m_Value);
		}

		WINDOW* Get() { return s_Window; }

		static void Initialize()
		{
			s_Window = newwin(
				// height (line count)
				LINES - InputWindow::kRowCount,
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
	} // namespace LoggingWindow

	namespace InputWindow
	{
		// This window is where user input is echoed to.
		static WINDOW* s_Window = nullptr;

		WINDOW* Get() { return s_Window; }

		template <bool kFlag>
		[[gnu::always_inline]] inline static void SetCharHighlight(int const position)
		{
			SetCharAttr<kFlag>(s_Window, kCursorStartY, kCursorStartX + position, A_STANDOUT);
		}

		static void Initialize()
		{
			s_Window = newwin(
				// height (line count)
				kRowCount,
				// width
				COLS,
				// upper corner y
				LINES - kRowCount,
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
			wmove(s_Window, kCursorStartY, kCursorStartX);

			SetCharHighlight<true>(0u);
		}
	} // namespace InputWindow

	namespace
	{
		static std::sig_atomic_t volatile s_ShouldResize{ false };

		extern "C" void WindowChangeSignalHandler(int const signal)
		{
			switch (signal)
			{
				#ifdef SIGWINCH
				case SIGWINCH:
					s_ShouldResize = true;
					return;
				#endif
				default:
					return;
			}
		}
	}

	inline static void HandleResize()
	{
		LoggingWindow::Println<Red.BuildAttr().m_Value>("Window resize is unimplemented.");
	}

	static void InitializeColorFunctionality()
	{
		// Initialize Curses color functionality.
		//
		// `man 'color(3NCURSES)'`, FUNCTIONS, start_color:
		// 	"It is good practice to call this routine right after `initscr`."
		start_color();

		// Maximum color pairs that the terminal supports.
		auto const maxColorPairCount{ COLOR_PAIRS };

		CursesColorID colorPairID{ 0 };

		for (ColorIndex const backgroundColor : kColorList)
		{
			for (ColorIndex const foregroundColor : kColorList)
			{
				static constexpr decltype(colorPairID) kExclusiveUpperLimit{
					std::min(
						decltype(colorPairID){ 256 },
						std::numeric_limits<decltype(colorPairID)>::max()
					)
				};

				if (colorPairID >= maxColorPairCount or colorPairID >= kExclusiveUpperLimit)
				{
					goto EndInitializeColorPairs;
				}

				// Initialize a Curses color pair for later use.
				// A Curses color pair is a combination of foreground color and a background color.
				init_pair(colorPairID++, GetColorID(foregroundColor), GetColorID(backgroundColor));
			}
		}

		EndInitializeColorPairs: {}
	}

	void Initialize()
	{
		// Initialize NCurses. This function exits the program on error!
		// This initializes the standard screen `stdscr` window.
		initscr();

		if (has_colors() == TRUE)
		{
			InitializeColorFunctionality();
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
		using Buffer = Common::String<char, kMaxInputStringLength>;

		// User keyboard input is stored here.
		static Buffer s_Buffer(
			Buffer::OnStringUpdateListener{[](Buffer::Data::size_type const index, Buffer::Data::value_type const character) -> void
			{
				mvwaddch(s_Window, kCursorStartY, kCursorStartX + index, character);
			}},
			Buffer::OnClearListener{[]() -> void
			{
				// Move the cursor back to the start of the input region,
				// in fact, to the front of the line to be sure (x = 0).
				wmove(s_Window, kCursorStartY, 0u);

				// "Window clear to end of line": clear the line.
				wclrtoeol(s_Window);

				// Redraw the border.
				box(s_Window,
					 // Use default vertical character.
					 0,
					 // Use default horizontal character.
					 0);
			}},
			Buffer::OnDecrementStringLengthListener{[](Buffer::Data::size_type const newStringLength) -> void
			{
				mvwaddch(s_Window, kCursorStartY, kCursorStartX + newStringLength, ' ');
			}}
		);

		Buffer const& GetBuffer() { return s_Buffer; }

		// The position for where to insert and remove in the input buffer.
		using FastCursor = std::uint_fast8_t;
		static FastCursor s_Cursor{ 0u };

		static_assert(std::is_unsigned_v<FastCursor> and
						  std::numeric_limits<FastCursor>::max() >= s_Buffer.kMaxStringLength,
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
			static constexpr DirectionT kNext{};
			SetCharHighlight<false>(s_Cursor);
			SetCharHighlight<true>(s_Cursor = kNext(s_Cursor, static_cast<FastCursor>(1u)));
		}

		static bool HandleSubmitString()
		{
			// Echo the command back.
			LoggingWindow::Println(chtype{'\"'}, s_Buffer.View(), chtype{'\"'});

			// Parse a command.
			{
				// Tokenize the string.
				std::vector<CommandToken> commandTokens;
				CommandTokenizeString(commandTokens, s_Buffer.GetData().data());

				// Parse command tokens.
				CommandParseTokens(commandTokens);
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

				s_Buffer.Clear(); SetCharHighlight<true>(s_Cursor = 0u);

				return dispatchHandle != s_StringDispatch.end() ? dispatchHandle->second() : false;
			}
		}

	} // namespace InputWindow

	bool InputWindow::ProcessSingleUserKey()
	{
		if (s_ShouldResize)
		{
			s_ShouldResize = false;
			HandleResize();
		}

		// Get one input key from the terminal, if any.
		switch (int const inputKey{ wgetch(s_Window) }; inputKey)
		{
			// No input.
			case ERR:
				return false;

			// "Ctrl+D", EOT (End of Transmission), should gracefully quit.
			case Key::Ctrl<'D'>:
				return true;

			case KEY_LEFT:
				// If the curser has space to move left, move it left.
				if (s_Cursor > 0u)
				{
					BumpCursor<Left>();
				}
				return false;

			case KEY_RIGHT:
				// If the cursor has space to move right, including the position of the null character,
				// move right.
				if (s_Cursor < s_Buffer.GetLength())
				{
					BumpCursor<Right>();
				}
				return false;

			case KEY_BACKSPACE:
				// If successfully removed a character, move the cursor left.
				// (Unsigned `int` underflow is defined to wrap.)
				if (s_Buffer.Remove(s_Cursor - 1u))
				{
					BumpCursor<Left>();
				}
				return false;

			// User is submitting the line.
			case '\r':
				return HandleSubmitString();

			case '\n':
				LoggingWindow::Println(Red("Unexpectedly got a newline character from user input."));
				return false;

			// These "Ctrl" characters are usually handled by the terminal,
			// so they are usually not sent to the program.
			case Key::Ctrl<'C'>:
			case Key::Ctrl<'Z'>:
				// (Most likely unreachable.)
				LoggingWindow::Println(Red("Unexpectedly got a `Ctrl` character (",
													static_cast<chtype>(inputKey), ") from user input."));
				return false;

			default:
				bool const inputKeyIsPrintable{ Common::ASCII::Match(inputKey) and
														  std::isprint<char>(inputKey, std::locale::classic()) };

				// If successfully inserted into the buffer, move the cursor to the right.
				if (inputKeyIsPrintable and s_Buffer.Insert(s_Cursor, inputKey))
				{
					BumpCursor<Right>();
				}

				return false;
		}
	}

}
