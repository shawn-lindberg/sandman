#pragma once

#include <cstdarg>
#include <sstream>

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
[[gnu::format(printf, 1, 2)]] bool LoggerAddMessage(char const* p_Format, ...);

// Add a message to the log (va_list version).
//
// p_Format:		Standard printf format string.
// p_Arguments:	Standard printf arguments.
//
// returns:		True if successful, false otherwise.
//
[[gnu::format(printf, 1, 0)]] bool LoggerAddMessage(char const* p_Format,
																	 std::va_list& p_Arguments);

/// @brief Log an empty line. (The logger still prints the date and time.)
/// 
/// @note `LoggerAddMessage("")` triggers a warning `zero-length gnu_printf format string
/// [-Werror=format-zero-length]`; this function accomplishes the same without triggering a warning.
/// 
/// @return `true` if successful, `false` otherwise
///
[[gnu::always_inline]] inline bool LoggerAddEmptyLine()
{
	return LoggerAddMessage("%s", "");
}

/// @brief Log a variable amount of arguments.
/// 
/// @note This function uses an output string stream `std::ostringstream`
/// to convert its arguments to a string which is passed to `LoggerAddMessage`.
/// So, this function can be used as a quick print statement when debugging,
/// or as shorthand for when you would like to make use of `std::ostringstream`
/// to format a string and log it.
/// 
/// @param p_Arguments arguments that are printed using the logger
/// 
/// @return `true` if successful, `false` otherwise
/// 
/// @attention This function constructs and destroys a `std::ostringstream` every call.
///
template <typename... ParamsT>
[[gnu::always_inline]] inline bool LoggerPrintArgs(ParamsT const&... p_Arguments)
{
	return LoggerAddMessage("%s", (std::ostringstream() << ... << p_Arguments).str().c_str());
}
