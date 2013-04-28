#if defined (_WIN32)
	#include <conio.h>
	#include <Windows.h>
#elif defined (__linux__)
	#include <ncurses.h>
#endif // defined (_WIN32)
#include <ctype.h>
#include <stdio.h>

#include "control.h"
#include "logger.h"
#include "serial_connection.h"
#include "speech_recognizer.h"
#include "timer.h"

#if defined (_WIN32)
	#define USE_SERIAL_CONNECTION
#endif // defined (_WIN32)

#if !defined (USE_SERIAL_CONNECTION)
	#include "wiringPi.h"
#endif // !defined (USE_SERIAL_CONNECTION)

#if defined (_WIN32)
	#define DATADIR	"data"
#elif defined (__linux__)
	#define DATADIR	AM_DATADIR
#endif // defined (_WIN32)

// Types
//

// Types of command tokens.
enum CommandTokenTypes
{
	COMMAND_TOKEN_INVALID = -1,

	COMMAND_TOKEN_SAND,
	COMMAND_TOKEN_MAN,
	COMMAND_TOKEN_HEAD,
	COMMAND_TOKEN_KNEE,
	COMMAND_TOKEN_ELEVATION,
	COMMAND_TOKEN_UP,
	COMMAND_TOKEN_DOWN,
	COMMAND_TOKEN_STOP,

	NUM_COMMAND_TOKEN_TYPES,
};

// Types of controls.
enum ControlTypes
{
	CONTROL_HEAD_UP = 0,
	CONTROL_HEAD_DOWN,
	CONTROL_KNEE_UP,
	CONTROL_KNEE_DOWN,
	CONTROL_ELEVATION_UP,
	CONTROL_ELEVATION_DOWN,

	NUM_CONTROL_TYPES,
};

// Locals
//

// Names for each command token.
static char const* const s_CommandTokenNames[] = 
{
	"sand",			// COMMAND_TOKEN_SAND
	"man",			// COMMAND_TOKEN_MAN
	"head",			// COMMAND_TOKEN_HEAD
	"knee",			// COMMAND_TOKEN_KNEE
	"elevation",	// COMMAND_TOKEN_ELEVATION
	"up",			// COMMAND_TOKEN_UP
	"down",			// COMMAND_TOKEN_DOWN
	"stop",			// COMMAND_TOKEN_STOP
};

// The command string to send to the micro for each control.
static char const* const s_ControlCommandStrings[] =
{
	"hu ",	// CONTROL_HEAD_UP
	"hd ",	// CONTROL_HEAD_DOWN
	"ku ",	// CONTROL_KNEE_UP
	"kd ",	// CONTROL_KNEE_DOWN
	"eu ",	// CONTROL_ELEVATION_UP
	"ed ",	// CONTROL_ELEVATION_DOWN
};

// The controls.
static Control s_Controls[NUM_CONTROL_TYPES];

// Whether controls have been initialized.
static bool s_ControlsInitialized = false;

// Functions
//

template<class T>
T const& Min(T const& p_A, T const& p_B)
{
	return (p_A < p_B) ? p_A : p_B;
}

// Converts a string to lowercase.
//
// p_String:	The string to convert.
//
static void ConvertStringToLowercase(char* p_String)
{
	// Sanity check.
	if (p_String == NULL)
	{
		return;
	}

	char* l_CurrentLetter = p_String;
	while (*l_CurrentLetter != '\0')
	{
		// Convert this letter.
		*l_CurrentLetter = static_cast<char>(tolower(static_cast<unsigned int>(*l_CurrentLetter)));

		// Next letter.
		l_CurrentLetter++;
	}
}

