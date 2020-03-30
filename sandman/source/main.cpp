#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>

#include <ncurses.h>
#include "wiringPi.h"

#include "config.h"
#include "control.h"
#include "input.h"
#include "logger.h"
#include "schedule.h"
#include "sound.h"

#if defined (USE_INTERNAL_SPEECH_RECOGNITION)
	#include "speech_recognizer.h"
#endif // defined (USE_INTERNAL_SPEECH_RECOGNITION)
	
#include "timer.h"

#define DATADIR	AM_DATADIR
#define CONFIGDIR	AM_CONFIGDIR
#define TEMPDIR	AM_TEMPDIR

// Types
//

// Describes a command token.
struct CommandToken
{
	// Types of command tokens.
	enum Types
	{
		TYPE_INVALID = -1,

		TYPE_BACK,
		TYPE_LEGS,
		TYPE_ELEVATION,
		TYPE_RAISE,
		TYPE_LOWER,
		TYPE_STOP,
		TYPE_VOLUME,
		// TYPE_MUTE,
		// TYPE_UNMUTE,
		TYPE_SCHEDULE,
		TYPE_START,
		TYPE_STATUS,
		
		TYPE_NOT_PARAMETER_COUNT, 
		
		// The following command tokens are parameters.
		TYPE_INTEGER = TYPE_NOT_PARAMETER_COUNT, 
		
		TYPE_COUNT,
	};

	// The type of the token.
	Types	m_Type = TYPE_INVALID;
	
	// The value of the integer parameter, if relevant.
	int m_Parameter = 0;
};

// Locals
//

// Names for each command token.
static char const* const s_CommandTokenNames[] = 
{
	"back",			// TYPE_BACK
	"legs",			// TYPE_LEGS
	"elevation",	// TYPE_ELEVATION
	"raise",			// TYPE_RAISE
	"lower",			// TYPE_LOWER
	"stop",			// TYPE_STOP
	"volume",		// TYPE_VOLUME
	// "mute",			// TYPE_MUTE
	// "unmute",		// TYPE_UNMUTE
	"schedule",		// TYPE_SCHEDULE
	"start",			// TYPE_START
	"status",		// TYPE_STATUS
	
	"integer", 		// TYPE_INTEGER
};

// Whether controls have been initialized.
static bool s_ControlsInitialized = false;

// The input device.
static Input s_Input;

#if defined (USE_INTERNAL_SPEECH_RECOGNITION)

	// The speech recognizer.
	static SpeechRecognizer s_Recognizer;
	
#endif // defined (USE_INTERNAL_SPEECH_RECOGNITION)

// Whether to start as a daemon or terminal program.
static bool s_DaemonMode = false;

// Used to listen for connections.
static int s_ListeningSocket = -1;

// Keep handles to the controls.
static ControlHandle s_BackControlHandle;
static ControlHandle s_LegsControlHandle;
static ControlHandle s_ElevationControlHandle;

// Functions
//

// Converts a string to lowercase.
//
// p_String:	The string to convert.
//
static void ConvertStringToLowercase(char* p_String)
{
	// Sanity check.
	if (p_String == nullptr)
	{
		return;
	}

	auto* l_CurrentLetter = p_String;
	while (*l_CurrentLetter != '\0')
	{
		// Convert this letter.
		*l_CurrentLetter = static_cast<char>(tolower(static_cast<unsigned int>(*l_CurrentLetter)));

		// Next letter.
		l_CurrentLetter++;
	}
}

