#include <cstddef>
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
#include "shell.h"
#include "notification.h"
#include "reports.h"
#include "schedule.h"
#include "timer.h"

// Types
//

enum ProgramMode
{
	kProgramModeInteractive = 0,
	kProgramModeDaemon,
	kProgramModeDocker,

	kNumProgramModes
};

// Locals
//

// Whether controls have been initialized.
static bool s_controlsInitialized = false;

// The input device.
static Input s_input;

// What mode the program is running in.
static ProgramMode s_programMode = kProgramModeInteractive;

// Used to listen for connections.
static int s_listeningSocket = -1;

static int s_exitCode = 0;

// Functions
//

// Do daemon specific initialization.
// 
// Returns: True on success, false on failure.
// 
static bool InitializeDaemon()
{
	std::printf("Initializing as a daemon.\n");

	// Fork a child off of the parent process.
	auto const processID = fork();

	// Legitimate failure.
	if (processID < 0)
	{
		s_exitCode = 1;
		return false;
	}

	// The parent gets the ID of the child and exits.
	if (processID > 0)
	{
		s_exitCode = 1;
		return false;
	}

	// The child gets 0 and continues.

	// Allow file access.
	umask(0);

	// Initialize logging.
	if (Logger::Initialize(SANDMAN_TEMP_DIR "sandman.log") == false)
	{
		s_exitCode = 1;
		return false;
	}

	// Need a new session ID.
	auto const sessionID = setsid();

	if (sessionID < 0)
	{
		Logger::WriteLine(Shell::Red("Failed to get new session ID for daemon."));
		s_exitCode = 1;
		return false;
	}

	// Change the current working directory.
	if (chdir(SANDMAN_TEMP_DIR) < 0)
	{
		Logger::WriteFormattedLine(Shell::Red,
											"Failed to change working directory to \"%s\" ID for daemon.",
											SANDMAN_TEMP_DIR);
		s_exitCode = 1;
		return false;
	}

	// Close stdin, stdout, stderr.
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
	s_listeningSocket = socket(AF_UNIX, SOCK_STREAM, 0);

	if (s_listeningSocket < 0)
	{
		Logger::WriteLine(Shell::Red("Failed to create listening socket."));
		s_exitCode = 1;
		return false;
	}

	// Set to non-blocking.
	if (fcntl(s_listeningSocket, F_SETFL, O_NONBLOCK) < 0)
	{
		Logger::WriteLine(Shell::Red("Failed to make listening socket non-blocking."));
		s_exitCode = 1;
		return false;
	}

	sockaddr_un listeningAddress;
	{
		listeningAddress.sun_family = AF_UNIX;
		std::strncpy(listeningAddress.sun_path, SANDMAN_TEMP_DIR "sandman.sock",
							sizeof(listeningAddress.sun_path) - 1);
	}

	// Unlink the file if needed.
	unlink(listeningAddress.sun_path);

	// Bind the socket to the file.
	if (bind(s_listeningSocket, reinterpret_cast<sockaddr*>(&listeningAddress),
				sizeof(sockaddr_un)) < 0)
	{
		Logger::WriteLine(Shell::Red("Failed to bind listening socket."));
		s_exitCode = 1;
		return false;
	}

	// Mark the socket for listening.
	if (listen(s_listeningSocket, 5) < 0)
	{
		Logger::WriteLine(Shell::Red("Failed to mark listening socket to listen."));
		s_exitCode = 1;
		return false;
	}

	// Sockets setup!
	return true;
}

