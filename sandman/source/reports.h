#pragma once

#include <string.h>

#include "control.h"

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

// Add an item to the report corresponding to a control event.
// 
// p_ControlName:	The name of the control.
// p_Action:		The action performed on the control.
// p_SourceName:	An identifier for where this item comes from.
// 
void ReportsAddControlItem(std::string const& p_ControlName, Control::Actions const p_Action, 
	std::string const& p_SourceName);

// Add an item to the report corresponding to a schedule event.
// 
// p_ActionName:	The name of the schedule action.
// 
void ReportsAddScheduleItem(std::string const& p_ActionName);

// Add an item to the report corresponding to a status event.
// 
void ReportsAddStatusItem();