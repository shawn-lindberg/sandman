#include "shell.h"

#include "command.h"
#include "logger.h"
#include "shell/input_window_eventful_buffer.h"

#include <algorithm>
#include <csignal>
#include <cstring>
#include <limits>
#include <locale>
#include <unordered_map>
#include <vector>
#include <stack>

namespace Shell
{
	static std::recursive_mutex s_mutex;

	Lock::Lock(): m_lock(s_mutex) {};

	// Configure a window with "sensible" defaults.
	static void ConfigureWindowDefaults(WINDOW* const window)
	{
		if (window == nullptr)
		{
			return;
		}

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
			// Make sure to put the physical cursor of the terminal
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


	// Set a character's attributes at a position on the window.
	//
	// Note that the Y parameter is before the X parameter.
	template <bool kFlag>
	inline static void SetCharAttr(WINDOW* const window,
																			int const y, int const x,
																			AttributeBundle::Value const attributes)
	{
		chtype const character{ mvwinch(window, y, x) };

		if constexpr (kFlag)
		{
			mvwaddch(window, y, x, character bitor attributes);
		}
		else
		{
			mvwaddch(window, y, x, character bitand compl(attributes));
		}
	}

	namespace LoggingWindow
	{
		// This window is where messages from the logger are written to.
		static WINDOW* s_window{nullptr};

		static std::stack<AttributeBundle> s_attributeStack;

		void Refresh() { wrefresh(s_window); }

		void Write(chtype const character) { waddch(s_window, character); }

		void Write(char const* const string) { waddstr(s_window, string); }

		[[nodiscard]] bool PushAttributes(AttributeBundle const attributes)
		{
			s_attributeStack.push(attributes);

			// Set attributes.
			wattrset(s_window, attributes.m_value);

			// Success.
			return true;
		}

		void PopAttributes()
		{
			if (not s_attributeStack.empty()) {
				s_attributeStack.pop();
			}

			if (not s_attributeStack.empty())
			{
				wattrset(s_window, s_attributeStack.top().m_value);
			}
			else
			{
				wattrset(s_window, Normal.m_value);
			}
		}

		void ClearAllAttributes()
		{
			s_attributeStack = {}; // clear the stack
			wattrset(s_window, Normal.m_value);
		}

		static void Initialize()
		{
			s_window = newwin(/* height (line   count) */ LINES - InputWindow::kRowCount,
									/* width  (column count) */ COLS                          ,
									/* upper  corner y       */ 0                             ,
									/* left   corner x       */ 0                             );

			ConfigureWindowDefaults(s_window);

			// Scroll when cursor is moved off the edge of the window.
			scrollok(s_window, TRUE);
		}
	} // namespace LoggingWindow

	namespace InputWindow
	{
		// This window is where user input is echoed to.
		static WINDOW* s_window{nullptr};

		template <bool kFlag>
		inline static void SetCharHighlight(int const positionX)
		{
			int const offsetX{ positionX };
			SetCharAttr<kFlag>(s_window, kCursorStartY, kCursorStartX + offsetX, A_STANDOUT);
		}

		static void Initialize()
		{
			s_window = newwin(/* height (line   count) */ kRowCount        ,
									/* width  (column count) */ COLS             ,
									/* upper  corner y       */ LINES - kRowCount,
									/* left   corner x       */ 0                );

			ConfigureWindowDefaults(s_window);

			// Draw a border on the window.
			box(s_window, /* Use default vertical   character. */ 0,
							  /* Use default horizontal character. */ 0);

			// Move the cursor to the corner.
			wmove(s_window, kCursorStartY, kCursorStartX);

			SetCharHighlight<true>(0u);
		}
	} // namespace InputWindow

	namespace
	{
		static std::sig_atomic_t volatile s_shouldResize{ false };

		extern "C" void WindowChangeSignalHandler(int const signal)
		{
			#ifdef SIGWINCH
			if (signal == SIGWINCH) {
				s_shouldResize = true;
			}
			#endif
		}
	}

	void CheckResize()
	{
		if (s_shouldResize)
		{
			s_shouldResize = false;
			LoggingWindow::PrintLine(Red("Window resize is unimplemented."));
		}
	}

	static void InitializeColorFunctionality()
	{
		// Initialize Curses color functionality.
		//
		// `main 'color(3NCURSES)'`, FUNCTIONS, start_color:
		// "It initializes two global variables, `COLORS` and `COLOR_PAIRS`."
		//
		// `main 'color(3NCURSES)'`, FUNCTIONS, start_color:
		// "It initializes the special color pair 0 to the default foreground and background colors."
		// No other colors pairs are initialized yet, but more will be initialized,
		// manually after this function call.
		//
		// `man 'color(3NCURSES)'`, FUNCTIONS, start_color:
		// "It is good practice to call this routine right after `initscr`."
		start_color();

		if (COLORS < decltype(COLORS){ColorMatrix::kColorCount})
		{
			// Doesn't support the standard colors; do something.
			return;
		}

		// Maximum color pairs that the terminal supports.
		auto const maxColorPairCount{ COLOR_PAIRS };

		// Starting from 1 instead of 0
		// because 0 denotes the default color which can't be changed portably
		// across Curses implementations.
		//
		// See further down in the nested for-loops in the comment
		// above the call to `init_pair` for some sources.
		ColorMatrix::CursesColorID colorPairID{ 1 };

		for (ColorMatrix::Index backgroundColor{0u}; backgroundColor < ColorMatrix::kColorCount; ++backgroundColor)
		{
			for (ColorMatrix::Index foregroundColor{0u}; foregroundColor < ColorMatrix::kColorCount; ++foregroundColor)
			{
				static constexpr ColorMatrix::CursesColorID kExclusiveUpperLimit{
					std::min(ColorMatrix::CursesColorID{256}, std::numeric_limits<ColorMatrix::CursesColorID>::max())
				};

				if (colorPairID >= maxColorPairCount or colorPairID >= kExclusiveUpperLimit)
				{
					return;
				}

				// Initialize a Curses color pair for later use.
				//
				// A Curses color pair is a combination of foreground color and a background color.
				//
				// The first argument should not be zero, because that denotes the
				// default color pair which can't be changed without extensions to Curses.
				//
				// Here are some sources that agree that the first argument should not be zero.
				//
				// According to
				// "https://www.ibm.com/docs/en/aix/7.3?topic=i-init-pair-subroutine",
				// in section "Parameters",
				// the first argument is a value that must be between `1` and `COLOR_PAIRS - 1`.
				// (It says `COLORS_PAIRS` instead of `COLOR_PAIRS`, but I believe
				// that is likely to be a mistake.)
				//
				// According to
				// "https://docs.oracle.com/cd/E36784_01/html/E36880/init-pair-3curses.html",
				// in section "Description", in subsection "Routine Descriptions",
				// in paragraph starting with "The init_pair() routine...",
				// the first argument is a value that must be btewen `1` and `COLOR_PAIRS - 1`.
				//
				// I cited those sources because the manual page `man 'init_pair(3NCURSES)'`,
				// in section "FUNCTIONS", in subsection "init_pair",
				// only says that for applications to be portable,
				// the first argument to `init_pair` must be a "legal color pair value".
				// However, in that section, it doesn't define what a "legal color pair value" is.
				// Although, it does state that, as an extension, NCurses does allows color pair 0
				// to be set if the routine `assume_default_colors` is called. This implies
				// that without the NCurses extension, it cannot be set.
				//
				init_pair(colorPairID,
							 ColorMatrix::getCursesColorIDOrDefault(foregroundColor, COLOR_WHITE),
							 ColorMatrix::getCursesColorIDOrDefault(backgroundColor, COLOR_BLACK));

				// Don't put the increment in call to `init_pair` in case it is a macro.
				// Sometimes Curses functions are macros.
				++colorPairID;
			}
		}
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
		delwin(LoggingWindow::s_window);
		LoggingWindow::s_window = nullptr;

		delwin(InputWindow::s_window);
		InputWindow::s_window = nullptr;

		endwin();
	}

	namespace InputWindow
	{
		using BufferT = EventfulBuffer<char, kMaxInputStringLength>;

		// User keyboard input is stored here.
		static BufferT s_buffer(
			BufferT::OnStringUpdateListener{[](
				BufferT::Data::size_type const index, BufferT::Data::value_type const character
			) -> void
			{
				mvwaddch(s_window, kCursorStartY, kCursorStartX + index, character);
			}},
			BufferT::OnClearListener{[]() -> void
			{
				// Move the cursor back to the start of the input region,
				// in fact, to the front of the line to be sure (x = 0).
				wmove(s_window, kCursorStartY, 0u);

				// "Window clear to end of line": clear the line.
				wclrtoeol(s_window);

				// Redraw the border.
				box(s_window, /* Use default vertical   character. */ 0,
								  /* Use default horizontal character. */ 0);
			}},
			BufferT::OnDecrementStringLengthListener{[](
				BufferT::Data::size_type const newStringLength
			) -> void
			{
				mvwaddch(s_window, kCursorStartY, kCursorStartX + newStringLength, ' ');
			}}
		);

		BufferT const& GetBuffer() { return s_buffer; }

		using FastCursor = std::uint_fast8_t;

		// The position for where to insert and remove in the input buffer.
		static FastCursor s_cursor{ 0u };

		static_assert(std::is_unsigned_v<FastCursor> and
						  std::numeric_limits<FastCursor>::max() >= s_buffer.kMaxStringLength,
						  "The input buffer cursor must be able to represent "
						  "all valid positions in the input buffer string for insertion, "
						  "including the exclusive end position where the current null character is.");

		static_assert(std::numeric_limits<FastCursor>::max() <= std::numeric_limits<int>::max(),
						  "NCurses uses `int` for window positions, so the cursor should never be "
						  "a value that can't be a valid window position.");

		inline namespace CursorMovements
		{
			using Right = std::plus<FastCursor>;
			using Left = std::minus<FastCursor>;
		}

		template <typename CursorMovementT>
		inline static std::enable_if_t<
			std::is_same_v<CursorMovementT, Left> or std::is_same_v<CursorMovementT, Right>, void>
			BumpCursor()
		{
			static constexpr CursorMovementT kNext{};
			SetCharHighlight<false>(s_cursor);
			SetCharHighlight<true>(s_cursor = kNext(s_cursor, static_cast<FastCursor>(1u)));
		}

		static std::unordered_map<std::string_view, Result (*)()> const s_dispatchTable
		{
			{"quit"sv, []() constexpr -> Result {
				// Return a value denoting that the program should stop running.
				return Result::kRequestToQuit;
			}},
			{""sv, []() -> Result {
				redrawwin(LoggingWindow::s_window);
				redrawwin(InputWindow::s_window);

				wrefresh(LoggingWindow::s_window);
				wrefresh(InputWindow::s_window);

				// Reset cursor, just to be safe.
				s_cursor = 0u;

				LoggingWindow::PrintLine(Magenta("Refreshed the screen."));

				return Result::kNone;
			}}
		};

		static Result HandleSubmitString()
		{

			std::string_view const bufferView(s_buffer.View());

			if (not bufferView.empty())
			{
				// Echo the command back.
				LoggingWindow::PrintLine(Cyan(chtype{'\"'}), bufferView, Cyan(chtype{'\"'}));
			}

			// Parse a command.
			{
				// Tokenize the string.
				std::vector<CommandToken> commandTokens;
				CommandTokenizeString(commandTokens, s_buffer.GetData().data());

				// Parse command tokens.
				if (CommandParseTokens(commandTokens) == CommandParseTokensReturnTypes::kInvalid)
				{
					LoggingWindow::PrintLine(Red("Invalid command: \""), s_buffer.GetData().data(), 
													 Red("\"."));
				}
			}

			// Handle dispatch.
			{
				auto const dispatchEntry(s_dispatchTable.find(bufferView));

				s_buffer.Clear();
				SetCharHighlight<true>(s_cursor = 0u);

				if (dispatchEntry == s_dispatchTable.end())
				{
					return Result::kNone;
				}

				auto const operation{ dispatchEntry->second };

				Result const result{ operation() };

				return result;
			}
		}

	} // namespace InputWindow

	auto InputWindow::ProcessSingleUserKey() -> Result
	{
		// Get one input key from the terminal, if any.
		switch (int const inputKey{ wgetch(s_window) })
		{
			// No input.
			case ERR: return Result::kNone;

			// "Ctrl+D", EOT (End of Transmission), should gracefully quit.
			case Key::Ctrl<'D'>: return Result::kRequestToQuit;

			case KEY_LEFT:
			{
				// If the curser has space to move left, move it left.
				if (s_cursor > 0u)
				{
					BumpCursor<Left>();
				}
				return Result::kNone;
			}

			case KEY_RIGHT:
			{
				// If the cursor has space to move right, including the position of the null character,
				// move right.
				if (s_cursor < s_buffer.GetLength())
				{
					BumpCursor<Right>();
				}
				return Result::kNone;
			}

			case KEY_BACKSPACE:
			{
				// If successfully removed a character, move the cursor left.
				// (Unsigned `int` underflow is defined to wrap.)
				if (s_buffer.Remove(s_cursor - 1u))
				{
					BumpCursor<Left>();
				}
				return Result::kNone;
			}

			// User is submitting the line.
			case '\r': return HandleSubmitString();

			case '\n':
			{
				LoggingWindow::PrintLine(Red("Unexpectedly got a newline character from user input."));
				return Result::kNone;
			}

			// These "Ctrl" characters are usually handled by the terminal,
			// so they are usually not sent to the program.
			case Key::Ctrl<'C'>: [[fallthrough]];
			case Key::Ctrl<'Z'>:
			{
				// (Most likely unreachable.)
				LoggingWindow::PrintLine(Red("Unexpectedly got a `Ctrl` character '",
													  static_cast<chtype>(inputKey), "' from user input."));
				return Result::kNone;
			}

			default:
			{
				/*
					Here, whether the input key is printable is determined by the `"C"` locale.
					The `"C"` locale is used as indicated by the argument `std::local::classic()`, in
					the call to `std::isprint` defined in `<locale>`, so it behaves like
					`std::isprint` defined in `<cctype>` as if the default `"C"` locale is being used.

					The `"C"` locale classifies these characters as printable:
					+ digits (`0123456789`)
					+ uppercase letters (`ABCDEFGHIJKLMNOPQRSTUVWXYZ`)
					+ lowercase letters (`abcdefghijklmnopqrstuvwxyz`)
					+ punctuation characters (``!"#$%&'()*+,-./:;<=>?@[\]^_`{|}~``)
					+ space (` `)

					Sources:
					+ [`std::isprint` defined in `<cctype>`](
						https://en.cppreference.com/w/cpp/string/byte/isprint)
					+ [`std::isprint` defined in `<locale>`](
						https://en.cppreference.com/w/cpp/locale/isprint)
				*/
				bool const inputKeyIsPrintable{ std::isprint<char>(inputKey, std::locale::classic()) };

				if (not inputKeyIsPrintable)
				{
					LoggingWindow::PrintLine(Red("Cannot write '", static_cast<chtype>(inputKey),
														  "' into the input buffer because '",
														  static_cast<chtype>(inputKey),
														  "' is not considered a printable character."));
					return Result::kNone;
				}

				// If successfully inserted into the buffer, move the cursor to the right.
				if (s_buffer.Insert(s_cursor, inputKey))
				{
					BumpCursor<Right>();
				}
				else
				{
					LoggingWindow::PrintLine(Red("Failed to write '", static_cast<chtype>(inputKey),
														  "' into the input buffer; "
														  "it is probable that the input buffer is full."));
				}

				return Result::kNone;
			}
		}
	}

}
