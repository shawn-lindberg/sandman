#include <cctype>
#include <cstdio>
#include <ctime>

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "command.h"
#include "config.h"
#include "control.h"
#include "input.h"
#include "logger.h"
#include "mqtt.h"
#include "ncurses_context.h"
#include "notification.h"
#include "reports.h"
#include "schedule.h"
#include "timer.h"

// Types
//


// Locals
//

// Whether controls have been initialized.
static bool s_ControlsInitialized = false;

// The input device.
static Input s_Input;

// Whether to start as a daemon or terminal program.
static bool s_DaemonMode = false;

// Used to listen for connections.
static int s_ListeningSocket = -1;

static int s_ExitCode = 0;

// Functions
//

// Initialize program components.
//
// returns:		True for success, false otherwise.
//
static bool Initialize()
{
	if (s_DaemonMode == true)
	{
		std::printf("Initializing as a daemon.\n");

		// Fork a child off of the parent process.
		auto const l_ProcessID = fork();

		// Legitimate failure.
		if (l_ProcessID < 0)
		{
			s_ExitCode = 1;
			return false;
		}

		// The parent gets the ID of the child and exits.
		if (l_ProcessID > 0)
		{
			s_ExitCode = 1;
			return false;
		}

		// The child gets 0 and continues.

		// Allow file access.
		umask(0);

		// Initialize logging.
		if (LoggerInitialize(SANDMAN_TEMP_DIR "sandman.log") == false)
		{
			s_ExitCode = 1;
			return false;
		}

		// Need a new session ID.
		auto const l_SessionID = setsid();

		if (l_SessionID < 0)
		{
			LoggerAddMessage("Failed to get new session ID for daemon.");
			s_ExitCode = 1;
			return false;
		}

		// Change the current working directory.
		if (chdir(SANDMAN_TEMP_DIR) < 0)
		{
			LoggerAddMessage("Failed to change working directory to \"%s\" ID for daemon.",
								  SANDMAN_TEMP_DIR);
			s_ExitCode = 1;
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
			s_ExitCode = 1;
			return false;
		}

		// Set to non-blocking.
		if (fcntl(s_ListeningSocket, F_SETFL, O_NONBLOCK) < 0)
		{
			LoggerAddMessage("Failed to make listening socket non-blocking.");
			s_ExitCode = 1;
			return false;
		}

		sockaddr_un l_ListeningAddress;
		{
			l_ListeningAddress.sun_family = AF_UNIX;
			std::strncpy(l_ListeningAddress.sun_path, SANDMAN_TEMP_DIR "sandman.sock",
							 sizeof(l_ListeningAddress.sun_path) - 1);
		}

		// Unlink the file if needed.
		unlink(l_ListeningAddress.sun_path);

		// Bind the socket to the file.
		if (bind(s_ListeningSocket, reinterpret_cast<sockaddr*>(&l_ListeningAddress),
					sizeof(sockaddr_un)) < 0)
		{
			LoggerAddMessage("Failed to bind listening socket.");
			s_ExitCode = 1;
			return false;
		}

		// Mark the socket for listening.
		if (listen(s_ListeningSocket, 5) < 0)
		{
			LoggerAddMessage("Failed to mark listening socket to listen.");
			s_ExitCode = 1;
			return false;
		}

		// Sockets setup!
	}
	else
	{
		NCurses::Initialize();

		// Initialize logging.
		if (LoggerInitialize(SANDMAN_TEMP_DIR "sandman.log") == false)
		{
			s_ExitCode = 1;
			return false;
		}

		LoggerEchoToScreen(true);
	}

	// Read the config.
	Config l_Config;
	if (l_Config.ReadFromFile(SANDMAN_CONFIG_DIR "sandman.conf") == false)
	{
		s_ExitCode = 1;
		return false;
	}

	// Initialize MQTT.
	if (MQTTInitialize() == false)
	{
		s_ExitCode = 1;
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

	// Initialize reports.
	ReportsInitialize();

	// Initialize the commands.
	CommandInitialize(s_Input);

	NotificationPlay("initialized");

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

	// Uninitialize the commands.
	CommandUninitialize();

	// Uninitialize reports.
	ReportsUninitialize();

	// Uninitialize the schedule.
	ScheduleUninitialize();

	// Uninitialize MQTT.
	MQTTUninitialize();

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
		NCurses::Uninitialize();
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
static bool ProcessKeyboardInput(char* p_KeyboardInputBuffer,
											unsigned int& p_KeyboardInputBufferSize,
											unsigned int const p_KeyboardInputBufferCapacity)
{

	// Pointer to the user input window.
	auto const l_Window = NCurses::GetInputWindow();

	// This is the offset for where to place the cursor after a character is typed.
	//
	// As the user types, this increases.
	// When the buffer is flushed or the user presses "enter", this resets to zero.
	static int s_InputWindowCursorOffsetX{ 0 };

	// Try to get keyboard commands.

	int const l_InputKey{ mvwgetch(l_Window, NCurses::INPUT_WINDOW_CURSOR_START_Y,
											 NCurses::INPUT_WINDOW_CURSOR_START_X +
											 s_InputWindowCursorOffsetX) };

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
			wmove(l_Window, NCurses::INPUT_WINDOW_CURSOR_START_Y,
					NCurses::INPUT_WINDOW_CURSOR_START_X);

			// Clear the line.
			wclrtoeol(l_Window);

			// Redraw the border.
			box(l_Window, 0 /* use default vertical character */,
				 0 /* use default horizontal character */);

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

	auto const l_NumReceivedBytes =
		recv(l_ConnectionSocket, l_MessageBuffer, l_MessageBufferCapacity - 1, 0);

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

	if (std::strcmp(l_MessageBuffer, "shutdown") == 0)
	{
		l_Done = true;
	}
	else
	{
		// Parse a command.

		// Tokenize the message.
		std::vector<CommandToken> l_CommandTokens;
		CommandTokenizeString(l_CommandTokens, l_MessageBuffer);

		// Parse command tokens.
		CommandParseTokens(l_CommandTokens);
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
		std::printf("Failed to create sending socket.\n");
		return;
	}

	sockaddr_un l_SendingAddress;
	{
		l_SendingAddress.sun_family = AF_UNIX;
		std::strncpy(l_SendingAddress.sun_path, SANDMAN_TEMP_DIR "sandman.sock",
						 sizeof(l_SendingAddress.sun_path) - 1);
	}

	// Attempt to connect to the daemon.
	if (connect(l_SendingSocket, reinterpret_cast<sockaddr*>(&l_SendingAddress),
					sizeof(sockaddr_un)) < 0)
	{
		std::printf("Failed to connect to the daemon.\n");
		close(l_SendingSocket);
		return;
	}

	// Send the message.
	if (send(l_SendingSocket, p_Message, std::strlen(p_Message), 0) < 0)
	{
		std::printf("Failed to send \"%s\" message to the daemon.\n", p_Message);
		close(l_SendingSocket);
		return;
	}

	std::printf("Sent \"%s\" message to the daemon.\n", p_Message);

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
static bool HandleCommandLine(char** p_Arguments, unsigned int p_ArgumentCount)
{
	for (unsigned int l_ArgumentIndex = 0; l_ArgumentIndex < p_ArgumentCount; l_ArgumentIndex++)
	{
		auto const* l_Argument = p_Arguments[l_ArgumentIndex];

		// Start as a daemon?
		if (std::strcmp(l_Argument, "--daemon") == 0)
		{
			s_DaemonMode = true;
			break;
		}
		else if (std::strcmp(l_Argument, "--shutdown") == 0)
		{
			SendMessageToDaemon("shutdown");
			return true;
		}
		else
		{
			// We are going to see if there is a command to send to the daemon.
			static char const* s_CommandPrefix = "--command=";

			auto const* l_CommandStringStart = std::strstr(l_Argument, s_CommandPrefix);

			if (l_CommandStringStart != nullptr)
			{
				// Skip the command prefix.
				l_CommandStringStart += std::strlen(s_CommandPrefix);

				static constexpr unsigned int l_CommandBufferCapacity = 100;
				char l_CommandBuffer[l_CommandBufferCapacity];

				// Copy only the actual command.
				std::strncpy(l_CommandBuffer, l_CommandStringStart, l_CommandBufferCapacity - 1);
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
		return s_ExitCode;
	}

	// Initialization.
	if (Initialize() == false)
	{
		Uninitialize();
		return s_ExitCode;
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

		// Process command.
		CommandProcess();

		// Process controls.
		ControlsProcess();

		// Process the input.
		s_Input.Process();

		// Process MQTT.
		MQTTProcess();

		// Process the schedule.
		ScheduleProcess();

		// Process the reports.
		ReportsProcess();

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
	return s_ExitCode;
}
