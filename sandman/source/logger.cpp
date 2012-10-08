#include "logger.h"

#if defined (__linux__)
	#include <ncurses.h>
#endif // defined (__linux__)

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// Locals
//

// The file to log messages to.
static FILE* s_LogFile = NULL;

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
	// Initialize the file.
	s_LogFile = NULL;

	if (p_LogFileName == NULL)
	{
		return false;
	}

	// Try to open (and destroy old log).
	s_LogFile = fopen(p_LogFileName, "w");

	if (s_LogFile == NULL)
	{
		return false;
	}

	return true;
}

// Uninitialize the logger.
//
void LoggerUninitialize()
{
	// Close the file.
	if (s_LogFile != NULL)
	{
		fclose(s_LogFile);
	}

	s_LogFile = NULL;
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
	unsigned int const l_LogStringBufferCapacity = 2048;
	char l_LogStringBuffer[l_LogStringBufferCapacity];

	// Initialize buffer write parameters.
	unsigned int l_RemainingCapacity = l_LogStringBufferCapacity;
	char* l_RemainingBuffer = l_LogStringBuffer;

	// Get the time.
	time_t l_RawTime = time(NULL);
	tm* l_LocalTime = localtime(&l_RawTime);

	// Put the date and time in the buffer in 2012/09/23 17:44:05 CDT format.
	strftime(l_RemainingBuffer, l_RemainingCapacity, "%Y/%m/%d %H:%M:%S %Z", l_LocalTime);
	
	// Force terminate.
	l_RemainingBuffer[l_RemainingCapacity - 1] = '\0';

	// Update buffer write parameters.
	unsigned int l_CapacityUsed = strlen(l_RemainingBuffer);
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
	va_list l_Arguments;
	va_start(l_Arguments, p_Format);

	if (vsnprintf(l_RemainingBuffer, l_RemainingCapacity, p_Format, l_Arguments) < 0)
	{
		return false;
	}

	va_end(l_Arguments);

	// Force terminate.
	l_RemainingBuffer[l_RemainingCapacity - 1] = '\0';

	// Print to standard output (and add a newline).
	#if defined (_WIN32)

		puts(l_LogStringBuffer);

	#elif defined (__linux__)

		addstr(l_LogStringBuffer);
		refresh();

	#endif // defined (_WIN32)

	// Print to log file.
	if (s_LogFile != NULL)
	{
		fputs(l_LogStringBuffer, s_LogFile);

		// fputs doesn't add a newline, do it now.
		fputs("\n", s_LogFile);
	}

	return true;
}