// Initialize program components.
//
// returns:		True for success, false otherwise.
//
static bool Initialize()
{
	switch (s_programMode)
	{
		case kProgramModeDaemon:
		{
			if (InitializeDaemon() == false)
			{
				return false;
			}
		}
		break;

		case kProgramModeDocker:
		{
			// Initialize logging.
			if (Logger::Initialize(SANDMAN_TEMP_DIR "sandman.log") == false)
			{
				s_exitCode = 1;
				return false;
			}
		}
		break;

		case kProgramModeInteractive:
		{
			// Initialize logging.
			if (Logger::Initialize(SANDMAN_TEMP_DIR "sandman.log") == false)
			{
				s_exitCode = 1;
				return false;
			}

			Shell::Initialize();
			Logger::SetEchoToScreen(true);
		}
		break;

		default:
		{
			std::printf("Undefined program mode!");
		}
		return false;
	};

	// Read the config.
	Config config;
	if (config.ReadFromFile(SANDMAN_CONFIG_DIR "sandman.conf") == false)
	{
		s_exitCode = 1;
		return false;
	}

	// Initialize MQTT.
	if (MQTTInitialize() == false)
	{
		s_exitCode = 1;
		return false;
	}

	// Initialize controls.
	static constexpr bool kEnableGPIO = true;
	ControlsInitialize(config.GetControlConfigs(), kEnableGPIO);

	// Set control durations.
	Control::SetDurations(config.GetControlMaxMovingDurationMS(),
								 config.GetControlCoolDownDurationMS());

	// Enable all controls.
	Control::Enable(true);

	// Controls have been initialized.
	s_controlsInitialized = true;

	// Initialize the input device.
	s_input.Initialize(config.GetInputDeviceName(), config.GetInputBindings());

	// Initialize the schedule.
	ScheduleInitialize();

	// Initialize reports.
	ReportsInitialize();

	// Initialize the commands.
	CommandInitialize(s_input);

	NotificationPlay("initialized");

	return true;
}

// Uninitialize program components.
//
static void Uninitialize()
{
	// Close the listening socket, if there was one.
	if (s_listeningSocket >= 0)
	{
		close(s_listeningSocket);
	}

	// Uninitialize the commands.
	CommandUninitialize();

	// Uninitialize reports.
	ReportsUninitialize();

	// Uninitialize the schedule.
	ScheduleUninitialize();

	// Uninitialize MQTT.
	MQTTUninitialize();

	if (s_controlsInitialized == true)
	{
		// Disable all controls.
		Control::Enable(false);

		// Uninitialize controls.
		ControlsUninitialize();
	}

	// Uninitialize the input.
	s_input.Uninitialize();

	// Uninitialize logging.
	Logger::Uninitialize();

	if (s_programMode == kProgramModeInteractive)
	{
		Shell::Uninitialize();
	}
}

// Process socket communication.
//
// returns:		True if the quit command was received, false otherwise.
//
static bool ProcessSocketCommunication()
{
	// Attempt to accept an incoming connection.
	auto const connectionSocket = accept(s_listeningSocket, nullptr, nullptr);

	if (connectionSocket < 0)
	{
		return false;
	}

	// Got a connection.
	Logger::WriteFormattedLine("Got a new connection.");

	// Try to read data.
	static constexpr std::size_t kMessageBufferCapacity{ 100u };
	char messageBuffer[kMessageBufferCapacity];

	// Assert can subtract one from this without underflow.
	static_assert(kMessageBufferCapacity >= 1u);

	auto const numReceivedBytes = recv(connectionSocket, messageBuffer,
												  kMessageBufferCapacity - 1u, 0);

	if (numReceivedBytes <= 0)
	{
		Logger::WriteFormattedLine("Connection closed, error receiving.");

		// Close the connection.
		close(connectionSocket);
		return false;
	}

	// Terminate.
	messageBuffer[numReceivedBytes] = '\0';

	Logger::WriteFormattedLine("Received \"%s\".", messageBuffer);

	// Handle the message, if necessary.
	auto done = false;

	if (std::strcmp(messageBuffer, "shutdown") == 0)
	{
		done = true;
	}
	else
	{
		// Parse a command.

		// Tokenize the message.
		std::vector<CommandToken> commandTokens;
		CommandTokenizeString(commandTokens, messageBuffer);

		// Parse command tokens.
		CommandParseTokens(commandTokens);
	}

	Logger::WriteFormattedLine("Connection closed.");

	// Close the connection.
	close(connectionSocket);

	return done;
}

// Send a message to the daemon process.
//
// message:	The message to send.
//
static void SendMessageToDaemon(char const* message)
{
	// Create a sending socket.
	auto const sendingSocket = socket(AF_UNIX, SOCK_STREAM, 0);

	if (sendingSocket < 0)
	{
		std::printf("Failed to create sending socket.\n");
		return;
	}

	sockaddr_un sendingAddress;
	{
		sendingAddress.sun_family = AF_UNIX;
		std::strncpy(sendingAddress.sun_path, SANDMAN_TEMP_DIR "sandman.sock",
						 sizeof(sendingAddress.sun_path) - 1);
	}

	// Attempt to connect to the daemon.
	if (connect(sendingSocket, reinterpret_cast<sockaddr*>(&sendingAddress),
					sizeof(sockaddr_un)) < 0)
	{
		std::printf("Failed to connect to the daemon.\n");
		close(sendingSocket);
		return;
	}

	// Send the message.
	if (send(sendingSocket, message, std::strlen(message), 0) < 0)
	{
		std::printf("Failed to send \"%s\" message to the daemon.\n", message);
		close(sendingSocket);
		return;
	}

	std::printf("Sent \"%s\" message to the daemon.\n", message);

	// Close the connection.
	close(sendingSocket);
}

