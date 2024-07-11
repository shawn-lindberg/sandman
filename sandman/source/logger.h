/// @file
#pragma once

#include <stdarg.h>

// Types
//

// Functions
//

/// @brief Initialize the logger.
///
/// @param p_LogFileName File name of the log for output.
///
/// @returns `true` if successful, `false` otherwise.
///
bool LoggerInitialize(char const* p_LogFileName);

/// @brief Uninitialize the logger.
///
void LoggerUninitialize();

/// @brief Set whether to echo messages to the screen as well.
///
/// @param p_LogToScreen Whether to echo messages to the screen.
///
void LoggerEchoToScreen(bool p_LogToScreen);

/// @brief Add a message to the log.
///
/// @param p_Format Standard printf format string.
/// @param ... Standard printf arguments.
///
/// @returns `true` if successful, `false` otherwise.
///
bool LoggerAddMessage(char const* p_Format, ...);

/// @brief Add a message to the log (`va_list` version).
///
/// @param p_Format Standard printf format string.
/// @param p_Arguments Standard printf arguments.
///
/// @returns `true` if successful, `false` otherwise.
///
bool LoggerAddMessage(char const* p_Format, va_list& p_Arguments);

