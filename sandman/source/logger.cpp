#include "logger.h"

#if defined (__linux__)
	#include <ncurses.h>
#endif // defined (__linux__)

#include <mutex>
#include <stdio.h>
#include <string.h>
#include <time.h>

// Locals
//

// Used to enforce serialization of messages.
std::mutex s_LogMutex;

// The file to log messages to.
static FILE* s_LogFile = nullptr;

// Whether to echo messages to the screen.
static bool s_LogToScreen = false;

// Functions
//

// Initialize the logger.
//
// p_LogFileName:	File name of the log for output.
//
// returns:		True if successful, false otherwise.
//
bool LoggerInitialize(char const* p_LogFileName)
{
	// Acquire a lock for the rest of the function.
	const std::lock_guard<std::mutex> l_LogGuard(s_LogMutex);

	// Initialize the file.
	s_LogFile = nullptr;

	if (p_LogFileName == nullptr)
	{
		return false;
	}

	// Try to open (and destroy old log).
	s_LogFile = fopen(p_LogFileName, "w");

	if (s_LogFile == nullptr)
	{
		return false;
	}

	return true;
}

// Uninitialize the logger.
//
void LoggerUninitialize()
{
	// Acquire a lock for the rest of the function.
	const std::lock_guard<std::mutex> l_LogGuard(s_LogMutex);

	// Close the file.
	if (s_LogFile != nullptr)
	{
		fclose(s_LogFile);
	}

	s_LogFile = nullptr;
}

// Set whether to echo messages to the screen as well.
//
// p_LogToScreen:	Whether to echo messages to the screen.
//
void LoggerEchoToScreen(bool p_LogToScreen)
{
	s_LogToScreen = p_LogToScreen;
}

// Add a message to the log.
//
// p_Format:	Standard printf format string.
// ...:			Standard printf arguments.
//
// returns:		True if successful, false otherwise.
//
bool LoggerAddMessage(char const* p_Format, ...)
{	
	va_list l_Arguments;
	va_start(l_Arguments, p_Format);

	auto const l_Result = LoggerAddMessage(p_Format, l_Arguments);

	va_end(l_Arguments);
	
	return l_Result;
}

// Add a message to the log (va_list version).
//
// p_Format:		Standard printf format string.
// p_Arguments:	Standard printf arguments.
//
// returns:		True if successful, false otherwise.
//
bool LoggerAddMessage(char const* p_Format, va_list& p_Arguments)
{
	static unsigned int const l_LogStringBufferCapacity = 2048;
	char l_LogStringBuffer[l_LogStringBufferCapacity];

	// Initialize buffer write parameters.
	auto l_RemainingCapacity = l_LogStringBufferCapacity;
	auto* l_RemainingBuffer = l_LogStringBuffer;

	// Get the time.
	auto const l_RawTime = time(nullptr);
	auto* l_LocalTime = localtime(&l_RawTime);

	// Put the date and time in the buffer in 2012/09/23 17:44:05 CDT format.
	strftime(l_RemainingBuffer, l_RemainingCapacity, "%Y/%m/%d %H:%M:%S %Z", l_LocalTime);
	
	// Force terminate.
	l_RemainingBuffer[l_RemainingCapacity - 1] = '\0';

	// Update buffer write parameters.
	unsigned int const l_CapacityUsed = strlen(l_RemainingBuffer);
	l_RemainingBuffer += l_CapacityUsed;
	l_RemainingCapacity -= l_CapacityUsed;

	// Add a separator.
	if (l_RemainingCapacity < 2)
	{
		return false;
	}

	l_RemainingBuffer[0] = ']';
	l_RemainingBuffer[1] = ' ';
	l_RemainingBuffer += 2;
	l_RemainingCapacity -= 2;

	// Add the log message.
	if (vsnprintf(l_RemainingBuffer, l_RemainingCapacity, p_Format, p_Arguments) < 0)
	{
		return false;
	}

	// Force terminate.
	l_RemainingBuffer[l_RemainingCapacity - 1] = '\0';

	{
		// Acquire a lock for the rest of the function.
		const std::lock_guard<std::mutex> l_LogGuard(s_LogMutex);

		// Print to standard output (and add a newline).
		if (s_LogToScreen == true)
		{
			#if defined (_WIN32)

				puts(l_LogStringBuffer);

			#elif defined (__linux__)

				addstr(l_LogStringBuffer);
				addch('\n');
				refresh();

			#endif // defined (_WIN32)
		}

		// Print to log file.
		if (s_LogFile != nullptr)
		{
			fputs(l_LogStringBuffer, s_LogFile);

			// fputs doesn't add a newline, do it now.
			fputs("\n", s_LogFile);
			
			fflush(s_LogFile);
		}
	}

	return true;
}


