#pragma once

#include <stdarg.h>

// Types
//

// Functions
//

// Initialize the logger.
//
// p_LogFileName:	File name of the log for output.
//
// returns:		True if successful, false otherwise.
//
bool LoggerInitialize(char const* p_LogFileName);

// Uninitialize the logger.
//
void LoggerUninitialize();

// Set whether to echo messages to the screen as well.
//
// p_LogToScreen:	Whether to echo messages to the screen.
//
void LoggerEchoToScreen(bool p_LogToScreen);

// Add a message to the log.
//
// p_Format:	Standard printf format string.
// ...:			Standard printf arguments.
//
// returns:		True if successful, false otherwise.
//
bool LoggerAddMessage(char const* p_Format, ...);

// Add a message to the log (va_list version).
//
// p_Format:		Standard printf format string.
// p_Arguments:	Standard printf arguments.
//
// returns:		True if successful, false otherwise.
//
bool LoggerAddMessage(char const* p_Format, va_list& p_Arguments);