// Converts a string to an (unsigned) integer.
//
// p_Integer:	(Output) The converted integer value.
// p_String:	The string to convert.
// 
// Returns:	True if a valid integer was created, false otherwise.
//
static bool ConvertStringToInteger(int& p_Integer, char* p_String)
{
	// Sanity check.
	if (p_String == nullptr)
	{
		return false;
	}
	
	if (p_String[0] == '\0')
	{
		return false;
	}
	
	// Build up the integer in a larger container, so overflow can be easily detected.
	int64_t l_Integer = 0;
	
	auto* l_CurrentCharacter = p_String;
	
	while (*l_CurrentCharacter != '\0')
	{
		// Convert this letter.
		switch (*l_CurrentCharacter)
		{
			case '0':	// Fall through...
			case '1':	// Fall through...
			case '2':	// Fall through...
			case '3':	// Fall through...
			case '4':	// Fall through...
			case '5':	// Fall through...
			case '6':	// Fall through...
			case '7':	// Fall through...
			case '8':	// Fall through...
			case '9':
			{
				// Consider leading zeros to be badly formed.
				if ((l_Integer == 0) && (l_CurrentCharacter != p_String)) {
					return false;
				}
			
				// This calculation depends on the ASCII character values starting at zero and 
				// going up to nine contiguously.
				auto const l_Digit = (*l_CurrentCharacter - '0');
				l_Integer = (l_Integer * 10) + l_Digit;
			}
			break;
			
			default:
			{
			}
			return false;
		}

		// If the number has gotten too large, bail.
		if (l_Integer > INT_MAX) {
			return false;
		}
		
		// Next letter.
		l_CurrentCharacter++;
	}
	
	// Convert the accumulated value.
	p_Integer = static_cast<int>(l_Integer);
	return true;
}

// Initialize program components.
//
// returns:		True for success, false otherwise.
//
static bool Initialize()
{				
	if (s_DaemonMode == true)
	{
		printf("Initializing as a daemon.\n");
		
		// Fork a child off of the parent process.
		auto const l_ProcessID = fork();
		
		// Legitimate failure.
		if (l_ProcessID < 0)
		{
			return false;
		}
		
		// The parent gets the ID of the child and exits.
		if (l_ProcessID > 0)
		{
			return false;
		}
		
		// The child gets 0 and continues.
		
		// Allow file access.
		umask(0);
		
		// Initialize logging.
		if (LoggerInitialize(TEMPDIR "sandman.log") == false)
		{
			return false;
		}

		// Need a new session ID.
		auto const l_SessionID = setsid();
		
		if (l_SessionID < 0)
		{
			LoggerAddMessage("Failed to get new session ID for daemon.");
			return false;
		}
	
		// Change the current working directory.
		if (chdir(TEMPDIR) < 0)
		{
			LoggerAddMessage("Failed to change working directory to \"%s\" ID for daemon.", TEMPDIR);
			return false;
		}
		
		// Close stdin, stdou, stderr.
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
		
		// Redirect stdin, stdout, stderr to /dev/null (this relies on them mapping to
		// the lowest numbered file descriptors).
		open("dev/null", O_RDWR);
		open("dev/null", O_RDWR);
		open("dev/null", O_RDWR);
		
		// Now that we are a daemon, set up Unix domain sockets for communication.
		
		// Create a listening socket.
		s_ListeningSocket = socket(AF_UNIX, SOCK_STREAM, 0);
		
		if (s_ListeningSocket < 0)
		{
			LoggerAddMessage("Failed to create listening socket.");
			return false;
		}
		
		// Set to non-blocking.
		if (fcntl(s_ListeningSocket, F_SETFL, O_NONBLOCK) < 0)
		{
			LoggerAddMessage("Failed to make listening socket non-blocking.");
			return false;
		}
		
		sockaddr_un l_ListeningAddress;
		{
			l_ListeningAddress.sun_family = AF_UNIX;
			strncpy(l_ListeningAddress.sun_path, TEMPDIR "sandman.sock", 
				sizeof(l_ListeningAddress.sun_path) - 1);
		}
		
		// Unlink the file if needed.
		unlink(l_ListeningAddress.sun_path);
		
		// Bind the socket to the file.
		if (bind(s_ListeningSocket, reinterpret_cast<sockaddr*>(&l_ListeningAddress), sizeof(sockaddr_un)) < 0)
		{
			LoggerAddMessage("Failed to bind listening socket.");
			return false;
		}
		
		// Mark the socket for listening.
		if (listen(s_ListeningSocket, 5) < 0)
		{
			LoggerAddMessage("Failed to mark listening socket to listen.");
			return false;
		}
		
		// Sockets setup!	
	}
	else
	{
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
			
		// Initialize logging.
		if (LoggerInitialize(TEMPDIR "sandman.log") == false)
		{
			return false;
		}
		
		LoggerEchoToScreen(true);
	}
				
	// Read the config.
	Config l_Config;
	if (l_Config.ReadFromFile(CONFIGDIR "sandman.conf") == false)
	{
		return false;
	}	
		
	#if defined (USE_INTERNAL_SPEECH_RECOGNITION)

		// Initialize speech recognition.
		if (s_Recognizer.Initialize(l_Config.GetSpeechInputDeviceName(), l_Config.GetInputSampleRate(), 
			DATADIR "hmm/en_US/hub4wsj_sc_8k", DATADIR "lm/en_US/sandman.lm", 
			DATADIR "dict/en_US/sandman.dic", TEMPDIR "recognizer.log", 
			l_Config.GetPostSpeechDelaySec()) == false)
		{
			return false;
		}
		
	#endif // defined (USE_INTERNAL_SPEECH_RECOGNITION)	

	LoggerAddMessage("Initializing GPIO support...");
	
	if (wiringPiSetup() == -1)
	{
		LoggerAddMessage("\tfailed");
		return false;
	}

	LoggerAddMessage("\tsucceeded");
	LoggerAddMessage("");
			
	// Initialize sound.
	if (SoundInitialize() == false)
	{
		return false;
	}

	// Initialize controls.
	ControlsInitialize(l_Config.GetControlConfigs());

	// Set control durations.
	Control::SetDurations(l_Config.GetControlMaxMovingDurationMS(), 
		l_Config.GetControlCoolDownDurationMS());
	
	// Enable all controls.
	Control::Enable(true);
	
	// Controls have been initialized.
	s_ControlsInitialized = true;
	
	// Initialize the input device.
	s_Input.Initialize(l_Config.GetInputDeviceName(), l_Config.GetInputBindings());
	
	// Initialize the schedule.
	ScheduleInitialize();
	
	// Play initialization speech.
	SoundAddToQueue(DATADIR "audio/initialized.wav");

	return true;
}

