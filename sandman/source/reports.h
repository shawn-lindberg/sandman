#pragma once

#include <string.h>

#include "control.h"

// Types
//

// Functions
//

// Initialize the report system.
//
// baseDirectory:	The base directory for files.
//
void ReportsInitialize(std::string const& baseDirectory);

// Uninitialize the report system.
//
void ReportsUninitialize();

// Process the reports.
//
void ReportsProcess();

// Add an item to the report corresponding to a control event.
// 
// controlName:	The name of the control.
// action:		The action performed on the control.
// sourceName:	An identifier for where this item comes from.
// 
void ReportsAddControlItem(std::string const& controlName, Control::Actions const action, 
									std::string const& sourceName);

// Add an item to the report corresponding to a schedule event.
// 
// actionName:	The name of the schedule action.
// 
void ReportsAddScheduleItem(std::string const& actionName);

// Add an item to the report corresponding to a status event.
// 
void ReportsAddStatusItem();