// Uninitialize program components.
//
// p_Recognizer:	(Input/Output) The speech recognizer.
// p_Serial:		(Input/Output) The serial connection.
//
static void Uninitialize(SpeechRecognizer* p_Recognizer, SerialConnection* p_Serial)
{
	// Uninitialize the speech recognizer.
	if (p_Recognizer != NULL)
	{
		p_Recognizer->Uninitialize();
	}

	#if defined (USE_SERIAL_CONNECTION)

		// Uninitialize the serial connection.
		if (p_Serial != NULL)
		{
			p_Serial->Uninitialize();
		}

	#endif // defined (USE_SERIAL_CONNECTION)

	if (s_ControlsInitialized == true)
	{
		// Disable all controls.
		Control::Enable(false);
	
		// Uninitialize controls.
		for (unsigned int l_ControlIndex = 0; l_ControlIndex < NUM_CONTROL_TYPES; l_ControlIndex++)
		{
			s_Controls[l_ControlIndex].Uninitialize();
		}
	}
	
	// Uninitialize logging.
	LoggerUninitialize();

	#if defined (__linux__)

		// Uninitialize ncurses.
		endwin();

	#endif // defined (__linux__)
}

// Take a command string and turn it into a list of tokens.
//
// p_CommandTokenBufferSize:		(Output) How man tokens got put in the buffer.
// p_CommandTokenBuffer:			(Output) A buffer to hold the tokens.
// p_CommandTokenBufferCapacity:	The maximum the command token buffer can hold.
// p_CommandString:					The command string to tokenize.
//
static void TokenizeCommandString(unsigned int& p_CommandTokenBufferSize, CommandTokenTypes* p_CommandTokenBuffer,
	unsigned int const p_CommandTokenBufferCapacity, char const* p_CommandString)
{
	// Store token strings here.
	unsigned int const l_TokenStringBufferCapacity = 32;
	char l_TokenStringBuffer[l_TokenStringBufferCapacity];

	// Get the first token string start.
	char const* l_NextTokenStringStart = p_CommandString;

	while (l_NextTokenStringStart != NULL)
	{
		// Get the next token string end.
		char const* l_NextTokenStringEnd = strchr(l_NextTokenStringStart, ' ');

		// Get the token string length.
		unsigned int l_TokenStringLength = 0;
		if (l_NextTokenStringEnd != NULL)
		{
			l_TokenStringLength = l_NextTokenStringEnd - l_NextTokenStringStart;
		}
		else 
		{
			l_TokenStringLength = strlen(l_NextTokenStringStart);
		}

		// Copy the token string.
		unsigned int l_AmountToCopy = Min(l_TokenStringBufferCapacity - 1, l_TokenStringLength);
		strncpy(l_TokenStringBuffer, l_NextTokenStringStart, l_AmountToCopy);
		l_TokenStringBuffer[l_AmountToCopy] = '\0';

		// Make sure the token string is lowercase.
		ConvertStringToLowercase(l_TokenStringBuffer);

		// Match the token string to a token if possible.
		CommandTokenTypes l_Token = COMMAND_TOKEN_INVALID;

		for (unsigned int l_TokenType = 0; l_TokenType < NUM_COMMAND_TOKEN_TYPES; l_TokenType++)
		{
			// Compare the token string to its name.
			if (strcmp(l_TokenStringBuffer, s_CommandTokenNames[l_TokenType]) != 0)
			{
				continue;
			}

			// Found a match.
			l_Token = static_cast<CommandTokenTypes>(l_TokenType);
			break;
		}

		// Add the token to the buffer.
		if (p_CommandTokenBufferSize < p_CommandTokenBufferCapacity)
		{
			p_CommandTokenBuffer[p_CommandTokenBufferSize] = l_Token;
			p_CommandTokenBufferSize++;
		} 
		else
		{
			LoggerAddMessage("Voice command too long, tail will be ignored.");
		}


		// Get the next token string start (skip delimiter).
		if (l_NextTokenStringEnd != NULL)
		{
			l_NextTokenStringStart = l_NextTokenStringEnd + 1;
		}
		else
		{
			l_NextTokenStringStart = NULL;
		}
	}
}

