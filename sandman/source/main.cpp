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
#include "logger.h"
#include "schedule.h"
#include "sound.h"

#if defined (USE_INTERNAL_SPEECH_RECOGNITION)
	#include "speech_recognizer.h"
#endif // defined (USE_INTERNAL_SPEECH_RECOGNITION)
	
#include "timer.h"

#define DATADIR		AM_DATADIR
#define CONFIGDIR	AM_CONFIGDIR
#define TEMPDIR		AM_TEMPDIR

// Types
//

// Types of command tokens.
enum CommandTokenTypes
{
	COMMAND_TOKEN_INVALID = -1,

	COMMAND_TOKEN_SAND,
	COMMAND_TOKEN_MAN,
	COMMAND_TOKEN_BACK,
	COMMAND_TOKEN_LEGS,
	COMMAND_TOKEN_ELEVATION,
	COMMAND_TOKEN_RAISE,
	COMMAND_TOKEN_LOWER,
	COMMAND_TOKEN_STOP,
	COMMAND_TOKEN_VOLUME,
	// COMMAND_TOKEN_MUTE,
	// COMMAND_TOKEN_UNMUTE,
	COMMAND_TOKEN_SCHEDULE,
	COMMAND_TOKEN_START,
	COMMAND_TOKEN_STATUS,
	
	NUM_COMMAND_TOKEN_TYPES,
};

// Types of controls.
enum ControlTypes
{
	CONTROL_BACK = 0,
	CONTROL_LEGS,
	CONTROL_ELEVATION,

	NUM_CONTROL_TYPES,
};

// Locals
//

// Names for each command token.
static char const* const s_CommandTokenNames[] = 
{
	"sand",			// COMMAND_TOKEN_SAND
	"man",			// COMMAND_TOKEN_MAN
	"back",			// COMMAND_TOKEN_BACK
	"legs",			// COMMAND_TOKEN_LEGS
	"elevation",	// COMMAND_TOKEN_ELEVATION
	"raise",		// COMMAND_TOKEN_RAISE
	"lower",		// COMMAND_TOKEN_LOWER
	"stop",			// COMMAND_TOKEN_STOP
	"volume",		// COMMAND_TOKEN_VOLUME
	// "mute",			// COMMAND_TOKEN_MUTE
	// "unmute",		// COMMAND_TOKEN_UNMUTE
	"schedule",		// COMMAND_TOKEN_SCHEDULE
	"start",		// COMMAND_TOKEN_START
	"status",		// COMMAND_TOKEN_STATUS
};

// The configuration parameters for each control.
static ControlConfig s_ControlConfigs[] = 
{
	{ "back", 0, 1, 13000 }, 	// CONTROL_BACK
	{ "legs", 2, 3, 13000 }, 	// CONTROL_LEGS
	{ "elev", 4, 5, 13000 },	// CONTROL_ELEVATION
};

// The controls.
static Control s_Controls[NUM_CONTROL_TYPES];

// Whether controls have been initialized.
static bool s_ControlsInitialized = false;

#if defined (USE_INTERNAL_SPEECH_RECOGNITION)

	// The speech recognizer.
	static SpeechRecognizer s_Recognizer;
	
#endif // defined (USE_INTERNAL_SPEECH_RECOGNITION)

// Whether to start as a daemon or terminal program.
static bool s_DaemonMode = false;

// Used to listen for connections.
static int s_ListeningSocket = -1;

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

