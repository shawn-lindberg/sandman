#include "logger.h"

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
	if (fopen_s(&s_LogFile, p_LogFileName, "w") != 0)
	{
		return false;
	}

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

	// Put the date in the buffer.
	if (_strdate_s(l_RemainingBuffer, l_RemainingCapacity) != 0)
	{
		return false;
	}
	
	// Force terminate.
	l_RemainingBuffer[l_RemainingCapacity - 1] = '\0';

	// Update buffer write parameters.
	unsigned int l_CapacityUsed = strlen(l_RemainingBuffer);
	l_RemainingBuffer += l_CapacityUsed;
	l_RemainingCapacity -= l_CapacityUsed;

	// Add a space.
	if (l_RemainingCapacity < 1)
	{
		return false;
	}

	l_RemainingBuffer[0] = ' ';
	l_RemainingBuffer++;
	l_RemainingCapacity--;

	// Put the time in the buffer.
	if (_strtime_s(l_RemainingBuffer, l_RemainingCapacity) != 0)
	{
		return false;
	}
	
	// Force terminate.
	l_RemainingBuffer[l_RemainingCapacity - 1] = '\0';

	// Update buffer write parameters.
	l_CapacityUsed = strlen(l_RemainingBuffer);
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

	if (vsprintf_s(l_RemainingBuffer, l_RemainingCapacity, p_Format, l_Arguments) < 0)
	{
		return false;
	}

	va_end(l_Arguments);

	// Force terminate.
	l_RemainingBuffer[l_RemainingCapacity - 1] = '\0';

	// Print to standard output (and add a newline).
	puts(l_LogStringBuffer);

	// Print to log file.
	if (s_LogFile != NULL)
	{
		fputs(l_LogStringBuffer, s_LogFile);

		// fputs doesn't add a newline, do it now.
		fputs("\n", s_LogFile);
	}

	return true;
}