// Handle the commandline arguments.
//
//	arguments:		The argument list.
// argumentCount:	The number of arguments in the list.
//
// Returns:	True if the program should exit, false if it should continue.
//
static bool HandleCommandLine(char** arguments, unsigned int argumentCount)
{
	for (unsigned int argumentIndex = 0; argumentIndex < argumentCount; argumentIndex++)
	{
		auto const* argument = arguments[argumentIndex];

		// Start as a daemon?
		if (std::strcmp(argument, "--daemon") == 0)
		{
			s_programMode = kProgramModeDaemon;
			break;
		}
		// Start as docker?
		else if (std::strcmp(argument, "--docker") == 0)
		{
			s_programMode = kProgramModeDocker;
			break;
		}
		else if (std::strcmp(argument, "--shutdown") == 0)
		{
			SendMessageToDaemon("shutdown");
			return true;
		}
		else
		{
			// We are going to see if there is a command to send to the daemon.
			static constexpr char const* kCommandPrefix = "--command=";

			auto const* commandStringStart = std::strstr(argument, kCommandPrefix);

			if (commandStringStart != nullptr)
			{
				// Skip the command prefix.
				commandStringStart += std::strlen(kCommandPrefix);

				static constexpr std::size_t kCommandBufferCapacity{ 100u };
				char commandBuffer[kCommandBufferCapacity];

				static_assert(kCommandBufferCapacity >= 1u);

				// Copy only the actual command.
				std::strncpy(commandBuffer, commandStringStart, kCommandBufferCapacity - 1u);
				commandBuffer[kCommandBufferCapacity - 1] = '\0';

				// Replace '_' with ' '.
				auto* currentCharacter = commandBuffer;

				while (*currentCharacter != '\0')
				{
					if (*currentCharacter == '_')
					{
						*currentCharacter = ' ';
					}

					currentCharacter++;
				}

				// Send the command to the daemon.
				SendMessageToDaemon(commandBuffer);
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
		return s_exitCode;
	}

	// Initialization.
	if (Initialize() == false)
	{
		Uninitialize();
		return s_exitCode;
	}

	auto done = false;
	while (done == false)
	{
		// We're gonna track the framerate.
		Time frameStartTime;
		TimerGetCurrent(frameStartTime);

		if (s_programMode == kProgramModeInteractive)
		{
			Shell::Lock const lock;
			done = Shell::InputWindow::ProcessSingleUserKey();
			Shell::CheckResize();
		}

		// Process command.
		CommandProcess();

		// Process controls.
		ControlsProcess();

		// Process the input.
		s_input.Process();

		// Process MQTT.
		MQTTProcess();

		// Process the schedule.
		ScheduleProcess();

		// Process the reports.
		ReportsProcess();

		if (s_programMode == kProgramModeDaemon)
		{
			// Process socket communication.
			done = ProcessSocketCommunication();
		}

		// Get the duration of the frame in nanoseconds.
		Time frameEndTime;
		TimerGetCurrent(frameEndTime);

		float const frameDurationMS = TimerGetElapsedMilliseconds(frameStartTime, frameEndTime);
		auto const frameDurationNS = static_cast<unsigned long>(frameDurationMS * 1.0e6f);

		// If the frame is shorter than the duration corresponding to the desired framerate, sleep the
		// difference off.
		unsigned long const targetFrameDurationNS = 1000000000 / 60;

		if (frameDurationNS < targetFrameDurationNS)
		{
			timespec sleepTime;
			sleepTime.tv_sec = 0;
			sleepTime.tv_nsec = targetFrameDurationNS - frameDurationNS;

			timespec remainingTime;
			nanosleep(&sleepTime, &remainingTime);
		}
	}

	Logger::WriteFormattedLine("Uninitializing.");

	// Cleanup.
	Uninitialize();
	return s_exitCode;
}