// Uninitialize program components.
//
static void Uninitialize()
{
	// Close the listening socket, if there was one.
	if (s_ListeningSocket >= 0)
	{
		close(s_ListeningSocket);
	}
	
	// Uninitialize the schedule.
	ScheduleUninitialize();
	
	#if defined (USE_INTERNAL_SPEECH_RECOGNITION)
	
		// Uninitialize the speech recognizer.
		s_Recognizer.Uninitialize();

	#endif // defined (USE_INTERNAL_SPEECH_RECOGNITION)

	// Uninitialize sound.
	SoundUninitialize();
	
	if (s_ControlsInitialized == true)
	{
		// Disable all controls.
		Control::Enable(false);
	
		// Uninitialize controls.
		ControlsUninitialize();
	}
		
	// Uninitialize the input.
	s_Input.Uninitialize();
		
	// Uninitialize logging.
	LoggerUninitialize();

	if (s_DaemonMode == false)
	{
		// Uninitialize ncurses.
		endwin();
	}
}

// Take a command string and turn it into a list of tokens.
//
// p_CommandTokens:	(Output) The resulting command tokens, in order.
// p_CommandString:	The command string to tokenize.
//
static void TokenizeCommandString(std::vector<CommandToken>& p_CommandTokens, 
	char const* p_CommandString)
{
	// Store token strings here.
	static constexpr unsigned int l_TokenStringBufferCapacity = 32;
	char l_TokenStringBuffer[l_TokenStringBufferCapacity];

	// Get the first token string start.
	auto const* l_NextTokenStringStart = p_CommandString;

	while (l_NextTokenStringStart != nullptr)
	{
		// Get the next token string end.
		auto const* l_NextTokenStringEnd = strchr(l_NextTokenStringStart, ' ');

		// Get the token string length.
		unsigned int l_TokenStringLength = 0;
		if (l_NextTokenStringEnd != nullptr)
		{
			l_TokenStringLength = l_NextTokenStringEnd - l_NextTokenStringStart;
		}
		else 
		{
			l_TokenStringLength = l_TokenStringBufferCapacity - 1;
		}

		// Copy the token string.
		strncpy(l_TokenStringBuffer, l_NextTokenStringStart, l_TokenStringLength);
		l_TokenStringBuffer[l_TokenStringLength] = '\0';

		// Make sure the token string is lowercase.
		ConvertStringToLowercase(l_TokenStringBuffer);

		// Match the token string to a token (with no parameter) if possible.
		CommandToken l_Token;

		for (unsigned int l_TokenType = 0; l_TokenType < CommandToken::TYPE_NOT_PARAMETER_COUNT; 
			l_TokenType++)
		{
			// Compare the token string to its name.
			if (strcmp(l_TokenStringBuffer, s_CommandTokenNames[l_TokenType]) != 0)
			{
				continue;
			}

			// Found a match.
			l_Token.m_Type = static_cast<CommandToken::Types>(l_TokenType);
			break;
		}

		// If we couldn't turn it into a plain old token, see if it is a parameter token.
		if (l_Token.m_Type == CommandToken::TYPE_INVALID)
		{
			if (ConvertStringToInteger(l_Token.m_Parameter, l_TokenStringBuffer) == true)
			{
				l_Token.m_Type = CommandToken::TYPE_INTEGER;
			}
		}
		
		// Add the token to the list.
		p_CommandTokens.push_back(l_Token);

		// Get the next token string start (skip delimiter).
		if (l_NextTokenStringEnd != nullptr)
		{
			l_NextTokenStringStart = l_NextTokenStringEnd + 1;
		}
		else
		{
			l_NextTokenStringStart = nullptr;
		}
	}
}