// Parse the command tokens into commands.
//
// p_CommandTokenBufferSize:		(Input/Output) How man tokens got put in the buffer.
// p_CommandTokenBuffer:			(Input/Output) A buffer to hold the tokens.
//
void ParseCommandTokens(unsigned int& p_CommandTokenBufferSize, CommandTokenTypes* p_CommandTokenBuffer)
{
	// Parse command tokens.
	for (unsigned int l_TokenIndex = 0; l_TokenIndex < p_CommandTokenBufferSize; l_TokenIndex++)
	{
		// Look for the prefix.
		if (p_CommandTokenBuffer[l_TokenIndex] != COMMAND_TOKEN_SAND)
		{
			continue;
		}

		// Next token.
		l_TokenIndex++;
		if (l_TokenIndex >= p_CommandTokenBufferSize)
		{
			break;
		}

		// Look for the rest of the prefix.
		if (p_CommandTokenBuffer[l_TokenIndex] != COMMAND_TOKEN_MAN)
		{
			continue;
		}

		// Next token.
		l_TokenIndex++;
		if (l_TokenIndex >= p_CommandTokenBufferSize)
		{
			break;
		}

		// Parse commands.
		if (p_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_HEAD)
		{
			// Next token.
			l_TokenIndex++;
			if (l_TokenIndex >= p_CommandTokenBufferSize)
			{
				break;
			}

			if (p_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_UP)
			{
				s_Controls[CONTROL_HEAD_UP].SetMovingDesired(true);
			}
			else if (p_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_DOWN)
			{
				s_Controls[CONTROL_HEAD_DOWN].SetMovingDesired(true);
			}
		}
		else if (p_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_KNEE)
		{
			// Next token.
			l_TokenIndex++;
			if (l_TokenIndex >= p_CommandTokenBufferSize)
			{
				break;
			}

			if (p_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_UP)
			{
				s_Controls[CONTROL_KNEE_UP].SetMovingDesired(true);
			}
			else if (p_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_DOWN)
			{
				s_Controls[CONTROL_KNEE_DOWN].SetMovingDesired(true);
			}
		}
		else if (p_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_ELEVATION)
		{
			// Next token.
			l_TokenIndex++;
			if (l_TokenIndex >= p_CommandTokenBufferSize)
			{
				break;
			}

			if (p_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_UP)
			{
				s_Controls[CONTROL_ELEVATION_UP].SetMovingDesired(true);
			}
			else if (p_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_DOWN)
			{
				s_Controls[CONTROL_ELEVATION_DOWN].SetMovingDesired(true);
			}
		}
		else if (p_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_STOP)
		{
			// Stop controls.
			for (unsigned int l_ControlIndex = 0; l_ControlIndex < NUM_CONTROL_TYPES; l_ControlIndex++)
			{
				s_Controls[l_ControlIndex].SetMovingDesired(false);
			}
		}
	}

	// All tokens parsed.
	p_CommandTokenBufferSize = 0;
}

