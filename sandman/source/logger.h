#pragma once

// Types
//

// Functions
//

// Initialize the logger.
//
// p_LogFileName:	File name of the log for output.
// p_LogToScreen:	Whether to echo messages to the screen.
//
// returns:		True if successful, false otherwise.
//
bool LoggerInitialize(char const* p_LogFileName, bool p_LogToScreen);

// Uninitialize the logger.
//
void LoggerUninitialize();

// Add a message to the log.
//
// p_Format:	Standard printf format string.
// ...:			Standard printf arguments.
//
// returns:		True if successful, false otherwise.
//
bool LoggerAddMessage(char const* p_Format, ...);