// Parse the command tokens into commands.
//
// p_CommandTokens:	All of the potential tokens for the command.
//
static void ParseCommandTokens(std::vector<CommandToken> const& p_CommandTokens)
{
	// Parse command tokens.
	auto const l_TokenCount = static_cast<unsigned int>(p_CommandTokens.size());
	for (unsigned int l_TokenIndex = 0; l_TokenIndex < l_TokenCount; l_TokenIndex++)
	{
		// Parse commands.
		auto l_Token = p_CommandTokens[l_TokenIndex];
		switch (l_Token.m_Type)
		{
			case CommandToken::TYPE_BACK:			// Fall through...
			case CommandToken::TYPE_LEGS:			// Fall through...
			case CommandToken::TYPE_ELEVATION:	
			{
				// Try to access the control corresponding to the command token.
				Control* l_Control = nullptr;
				
				switch (l_Token.m_Type)
				{
					case CommandToken::TYPE_BACK:
					{
						// Try to find the control.
						if (s_BackControlHandle.IsValid() == false)
						{
							s_BackControlHandle = Control::GetHandle("back");
						}
						
						l_Control = Control::GetFromHandle(s_BackControlHandle);
					}
					break;
					
					case CommandToken::TYPE_LEGS:
					{
						// Try to find the control.
						if (s_LegsControlHandle.IsValid() == false)
						{
							s_LegsControlHandle = Control::GetHandle("legs");
						}
						
						l_Control = Control::GetFromHandle(s_LegsControlHandle);
					}
					break;
					
					case CommandToken::TYPE_ELEVATION:
					{
						// Try to find the control.
						if (s_ElevationControlHandle.IsValid() == false)
						{
							s_ElevationControlHandle = Control::GetHandle("elev");
						}
			
						l_Control = Control::GetFromHandle(s_ElevationControlHandle);
					}
					break;
					
					default:
					{
						LoggerAddMessage("Unrecognized token \"%s\" trying to process a control movement "
							"command.", s_CommandTokenNames[l_Token.m_Type]);
					}
					break;
				}
			
				if (l_Control == nullptr)
				{
					break;
				}			
			
				// Next token.
				l_TokenIndex++;
				if (l_TokenIndex >= l_TokenCount)
				{
					break;
				}
				
				l_Token = p_CommandTokens[l_TokenIndex];
				
				// Try to get the action which should be performed on the control.
				auto l_Action = Control::ACTION_STOPPED;
				
				if (l_Token.m_Type == CommandToken::TYPE_RAISE)
				{
					l_Action = Control::ACTION_MOVING_UP;
				}
				else if (l_Token.m_Type == CommandToken::TYPE_LOWER)
				{
					l_Action = Control::ACTION_MOVING_DOWN;
				}
				
				if (l_Action == Control::ACTION_STOPPED)
				{
					break;
				}
				
				// Determine the duration percent.
				unsigned int l_DurationPercent = 100;
				
				// Peak at the next token.
				auto const l_NextTokenIndex = l_TokenIndex + 1;
				if (l_NextTokenIndex < l_TokenCount)
				{
					auto const l_NextToken = p_CommandTokens[l_NextTokenIndex];
					
					if (l_NextToken.m_Type == CommandToken::TYPE_INTEGER)
					{
						l_DurationPercent = l_NextToken.m_Parameter;
						
						// Actually consume this token.
						l_TokenIndex++;
					}
				}
				
				
				l_Control->SetDesiredAction(l_Action, Control::MODE_TIMED, l_DurationPercent);
			}
			break;
			
			case CommandToken::TYPE_STOP:
			{
				// Stop controls.
				ControlsStopAll();			
			}
			break;
			
			case CommandToken::TYPE_SCHEDULE:
			{
				// Next token.
				l_TokenIndex++;
				if (l_TokenIndex >= l_TokenCount)
				{
					break;
				}
				
				l_Token = p_CommandTokens[l_TokenIndex];
			
				if (l_Token.m_Type == CommandToken::TYPE_START)
				{
					ScheduleStart();
				}
				else if (l_Token.m_Type == CommandToken::TYPE_STOP)
				{
					ScheduleStop();
				}			
			}
			break;
			
			case CommandToken::TYPE_VOLUME:
			{
				// Next token.
				l_TokenIndex++;
				if (l_TokenIndex >= l_TokenCount)
				{
					break;
				}

				l_Token = p_CommandTokens[l_TokenIndex];
				
				if (l_Token.m_Type == CommandToken::TYPE_RAISE)
				{
					SoundIncreaseVolume();
				}
				else if (l_Token.m_Type == CommandToken::TYPE_LOWER)
				{
					SoundDecreaseVolume();
				}
			}
			break;
			
			// case CommandToken::TYPE_MUTE:
			// {
			// 	SoundMute(true);
			// }
			// break;
			
			// case CommandToken::TYPE_UNMUTE:
			// {
			// 	SoundMute(false);
			// }
			// break;
			
			case CommandToken::TYPE_STATUS:
			{
				// Play status speech.
				SoundAddToQueue(DATADIR "audio/running.wav");	
				
				if (ScheduleIsRunning() == true)
				{
					SoundAddToQueue(DATADIR "audio/sched_running.wav");	
				}
				
				if (s_Input.IsConnected() == true)
				{
					SoundAddToQueue(DATADIR "audio/control_connected.wav");
				}
			}
			break;
			
			default:	
			{
			}
			break;
		}
	}
}

