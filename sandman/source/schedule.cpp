#include "schedule.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"

#include "control.h"
#include "logger.h"
#include "notification.h"
#include "reports.h"
#include "timer.h"

#define DATADIR	AM_DATADIR
#define CONFIGDIR	AM_CONFIGDIR

// Constants
//


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

// Locals
//

// Whether the system is initialized.
static bool s_ScheduleInitialized = false;

// This will contain the schedule once it has been parsed in.
static std::vector<ScheduleEvent> s_ScheduleEvents;

// The current spot in the schedule.
static unsigned int s_ScheduleIndex = UINT_MAX;

// The time the delay for the next event began.
static Time s_ScheduleDelayStartTime;

// Functions
//


// ScheduleEvent members

// Read a schedule event from JSON. 
//	
// p_Object:	The JSON object representing the event.
//
// Returns:		True if the event was read successfully, false otherwise.
//
bool ScheduleEvent::ReadFromJSON(rapidjson::Value::ConstObject const& p_Object)
{
	// We must have a delay.
	auto const l_DelayIterator = p_Object.FindMember("delaySec");

	if (l_DelayIterator == p_Object.MemberEnd())
	{
		return false;
	}

	if (l_DelayIterator->value.IsInt() == false)
	{
		return false;
	}

	m_DelaySec = l_DelayIterator->value.GetInt();

	// We must also have a control action.
	auto const l_ControlActionIterator = p_Object.FindMember("controlAction");

	if (l_ControlActionIterator == p_Object.MemberEnd())
	{
		return false;
	}

	if (l_ControlActionIterator->value.IsObject() == false)
	{
		return false;
	}
	
	if (m_ControlAction.ReadFromJSON(l_ControlActionIterator->value.GetObject()) == false) 
	{
		return false;
	}
	
	return true;
}
		
// Functions
//

// Load the schedule from a file.
// 
static bool ScheduleLoad()
{
	// Open the schedule file.
	static auto const* const s_ScheduleFileName = CONFIGDIR "sandman.sched";

	auto* l_ScheduleFile = fopen(s_ScheduleFileName, "r");

	if (l_ScheduleFile == nullptr)
	{
		LoggerAddMessage("Failed to open the schedule file.\n");
		return false;
	}

	static constexpr unsigned int l_ReadBufferCapacity = 65536;
	char l_ReadBuffer[l_ReadBufferCapacity];
	rapidjson::FileReadStream l_ScheduleFileStream(l_ScheduleFile, l_ReadBuffer, 
		sizeof(l_ReadBuffer));

	rapidjson::Document l_ScheduleDocument;
	l_ScheduleDocument.ParseStream(l_ScheduleFileStream);

	if (l_ScheduleDocument.HasParseError() == true)
	{
		LoggerAddMessage("Failed to parse the schedule file.\n");
		fclose(l_ScheduleFile);
		return false;
	}

	// Find the list of events.
	auto const l_EventsIterator = l_ScheduleDocument.FindMember("events");

	if (l_EventsIterator == l_ScheduleDocument.MemberEnd())
	{
		fclose(l_ScheduleFile);
		return false;		
	}

	if (l_EventsIterator->value.IsArray() == false)
	{
		fclose(l_ScheduleFile);
		return false;
	}

	// Try to load each event in turn.
	for (auto const& l_EventObject : l_EventsIterator->value.GetArray())
	{
		if (l_EventObject.IsObject() == false)
		{
			continue;
		}

		// Try to read the event.
		ScheduleEvent l_Event;
		if (l_Event.ReadFromJSON(l_EventObject.GetObject()) == false)
		{
			continue;
		}
					
		// If we successfully read the event, add it to the list.
		s_ScheduleEvents.push_back(l_Event);
	}
							
	fclose(l_ScheduleFile);
	return true;
}

// Write the loaded schedule to the logger.
//
static void ScheduleLogLoaded()
{
	// Now write out the schedule.
	LoggerAddMessage("The following schedule is loaded:");
	
	if (s_ScheduleEvents.size() == 0)
	{
		LoggerAddMessage("\t<empty>");
		LoggerAddMessage("");
		return;
	}

	for (auto const& l_Event : s_ScheduleEvents)
	{
		// Split the delay into multiple units.
		auto l_DelaySec = l_Event.m_DelaySec;
		
		auto const l_DelayHours = l_DelaySec / 3600;
		l_DelaySec %= 3600;
		
		auto const l_DelayMin = l_DelaySec / 60;
		l_DelaySec %= 60;
		
		auto const* l_ActionText = (l_Event.m_ControlAction.m_Action == Control::ACTION_MOVING_UP) ? 
			"up" : "down";
			
		// Print the event.
		LoggerAddMessage("\t+%01ih %02im %02is -> %s, %s", l_DelayHours, l_DelayMin, 
			l_DelaySec, l_Event.m_ControlAction.m_ControlName, l_ActionText);
	}
	
	LoggerAddMessage("");
}