int main()
{
	#if defined (__linux__)

		// Initialize ncurses.
		initscr();

		// Don't wait for newlines, make getch non-blocking, and don't display input.
		cbreak();
		nodelay(stdscr, true);
		noecho();
		
		// Allow the window to scroll.
		scrollok(stdscr, true);
		idlok(stdscr, true);

		// Allow new-lines in the input.
		nonl();
		
	#endif // defined (__linux__)

	// Initialize logging.
	if (LoggerInitialize("sandman.log") == false)
	{
		Uninitialize(NULL, NULL);
		return 0;
	}

	// Initialize speech recognition.
	SpeechRecognizer l_Recognizer;
	if (l_Recognizer.Initialize("plughw:0,0", 16000, DATADIR "/hmm/en_US/hub4wsj_sc_8k", 
		DATADIR "/lm/en_US/sandman.lm", DATADIR "/dict/en_US/sandman.dic", "recognizer.log") == false)
	{
		Uninitialize(NULL, NULL);
		return 0;
	}

	#if defined (USE_SERIAL_CONNECTION)

		LoggerAddMessage("Opening serial port...");

		char const* const l_SerialPortName = "COM4";

		// Open the serial connection to the micro.
		SerialConnection l_Serial;
		if (l_Serial.Initialize(l_SerialPortName) == false)
		{
			Uninitialize(&l_Recognizer, NULL);

			LoggerAddMessage("\tfailed");
			return 0;
		}

		LoggerAddMessage("\tsucceeded");
		LoggerAddMessage("");

	#else 
	
		LoggerAddMessage("Initializing GPIO support...");
		
		if (wiringPiSetup() == -1)
		{
			LoggerAddMessage("\tfailed");

			Uninitialize(&l_Recognizer, NULL);
			return 0;
		}

		LoggerAddMessage("\tsucceeded");
		LoggerAddMessage("");
			
	#endif // defined (USE_SERIAL_CONNECTION)

	// Initialize controls.
	for (unsigned int l_ControlIndex = 0; l_ControlIndex < NUM_CONTROL_TYPES; l_ControlIndex++)
	{
		#if defined (USE_SERIAL_CONNECTION)

			s_Controls[l_ControlIndex].Initialize(&l_Serial, s_ControlCommandStrings[l_ControlIndex]);

		#else

			s_Controls[l_ControlIndex].Initialize(l_ControlIndex);

		#endif // defined (USE_SERIAL_CONNECTION)
	}

	// Enable all controls.
	Control::Enable(true);
	
	// Controls have been initialized.
	s_ControlsInitialized = true;
	
	// Store a keyboard input here.
	unsigned int const l_KeyboardInputBufferCapacity = 128;
	char l_KeyboardInputBuffer[l_KeyboardInputBufferCapacity];
	unsigned int l_KeyboardInputBufferSize = 0;

	// Store serial string data here.
	unsigned int const l_SerialStringBufferCapacity = 512;
	char l_SerialStringBuffer[l_SerialStringBufferCapacity];
	unsigned int l_SerialStringBufferSize = 0;

	bool l_Done = false;
	while (l_Done == false)
	{
		// We're gonna track the framerate.
		Time l_FrameStartTime;
		TimerGetCurrent(l_FrameStartTime);
		
		// Try to get keyboard commands.
		#if defined (_WIN32)

			if (_kbhit() != 0)
			{
				// Get the character.
				char l_NextChar = static_cast<char>(_getch());

		#elif defined (__linux__)

			int l_InputKey = getch();
			if ((l_InputKey != ERR) && (isascii(l_InputKey) == true))
			{
				// Get the character.
				char l_NextChar = static_cast<char>(l_InputKey);

		#endif // defined (_WIN32)

			// Accumulate characters until we get a terminating character or we run out of space.
			if ((l_NextChar != '\r') && (l_KeyboardInputBufferSize < (l_KeyboardInputBufferCapacity - 1)))
			{
				l_KeyboardInputBuffer[l_KeyboardInputBufferSize] = l_NextChar;
				l_KeyboardInputBufferSize++;
			}
			else
			{
				// Terminate the command.
				l_KeyboardInputBuffer[l_KeyboardInputBufferSize] = '\0';

				// Echo the command back.
				LoggerAddMessage("Keyboard command input: \"%s\"", l_KeyboardInputBuffer);

				// Parse a command.

				// Store command tokens here.
				unsigned int const l_CommandTokenBufferCapacity = 32;
				CommandTokenTypes l_CommandTokenBuffer[l_CommandTokenBufferCapacity];
				unsigned int l_CommandTokenBufferSize = 0;

				// Tokenize the speech.
				TokenizeCommandString(l_CommandTokenBufferSize, l_CommandTokenBuffer, l_CommandTokenBufferCapacity,
					l_KeyboardInputBuffer);

				// Parse command tokens.
				ParseCommandTokens(l_CommandTokenBufferSize, l_CommandTokenBuffer);

				if (strcmp(l_KeyboardInputBuffer, "quit") == 0)
				{
					l_Done = true;
				}

				// Prepare for a new command.
				l_KeyboardInputBufferSize = 0;
			}
		}

		// Process speech recognition.
		char const* l_RecognizedSpeech = NULL;
		if (l_Recognizer.Process(l_RecognizedSpeech) == false)
		{
			LoggerAddMessage("Error during speech recognition.");
			l_Done = true;
		}

		if (l_RecognizedSpeech != NULL)
		{
			// Parse a command.

			// Store command tokens here.
			unsigned int const l_CommandTokenBufferCapacity = 32;
			CommandTokenTypes l_CommandTokenBuffer[l_CommandTokenBufferCapacity];
			unsigned int l_CommandTokenBufferSize = 0;

			// Tokenize the speech.
			TokenizeCommandString(l_CommandTokenBufferSize, l_CommandTokenBuffer, l_CommandTokenBufferCapacity, l_RecognizedSpeech);

			// Parse command tokens.
			ParseCommandTokens(l_CommandTokenBufferSize, l_CommandTokenBuffer);
		}

		// Process controls.
		for (unsigned int l_ControlIndex = 0; l_ControlIndex < NUM_CONTROL_TYPES; l_ControlIndex++)
		{
			s_Controls[l_ControlIndex].Process();
		}

		#if defined (USE_SERIAL_CONNECTION)

			// Read serial port text, if there is any.  Read onto the end of the current string, nuking the current terminating
			// character.
			unsigned long int const l_NumBytesToRead = l_SerialStringBufferCapacity - l_SerialStringBufferSize;
			unsigned long int l_NumBytesRead = 0;

			if (l_Serial.ReadString(l_NumBytesRead, &l_SerialStringBuffer[l_SerialStringBufferSize], l_NumBytesToRead) == false)
			{
				l_Done = true;
			}

			// Did we read any?
			if (l_NumBytesRead != 0)
			{
				// Record the new size.
				l_SerialStringBufferSize += l_NumBytesRead;

				// If a new line is present, we can output.
				char* l_LineEnd = strchr(l_SerialStringBuffer, '\n');

				while (l_LineEnd != NULL)
				{
					// Terminate the string at line's end.
					*l_LineEnd = '\0';

					// Display it.
					LoggerAddMessage("%s: %s", l_SerialPortName, l_SerialStringBuffer);

					// Gobble a trailing carraige return.
					if (l_LineEnd[1] == '\r')
					{
						l_LineEnd++;
					}

					// Shift the buffer.
					// NOTE: Reduce size by the line length plus the terminating character that replaced the new line.  Then move
					// everything past that character plus a terminating character to the front.
					l_SerialStringBufferSize -= (l_LineEnd - l_SerialStringBuffer + 1);
					memmove(l_SerialStringBuffer, l_LineEnd + 1, l_SerialStringBufferSize + 1);

					// Look for another new line.
					l_LineEnd = strchr(l_SerialStringBuffer, '\n');
				}

				// Full?
				if ((l_SerialStringBufferSize + 1) == l_SerialStringBufferSize)
				{
					// Display it.
					LoggerAddMessage("%s: %s", l_SerialPortName, l_SerialStringBuffer);

					// Clear it.
					l_SerialStringBufferSize = 0;
				}
			}

		#endif // defined (USE_SERIAL_CONNECTION)

		// Get the duration of the frame in nanoseconds.
		Time l_FrameEndTime;
		TimerGetCurrent(l_FrameEndTime);
		
		float const l_FrameDurationMS = TimerGetElapsedMilliseconds(l_FrameStartTime, l_FrameEndTime);
		unsigned long const l_FrameDurationNS = static_cast<unsigned long>(l_FrameDurationMS * 1.0e6f);
		
		// If the frame is shorter than the duration corresponding to the desired framerate, sleep the
		// difference off.
		unsigned long const l_TargetFrameDurationNS = 1000000000 / 60;
		
		if (l_FrameDurationNS < l_TargetFrameDurationNS)
		{
			#if defined (__linux__)
			
				timespec l_SleepTime;
				l_SleepTime.tv_sec = 0;
				l_SleepTime.tv_nsec = l_TargetFrameDurationNS - l_FrameDurationNS;
				
				timespec l_RemainingTime;
				nanosleep(&l_SleepTime, &l_RemainingTime);
				
			#endif //defined (__linux__)
		}
	}

	// Cleanup.
	#if defined (USE_SERIAL_CONNECTION)

		Uninitialize(&l_Recognizer, &l_Serial);

	#else

		Uninitialize(&l_Recognizer, NULL);

	#endif // defined (USE_SERIAL_CONNECTION)
}