// Get keyboard input.
//
// p_KeyboardInputBuffer:			(input/output) The input buffer.
// p_KeyboardInputBufferSize:		(input/output) How much of the input buffer is in use.
// p_KeyboardInputBufferCapacity:	The capacity of the input buffer.
//
// returns:		True if the quit command was processed, false otherwise.
//
static bool ProcessKeyboardInput(char* p_KeyboardInputBuffer, unsigned int& p_KeyboardInputBufferSize, 
	unsigned int const p_KeyboardInputBufferCapacity)
{
	// Try to get keyboard commands.
	auto const l_InputKey = getch();
	if ((l_InputKey == ERR) || (isascii(l_InputKey) == false))
	{
		return false;
	}
	
	// Get the character.
	auto const l_NextChar = static_cast<char>(l_InputKey);

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
	TokenizeCommandString(l_CommandTokens, p_KeyboardInputBuffer);

	// Parse command tokens.
	ParseCommandTokens(l_CommandTokens);

	// Prepare for a new command.
	p_KeyboardInputBufferSize = 0;

	if (strcmp(p_KeyboardInputBuffer, "quit") == 0)
	{
		 return true;
	}
	
	return false;
}

// Process socket communication.
//
// returns:		True if the quit command was received, false otherwise.
//
static bool ProcessSocketCommunication()
{
	// Attempt to accept an incoming connection.
	auto const l_ConnectionSocket = accept(s_ListeningSocket, nullptr, nullptr);
	
	if (l_ConnectionSocket < 0)
	{
		return false;
	}
	
	// Got a connection.
	LoggerAddMessage("Got a new connection.");
	
	// Try to read data.
	static constexpr unsigned int l_MessageBufferCapacity = 100;
	char l_MessageBuffer[l_MessageBufferCapacity];
	
	auto const l_NumReceivedBytes = recv(l_ConnectionSocket, l_MessageBuffer, l_MessageBufferCapacity - 1, 
		0);
	
	if (l_NumReceivedBytes <= 0)
	{
		LoggerAddMessage("Connection closed, error receiving.");
	
		// Close the connection.
		close(l_ConnectionSocket);
		return false;
	}
	
	// Terminate.
	l_MessageBuffer[l_NumReceivedBytes] = '\0';
	
	LoggerAddMessage("Received \"%s\".", l_MessageBuffer);
	
	// Handle the message, if necessary.
	auto l_Done = false;
	
	if (strcmp(l_MessageBuffer, "shutdown") == 0)
	{
		l_Done = true;
	}
	else
	{
		// Parse a command.

		// Tokenize the message.
		std::vector<CommandToken> l_CommandTokens;
		TokenizeCommandString(l_CommandTokens,	l_MessageBuffer);

		// Parse command tokens.
		ParseCommandTokens(l_CommandTokens);
	}
	
	LoggerAddMessage("Connection closed.");
	
	// Close the connection.
	close(l_ConnectionSocket);
	
	return l_Done;
}