// Initialize the schedule.
//
void ScheduleInitialize()
{	
	s_ScheduleIndex = UINT_MAX;
	
	s_ScheduleEvents.clear();
	
	LoggerAddMessage("Initializing the schedule...");

	// Parse the schedule.
	if (ScheduleLoad() == false)
	{
		LoggerAddMessage("\tfailed");
		return;
	}
	
	LoggerAddMessage("\tsucceeded");
	LoggerAddMessage("");
	
	// Log the schedule that just got loaded.
	ScheduleLogLoaded();
	
	s_ScheduleInitialized = true;
}

// Uninitialize the schedule.
// 
void ScheduleUninitialize()
{
	if (s_ScheduleInitialized == false)
	{
		return;
	}
	
	s_ScheduleInitialized = false;
	
	s_ScheduleEvents.clear();
}

// Start the schedule.
//
void ScheduleStart()
{
	// Add the report item prior to checks, because we want to record the intent.
	ReportsAddScheduleItem("start");

	// Make sure it's initialized.
	if (s_ScheduleInitialized == false)
	{
		return;
	}
	
	// Make sure it's not running.
	if (ScheduleIsRunning() == true)
	{
		return;
	}
	
	s_ScheduleIndex = 0;
	TimerGetCurrent(s_ScheduleDelayStartTime);
	
	// Notify.
	NotificationPlay("schedule_start");
	
	LoggerAddMessage("Schedule started.");
}

// Stop the schedule.
//
void ScheduleStop()
{
	// Add the report item prior to checks, because we want to record the intent.
	ReportsAddScheduleItem("stop");

	// Make sure it's initialized.
	if (s_ScheduleInitialized == false)
	{
		return;
	}
	
	// Make sure it's running.
	if (ScheduleIsRunning() == false)
	{
		return;
	}
	
	s_ScheduleIndex = UINT_MAX;
	
	// Notify.
	NotificationPlay("schedule_stop");
	
	LoggerAddMessage("Schedule stopped.");
}

// Determine whether the schedule is running.
//
bool ScheduleIsRunning()
{
	return (s_ScheduleIndex != UINT_MAX);
}

// Process the schedule.
//
void ScheduleProcess()
{
	// Make sure it's initialized.
	if (s_ScheduleInitialized == false)
	{
		return;
	}
	
	// Running?
	if (ScheduleIsRunning() == false)
	{
		return;
	}

	// No need to do anything for schedules with zero events.
	auto const l_ScheduleEventCount = static_cast<unsigned int>(s_ScheduleEvents.size());
	if (l_ScheduleEventCount == 0)
	{
		return;
	}

	// Get elapsed time since delay start.
	Time l_CurrentTime;
	TimerGetCurrent(l_CurrentTime);

	auto const l_ElapsedTimeSec = 
		TimerGetElapsedMilliseconds(s_ScheduleDelayStartTime, l_CurrentTime) / 1000.0f;

	// Time up?
	auto& l_Event = s_ScheduleEvents[s_ScheduleIndex];
	
	if (l_ElapsedTimeSec < l_Event.m_DelaySec)
	{
		return;
	}
	
	// Move to the next event.
	s_ScheduleIndex = (s_ScheduleIndex + 1) % l_ScheduleEventCount;
	
	// Set the new delay start time.
	TimerGetCurrent(s_ScheduleDelayStartTime);
	
	// Sanity check the event.
	if (l_Event.m_ControlAction.m_Action >= Control::NUM_ACTIONS)
	{
		LoggerAddMessage("Schedule moving to event %i.", s_ScheduleIndex);
		return;
	}

	// Try to find the control to perform the action.
	auto* l_Control = l_Event.m_ControlAction.GetControl();
	
	if (l_Control == nullptr) {
		
		LoggerAddMessage("Schedule couldn't find control \"%s\". Moving to event %i.", 
			l_Event.m_ControlAction.m_ControlName, s_ScheduleIndex);
		return;
	}
		
	// Perform the action.
	l_Control->SetDesiredAction(l_Event.m_ControlAction.m_Action, Control::MODE_TIMED);
	
	ReportsAddControlItem(l_Control->GetName(), l_Event.m_ControlAction.m_Action, "schedule");

	LoggerAddMessage("Schedule moving to event %i.", s_ScheduleIndex);
}
