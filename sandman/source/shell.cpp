#include "shell.h"

#include "command.h"
#include "logger.h"
#include "common/ascii.h"
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
	static std::mutex s_Mutex;

	Lock::Lock(): m_Lock(s_Mutex) {};

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

	struct Y : Common::Box<int> { using Box::Box; };
	struct X : Common::Box<int> { using Box::Box; };

	template <bool kFlag>
	[[gnu::always_inline]] inline static void SetCharAttr(WINDOW* const window, Y const y,
																			X const x, AttributeBundle::Value const attributes)
	{
		chtype const character{ mvwinch(window, y.m_Value, x.m_Value) };

		if constexpr (kFlag)
		{
			mvwaddch(window, y.m_Value, x.m_Value, character bitor attributes);
		}
		else
		{
			mvwaddch(window, y.m_Value, x.m_Value, character bitand compl(attributes));
		}
	}

	namespace LoggingWindow
	{
		// This window is where messages from the logger are written to.
		static WINDOW* s_Window = nullptr;

		static std::stack<AttributeBundle> s_AttributeStack;

		void Refresh() { wrefresh(s_Window); }

		void Write(chtype const character) { waddch(s_Window, character); }

		void Write(char const* const string) { waddstr(s_Window, string); }

		[[nodiscard]] bool PushAttributes(AttributeBundle const attributes)
		{
			s_AttributeStack.push(attributes);

			// Set attributes.
			wattrset(s_Window, attributes.m_Value);

			// Success.
			return true;
		}

		void PopAttributes()
		{
			if (not s_AttributeStack.empty()) {
				s_AttributeStack.pop();
			}

			if (not s_AttributeStack.empty())
			{
				wattrset(s_Window, s_AttributeStack.top().m_Value);
			}
			else
			{
				wattrset(s_Window, Normal.m_Value);
			}
		}

		void ClearAllAttributes()
		{
			s_AttributeStack = {}; // clear the stack
			wattrset(s_Window, Normal.m_Value);
		}

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

		template <bool kFlag>
		[[gnu::always_inline]] inline static void SetCharHighlight(X const positionX)
		{
			auto const offsetX{ positionX.m_Value };
			SetCharAttr<kFlag>(s_Window, Y(kCursorStartY), X(kCursorStartX + offsetX), A_STANDOUT);
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

			SetCharHighlight<true>(X(0u));
		}
	} // namespace InputWindow

	namespace
	{
		static std::sig_atomic_t volatile s_ShouldResize{ false };

		extern "C" void WindowChangeSignalHandler(int const signal)
		{
			#ifdef SIGWINCH
			if (signal == SIGWINCH) {
				s_ShouldResize = true;
			}
			#endif
		}
	}

	void CheckResize()
	{
		if (s_ShouldResize)
		{
			s_ShouldResize = false;
			LoggingWindow::PrintLine(Red("Window resize is unimplemented."));
		}
	}

	static void InitializeColorFunctionality()
	{
		using namespace ColorMatrix;

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

		if (COLORS < decltype(COLORS){kList.size()})
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
		CursesColorID colorPairID{ 1 };

		for (Index const backgroundColor : kList)
		{
			for (Index const foregroundColor : kList)
			{
				static constexpr decltype(colorPairID) kExclusiveUpperLimit
				{
					std::min(
						decltype(colorPairID){ 256 }, std::numeric_limits<decltype(colorPairID)>::max()
					)
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
				init_pair(colorPairID, GetColorID(foregroundColor), GetColorID(backgroundColor));

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
		delwin(LoggingWindow::s_Window);
		LoggingWindow::s_Window = nullptr;

		delwin(InputWindow::s_Window);
		InputWindow::s_Window = nullptr;

		endwin();
	}

	namespace InputWindow
	{
		using BufferT = EventfulBuffer<char, kMaxInputStringLength>;

		// User keyboard input is stored here.
		static BufferT s_Buffer(
			BufferT::OnStringUpdateListener{[](BufferT::Data::size_type const index,
														 BufferT::Data::value_type const character) -> void
			{
				mvwaddch(s_Window, kCursorStartY, kCursorStartX + index, character);
			}},
			BufferT::OnClearListener{[]() -> void
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
			BufferT::OnDecrementStringLengthListener{[](BufferT::Data::size_type const newStringLength
																	) -> void
			{
				mvwaddch(s_Window, kCursorStartY, kCursorStartX + newStringLength, ' ');
			}}
		);

		BufferT const& GetBuffer() { return s_Buffer; }

		using FastCursor = std::uint_fast8_t;

		// The position for where to insert and remove in the input buffer.
		static FastCursor s_Cursor{ 0u };

		static_assert(std::is_unsigned_v<FastCursor> and
						  std::numeric_limits<FastCursor>::max() >= s_Buffer.kMaxStringLength,
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
		[[gnu::always_inline]] inline static std::enable_if_t<
			std::is_same_v<CursorMovementT, Left> or std::is_same_v<CursorMovementT, Right>, void>
			BumpCursor()
		{
			static constexpr CursorMovementT kNext{};
			SetCharHighlight<false>(X(s_Cursor));
			SetCharHighlight<true>(X(s_Cursor = kNext(s_Cursor, static_cast<FastCursor>(1u))));
		}

		static bool HandleSubmitString()
		{

			std::string_view const bufferView(s_Buffer.View());

			if (not bufferView.empty())
			{
				// Echo the command back.
				LoggingWindow::PrintLine(Cyan(chtype{'\"'}), bufferView, Cyan(chtype{'\"'}));
			}

			// Parse a command.
			{
				// Tokenize the string.
				std::vector<CommandToken> commandTokens;
				CommandTokenizeString(commandTokens, s_Buffer.GetData().data());

				// Parse command tokens.
				CommandParseTokens(commandTokens);
			}

			static std::unordered_map<std::string_view, bool (*)()> const s_DispatchTable
			{
				{"quit"sv, []() constexpr -> bool {
					// Return boolean `true` denoting that the program should stop running.
					return true;
				}},
				{""sv, []() -> bool {
					redrawwin(LoggingWindow::s_Window);
					redrawwin(InputWindow::s_Window);

					wrefresh(LoggingWindow::s_Window);
					wrefresh(InputWindow::s_Window);

					// Reset cursor, just to be safe.
					s_Cursor = 0u;

					LoggingWindow::PrintLine(Magenta("Refreshed the screen."));

					return false;
				}}
			};

			// Handle dispatch.
			{
				auto const dispatchEntry(s_DispatchTable.find(bufferView));

				s_Buffer.Clear();
				SetCharHighlight<true>(X(s_Cursor = 0u));

				return dispatchEntry != s_DispatchTable.end() ? dispatchEntry->second() : false;
			}
		}

	} // namespace InputWindow

	bool InputWindow::ProcessSingleUserKey()
	{
		// Get one input key from the terminal, if any.
		switch (int const inputKey{ wgetch(s_Window) })
		{
			// No input.
			case ERR: return false;

			// "Ctrl+D", EOT (End of Transmission), should gracefully quit.
			case Key::Ctrl<'D'>: return true;

			case KEY_LEFT:
			{
				// If the curser has space to move left, move it left.
				if (s_Cursor > 0u)
				{
					BumpCursor<Left>();
				}
				return false;
			}

			case KEY_RIGHT:
			{
				// If the cursor has space to move right, including the position of the null character,
				// move right.
				if (s_Cursor < s_Buffer.GetLength())
				{
					BumpCursor<Right>();
				}
				return false;
			}

			case KEY_BACKSPACE:
			{
				// If successfully removed a character, move the cursor left.
				// (Unsigned `int` underflow is defined to wrap.)
				if (s_Buffer.Remove(s_Cursor - 1u))
				{
					BumpCursor<Left>();
				}
				return false;
			}

			// User is submitting the line.
			case '\r': return HandleSubmitString();

			case '\n':
			{
				LoggingWindow::PrintLine(Red("Unexpectedly got a newline character from user input."));
				return false;
			}

			// These "Ctrl" characters are usually handled by the terminal,
			// so they are usually not sent to the program.
			case Key::Ctrl<'C'>: [[fallthrough]];
			case Key::Ctrl<'Z'>:
			{
				// (Most likely unreachable.)
				LoggingWindow::PrintLine(Red(
					"Unexpectedly got a `Ctrl` character '", static_cast<chtype>(inputKey),
					"' from user input."
				));
				return false;
			}

			default:
			{
				bool const inputKeyIsPrintable{
					Common::IsASCII(inputKey) and std::isprint<char>(inputKey, std::locale::classic())
				};

				// If successfully inserted into the buffer, move the cursor to the right.
				if (inputKeyIsPrintable and s_Buffer.Insert(s_Cursor, inputKey))
				{
					BumpCursor<Right>();
				}
				else
				{
					LoggingWindow::PrintLine(
						Red("Can't write '", static_cast<chtype>(inputKey), "' into the input buffer.")
					);
				}

				return false;
			}
		}
	}

}