// Send a message to the daemon process.
//
// p_Message:	The message to send.
//
static void SendMessageToDaemon(char const* p_Message)
{
	// Create a sending socket.
	auto const l_SendingSocket = socket(AF_UNIX, SOCK_STREAM, 0);
	
	if (l_SendingSocket < 0)
	{
		printf("Failed to create sending socket.\n");
		return;
	}
	
	sockaddr_un l_SendingAddress;
	{
		l_SendingAddress.sun_family = AF_UNIX;
		strncpy(l_SendingAddress.sun_path, TEMPDIR "sandman.sock", 
			sizeof(l_SendingAddress.sun_path) - 1);
	}
		
	// Attempt to connect to the daemon.
	if (connect(l_SendingSocket, reinterpret_cast<sockaddr*>(&l_SendingAddress), sizeof(sockaddr_un)) < 0)
	{
		printf("Failed to connect to the daemon.\n");
		close(l_SendingSocket);
		return;
	}

	// Send the message.
	if (send(l_SendingSocket, p_Message, strlen(p_Message), 0) < 0)
	{
		printf("Failed to send \"%s\" message to the daemon.\n", p_Message);
		close(l_SendingSocket);
		return;	
	}
	
	printf("Sent \"%s\" message to the daemon.\n", p_Message);
	
	// Close the connection.
	close(l_SendingSocket);
}

// Handle the commandline arguments.
//
//	p_Arguments:		The argument list.
// p_ArgumentCount:	The number of arguments in the list.
//
// Returns:	True if the program should exit, false if it should continue.
//
static bool HandleCommandLine(char** p_Arguments, int p_ArgumentCount)
{
	for (unsigned int l_ArgumentIndex = 0; l_ArgumentIndex < p_ArgumentCount; l_ArgumentIndex++)
	{
		auto const* l_Argument = p_Arguments[l_ArgumentIndex];
		
		// Start as a daemon?
		if (strcmp(l_Argument, "--daemon") == 0)
		{
			s_DaemonMode = true;
			break;
		}
		else if (strcmp(l_Argument, "--shutdown") == 0)
		{
			SendMessageToDaemon("shutdown");
			return true;
		}
		else 
		{
			// We are going to see if there is a command to send to the daemon.
			static char const* s_CommandPrefix = "--command=";	
			
			auto const* l_CommandStringStart = strstr(l_Argument, s_CommandPrefix);
			
			if (l_CommandStringStart != nullptr)
			{
				// Skip the command prefix.
				l_CommandStringStart += strlen(s_CommandPrefix);
				
				static constexpr unsigned int l_CommandBufferCapacity = 100;
				char l_CommandBuffer[l_CommandBufferCapacity];
				
				// Copy only the actual command.
				strncpy(l_CommandBuffer, l_CommandStringStart, l_CommandBufferCapacity - 1);
				l_CommandBuffer[l_CommandBufferCapacity - 1] = '\0';
				
				// Replace '_' with ' '.
				auto* l_CurrentCharacter = l_CommandBuffer;
				
				while (*l_CurrentCharacter != '\0')
				{
					if (*l_CurrentCharacter == '_')
					{
						*l_CurrentCharacter = ' ';
					}
					
					l_CurrentCharacter++;
				}
				
				// Send the command to the daemon.
				SendMessageToDaemon(l_CommandBuffer);
				return true;
			}
		}
	}
	
	return false;
}