// Initialize program components.
//
// returns:		True for success, false otherwise.
//
static bool Initialize()
{
	// Read the config.
	Config l_Config;
	if (l_Config.ReadFromFile(CONFIGDIR "sandman.conf") == false)
	{
		return false;
	}
	
	if (s_DaemonMode == true)
	{
		printf("Initializing as a daemon.\n");
		
		// Fork a child off of the parent process.
		pid_t l_ProcessID = fork();
		
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
		if (LoggerInitialize(TEMPDIR "sandman.log", (s_DaemonMode == false)) == false)
		{
			return false;
		}

		// Need a new session ID.
		pid_t l_SessionID = setsid();
		
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
		if (LoggerInitialize(TEMPDIR "sandman.log", (s_DaemonMode == false)) == false)
		{
			return false;
		}
	}

	#if defined (USE_INTERNAL_SPEECH_RECOGNITION)

		// Initialize speech recognition.
		if (s_Recognizer.Initialize(l_Config.GetInputDeviceName(), l_Config.GetInputSampleRate(), 
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
	for (unsigned int l_ControlIndex = 0; l_ControlIndex < NUM_CONTROL_TYPES; l_ControlIndex++)
	{		
		s_Controls[l_ControlIndex].Initialize(s_ControlConfigs[l_ControlIndex]);
	}

	// Set control durations.
	Control::SetDurations(l_Config.GetControlMaxMovingDurationMS(), 
		l_Config.GetControlCoolDownDurationMS());
	
	// Enable all controls.
	Control::Enable(true);
	
	// Controls have been initialized.
	s_ControlsInitialized = true;
	
	// Initialize the schedule.
	ScheduleInitialize(s_Controls, NUM_CONTROL_TYPES);
	
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
		for (unsigned int l_ControlIndex = 0; l_ControlIndex < NUM_CONTROL_TYPES; l_ControlIndex++)
		{
			s_Controls[l_ControlIndex].Uninitialize();
		}
	}
	
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
		if (p_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_BACK)
		{
			// Next token.
			l_TokenIndex++;
			if (l_TokenIndex >= p_CommandTokenBufferSize)
			{
				break;
			}

			if (p_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_RAISE)
			{
				s_Controls[CONTROL_BACK].SetDesiredAction(Control::ACTION_MOVING_UP,
					Control::MODE_TIMED);
			}
			else if (p_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_LOWER)
			{
				s_Controls[CONTROL_BACK].SetDesiredAction(Control::ACTION_MOVING_DOWN, 
					Control::MODE_TIMED);
			}
		}
		else if (p_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_LEGS)
		{
			// Next token.
			l_TokenIndex++;
			if (l_TokenIndex >= p_CommandTokenBufferSize)
			{
				break;
			}

			if (p_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_RAISE)
			{
				s_Controls[CONTROL_LEGS].SetDesiredAction(Control::ACTION_MOVING_UP, Control::MODE_TIMED);
			}
			else if (p_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_LOWER)
			{
				s_Controls[CONTROL_LEGS].SetDesiredAction(Control::ACTION_MOVING_DOWN, 
					Control::MODE_TIMED);
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

			if (p_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_RAISE)
			{
				s_Controls[CONTROL_ELEVATION].SetDesiredAction(Control::ACTION_MOVING_UP, 
					Control::MODE_TIMED);
			}
			else if (p_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_LOWER)
			{
				s_Controls[CONTROL_ELEVATION].SetDesiredAction(Control::ACTION_MOVING_DOWN, 
					Control::MODE_TIMED);
			}
		}
		else if (p_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_STOP)
		{
			// Stop controls.
			for (unsigned int l_ControlIndex = 0; l_ControlIndex < NUM_CONTROL_TYPES; l_ControlIndex++)
			{
				s_Controls[l_ControlIndex].SetDesiredAction(Control::ACTION_STOPPED, 
					Control::MODE_MANUAL);
			}
		}
		else if (p_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_SCHEDULE)
		{
			// Next token.
			l_TokenIndex++;
			if (l_TokenIndex >= p_CommandTokenBufferSize)
			{
				break;
			}
		
			if (p_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_START)
			{
				ScheduleStart();
			}
			else if (p_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_STOP)
			{
				ScheduleStop();
			}			
		}
		else if (p_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_VOLUME)
		{
			// Next token.
			l_TokenIndex++;
			if (l_TokenIndex >= p_CommandTokenBufferSize)
			{
				break;
			}

			if (p_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_RAISE)
			{
				SoundIncreaseVolume();
			}
			else if (p_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_LOWER)
			{
				SoundDecreaseVolume();
			}
		}
		// else if (p_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_MUTE)
		// {
		// 	SoundMute(true);
		// }
		// else if (p_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_UNMUTE)
		// {
		// 	SoundMute(false);
		// }
		else if (p_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_STATUS)
		{
			// Play status speech.
			SoundAddToQueue(DATADIR "audio/running.wav");	
			
			if (ScheduleIsRunning() == true)
			{
				SoundAddToQueue(DATADIR "audio/sched_running.wav");	
			}
		}
	}

	// All tokens parsed.
	p_CommandTokenBufferSize = 0;
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
	int l_InputKey = getch();
	if ((l_InputKey == ERR) || (isascii(l_InputKey) == false))
	{
		return false;
	}
	
	// Get the character.
	char l_NextChar = static_cast<char>(l_InputKey);

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

	// Store command tokens here.
	unsigned int const l_CommandTokenBufferCapacity = 32;
	CommandTokenTypes l_CommandTokenBuffer[l_CommandTokenBufferCapacity];
	unsigned int l_CommandTokenBufferSize = 0;

	// Tokenize the speech.
	TokenizeCommandString(l_CommandTokenBufferSize, l_CommandTokenBuffer, l_CommandTokenBufferCapacity,
		p_KeyboardInputBuffer);

	// Parse command tokens.
	ParseCommandTokens(l_CommandTokenBufferSize, l_CommandTokenBuffer);

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
	int l_ConnectionSocket = accept(s_ListeningSocket, NULL, NULL);
	
	if (l_ConnectionSocket < 0)
	{
		return false;
	}
	
	// Got a connection.
	LoggerAddMessage("Got a new connection.");
	
	// Try to read data.
	unsigned int const l_MessageBufferCapacity = 100;
	char l_MessageBuffer[l_MessageBufferCapacity];
	
	int l_NumReceivedBytes = recv(l_ConnectionSocket, l_MessageBuffer, l_MessageBufferCapacity - 1, 0);
	
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
	bool l_Done = false;
	
	if (strcmp(l_MessageBuffer, "shutdown") == 0)
	{
		l_Done = true;
	}
	else
	{
		// Parse a command.

		// Store command tokens here.
		unsigned int const l_CommandTokenBufferCapacity = 32;
		CommandTokenTypes l_CommandTokenBuffer[l_CommandTokenBufferCapacity];
		unsigned int l_CommandTokenBufferSize = 0;

		// Tokenize the message.
		TokenizeCommandString(l_CommandTokenBufferSize, l_CommandTokenBuffer, l_CommandTokenBufferCapacity,
			l_MessageBuffer);

		// Parse command tokens.
		ParseCommandTokens(l_CommandTokenBufferSize, l_CommandTokenBuffer);
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
	int l_SendingSocket = socket(AF_UNIX, SOCK_STREAM, 0);
	
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

int main(int argc, char** argv)
{		
	// Deal with command line arguments.
	for (unsigned int l_ArgumentIndex = 0; l_ArgumentIndex < argc; l_ArgumentIndex++)
	{
		char const* l_Argument = argv[l_ArgumentIndex];
		
		// Start as a daemon?
		if (strcmp(l_Argument, "--daemon") == 0)
		{
			s_DaemonMode = true;
			break;
		}
		else if (strcmp(l_Argument, "--shutdown") == 0)
		{
			SendMessageToDaemon("shutdown");
			return 0;
		}
		else 
		{
			// We are going to see if there is a command to send to the daemon.
			static char const* s_CommandPrefix = "--command=";	
			
			char const* l_CommandStringStart = strstr(l_Argument, s_CommandPrefix);
			
			if (l_CommandStringStart != NULL)
			{
				// Skip the command prefix.
				l_CommandStringStart += strlen(s_CommandPrefix);
				
				unsigned int const l_CommandBufferCapacity = 100;
				char l_CommandBuffer[l_CommandBufferCapacity];

				unsigned int const l_CommandStringLength = strlen(l_CommandStringStart);
				
				// Copy only the actual command.
				snprintf(l_CommandBuffer, l_CommandBufferCapacity, "sand man %.*s", 
					l_CommandStringLength, l_CommandStringStart);
				l_CommandBuffer[l_CommandBufferCapacity - 1] = '\0';
				
				// Replace '_' with ' '.
				char* l_CurrentCharacter = l_CommandBuffer;
				
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
				return 0;
			}
		}
	}
	
	// Initialization.
	if (Initialize() == false)
	{
		Uninitialize();
		return 0;
	}

	// Store a keyboard input here.
	unsigned int const l_KeyboardInputBufferCapacity = 128;
	char l_KeyboardInputBuffer[l_KeyboardInputBufferCapacity];
	unsigned int l_KeyboardInputBufferSize = 0;

	bool l_Done = false;
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
			char const* l_RecognizedSpeech = NULL;
			if (s_Recognizer.Process(l_RecognizedSpeech) == false)
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
				TokenizeCommandString(l_CommandTokenBufferSize, l_CommandTokenBuffer, l_CommandTokenBufferCapacity,
					l_RecognizedSpeech);

				// Parse command tokens.
				ParseCommandTokens(l_CommandTokenBufferSize, l_CommandTokenBuffer);
			}
			
		#endif // defined (USE_INTERNAL_SPEECH_RECOGNITION)

		// Process controls.
		for (unsigned int l_ControlIndex = 0; l_ControlIndex < NUM_CONTROL_TYPES; l_ControlIndex++)
		{
			s_Controls[l_ControlIndex].Process();
		}

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
		unsigned long const l_FrameDurationNS = static_cast<unsigned long>(l_FrameDurationMS * 1.0e6f);
		
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
