#pragma once

#include <stdarg.h>

// Types
//

// Functions
//

// Initialize the report system.
//
void ReportsInitialize();

// Uninitialize the report system.
//
void ReportsUninitialize();

// Process the reports.
//
void ReportsProcess();

// Add an item to the report.
//
// p_Format:	Standard printf format string.
// ...:			Standard printf arguments.
//
// returns:		True if successful, false otherwise.
//
bool ReportsAddItem(char const* p_Format, ...);

// Add an item to the report (va_list version).
//
// p_Format:		Standard printf format string.
// p_Arguments:	Standard printf arguments.
//
// returns:		True if successful, false otherwise.
//
bool ReportsAddItem(char const* p_Format, va_list& p_Arguments);