int main(int argc, char** argv)
{		
	// Deal with command line arguments.
	if (HandleCommandLine(argv, argc) == true) 
	{
		return 0;
	}
	
	// Initialization.
	if (Initialize() == false)
	{
		Uninitialize();
		return 0;
	}

	// Store a keyboard input here.
	static constexpr unsigned int l_KeyboardInputBufferCapacity = 128;
	char l_KeyboardInputBuffer[l_KeyboardInputBufferCapacity];
	unsigned int l_KeyboardInputBufferSize = 0;

	auto l_Done = false;
	while (l_Done == false)
	{
		// We're gonna track the framerate.
		Time l_FrameStartTime;
		TimerGetCurrent(l_FrameStartTime);
		
		if (s_DaemonMode == false)
		{
			// Process keyboard input.
			l_Done = ProcessKeyboardInput(l_KeyboardInputBuffer, l_KeyboardInputBufferSize, 
				l_KeyboardInputBufferCapacity);
		}

		#if defined (USE_INTERNAL_SPEECH_RECOGNITION)
		
			// Process speech recognition.
			char const* l_RecognizedSpeech = nullptr;
			if (s_Recognizer.Process(l_RecognizedSpeech) == false)
			{
				LoggerAddMessage("Error during speech recognition.");
				l_Done = true;
			}

			if (l_RecognizedSpeech != nullptr)
			{
				// Parse a command.
				static_assert(false, "Need to validate/ignore the command prefix.");
				
				// Store command tokens here.
				static constexpr unsigned int l_CommandTokenBufferCapacity = 32;
				CommandTokenTypes l_CommandTokenBuffer[l_CommandTokenBufferCapacity];
				unsigned int l_CommandTokenBufferSize = 0;

				// Tokenize the speech.
				TokenizeCommandString(l_CommandTokenBufferSize, l_CommandTokenBuffer, l_CommandTokenBufferCapacity,
					l_RecognizedSpeech);

				// Parse command tokens.
				ParseCommandTokens(l_CommandTokenBufferSize, l_CommandTokenBuffer);
			}
			
		#endif // defined (USE_INTERNAL_SPEECH_RECOGNITION)

		// Process controls.
		ControlsProcess();
		
		// Process the input.
		s_Input.Process();
		
		// Process the schedule.
		ScheduleProcess();
		
		// Process sound.
		SoundProcess();
		
		if (s_DaemonMode == true)
		{
			// Process socket communication.
			l_Done = ProcessSocketCommunication();
		}

		// Get the duration of the frame in nanoseconds.
		Time l_FrameEndTime;
		TimerGetCurrent(l_FrameEndTime);
		
		float const l_FrameDurationMS = TimerGetElapsedMilliseconds(l_FrameStartTime, l_FrameEndTime);
		auto const l_FrameDurationNS = static_cast<unsigned long>(l_FrameDurationMS * 1.0e6f);
		
		// If the frame is shorter than the duration corresponding to the desired framerate, sleep the
		// difference off.
		unsigned long const l_TargetFrameDurationNS = 1000000000 / 60;
		
		if (l_FrameDurationNS < l_TargetFrameDurationNS)
		{
			timespec l_SleepTime;
			l_SleepTime.tv_sec = 0;
			l_SleepTime.tv_nsec = l_TargetFrameDurationNS - l_FrameDurationNS;
			
			timespec l_RemainingTime;
			nanosleep(&l_SleepTime, &l_RemainingTime);
		}
	}

	LoggerAddMessage("Uninitializing.");
	
	// Cleanup.
	Uninitialize();
	return 0;
}
