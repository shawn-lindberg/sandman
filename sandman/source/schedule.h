#pragma once

#include <vector>

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "control.h"

// Types
//

// A schedule event.
struct ScheduleEvent
{
	// Read a schedule event from JSON. 
	//
	// p_EventObject:	The JSON object representing the event.
	//	
	// Returns:		True if the event was read successfully, false otherwise.
	//
	bool ReadFromJSON(rapidjson::Value::ConstObject const& p_EventObject);
	
	// Delay in seconds before this entry occurs (since the last).
	unsigned int	m_DelaySec;
	
	// The control action to perform at the scheduled time.
	ControlAction	m_ControlAction;
};


// Functions
//

// Initialize the schedule.
//
void ScheduleInitialize();

// Uninitialize the schedule.
// 
void ScheduleUninitialize();

// Start the schedule.
//
void ScheduleStart();

// Stop the schedule.
//
void ScheduleStop();

// Determine whether the schedule is running.
//
bool ScheduleIsRunning();

// Process the schedule.
//
void ScheduleProcess();

