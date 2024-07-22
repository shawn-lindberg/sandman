#include "schedule.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rapidjson/filereadstream.h"
#include "logger.h"
#include "notification.h"
#include "reports.h"
#include "timer.h"


// Constants
//


// Types
//

// Locals
//

// Whether the system is initialized.
static bool s_ScheduleInitialized = false;

// The current spot in the schedule.
static unsigned int s_ScheduleIndex = UINT_MAX;

// The time the delay for the next event began.
static Time s_ScheduleDelayStartTime;

static Schedule s_Schedule;

// Functions
//

// ScheduleEvent members

// Read a schedule event from JSON. 
//	
// p_Object:	The JSON object representing the event.
//
// Returns:		True if the event was read successfully, false otherwise.
//
bool ScheduleEvent::ReadFromJSON(rapidjson::Value const& p_Object)
{
	if (p_Object.IsObject() == false)
	{
		LoggerAddMessage("Schedule event could not be parsed because it is not an object.");
		return false;
	}

	// We must have a delay.
	auto const l_DelayIterator = p_Object.FindMember("delaySec");

	if (l_DelayIterator == p_Object.MemberEnd())
	{
		LoggerAddMessage("Schedule event is missing the delay time.");
		return false;
	}

	if (l_DelayIterator->value.IsInt() == false)
	{
		LoggerAddMessage("Schedule event has a delay time, but it's not an integer.");
		return false;
	}

	m_DelaySec = l_DelayIterator->value.GetInt();

	// We must also have a control action.
	auto const l_ControlActionIterator = p_Object.FindMember("controlAction");

	if (l_ControlActionIterator == p_Object.MemberEnd())
	{
		LoggerAddMessage("Schedule event is missing a control action.");
		return false;
	}
	
	if (m_ControlAction.ReadFromJSON(l_ControlActionIterator->value) == false) 
	{
		LoggerAddMessage("Schedule event control action could not be parsed.");
		return false;
	}
	
	return true;
}

// Schedule members

// Load a schedule from a file.
//
// p_FileName: The name of a file describing the schedule.
//
// Returns:    True if the schedule was loaded successfully, false otherwise.
//
bool Schedule::ReadFromFile(const char* p_FileName)
{
	m_Events.clear();

	auto* l_ScheduleFile = fopen(p_FileName, "r");

	if (l_ScheduleFile == nullptr)
	{
		LoggerAddMessage("Failed to open the schedule file %s.\n", p_FileName);
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
		LoggerAddMessage("Failed to parse the schedule file %s.\n", p_FileName);
		fclose(l_ScheduleFile);
		return false;
	}

	// Find the list of events.
	auto const l_EventsIterator = l_ScheduleDocument.FindMember("events");

	if (l_EventsIterator == l_ScheduleDocument.MemberEnd())
	{
		LoggerAddMessage("No schedule events in %s.\n", p_FileName);
		fclose(l_ScheduleFile);
		return false;		
	}

	if (l_EventsIterator->value.IsArray() == false)
	{
		LoggerAddMessage("Schedule has events, but it is not an array.");
		fclose(l_ScheduleFile);
		LoggerAddMessage("No event array in %s.\n", p_FileName);
		return false;
	}

	// Try to load each event in turn.
	for (auto const& l_EventObject : l_EventsIterator->value.GetArray())
	{
		// Try to read the event.
		ScheduleEvent l_Event;
		if (l_Event.ReadFromJSON(l_EventObject) == false)
		{
			continue;
		}
					
		// If we successfully read the event, add it to the list.
		m_Events.push_back(l_Event);
	}
							
	fclose(l_ScheduleFile);
	return true;
}

// Determines whether the schedule is empty.
//
bool Schedule::IsEmpty() const
{
	return (m_Events.size() == 0);
}

// Gets the number of events in the schedule.
//
size_t Schedule::GetNumEvents() const
{
	return m_Events.size();
}

// Get the events in the schedule.
// NOTE: This is intended to be const, however current ControlActions prevent that.
//
std::vector<ScheduleEvent>& Schedule::GetEvents()
{
	return m_Events;
}

// Functions
//

// Load the schedule from a file.
// 
static bool ScheduleLoad()
{
	// Open the schedule file.
	static auto const* const s_ScheduleFileName = SANDMAN_CONFIG_DIR "sandman.sched";

	return s_Schedule.ReadFromFile(s_ScheduleFileName);
}

// Write the loaded schedule to the logger.
//
static void ScheduleLogLoaded()
{
	// Now write out the schedule.
	LoggerAddMessage("The following schedule is loaded:");
	
	if (s_Schedule.IsEmpty())
	{
		LoggerAddMessage("\t<empty>");
		LoggerAddEmptyLine();
		return;
	}

	for (auto const& l_Event : s_Schedule.GetEvents())
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
	
	LoggerAddEmptyLine();
}

// Initialize the schedule.
//
void ScheduleInitialize()
{	
	s_ScheduleIndex = UINT_MAX;
	
	LoggerAddMessage("Initializing the schedule...");

	// Parse the schedule.
	if (ScheduleLoad() == false)
	{
		LoggerAddMessage("\tfailed");
		return;
	}
	
	LoggerAddMessage("\tsucceeded");
	LoggerAddEmptyLine();
	
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
	auto const l_ScheduleEventCount = static_cast<unsigned int>(s_Schedule.GetNumEvents());
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
	auto& l_Event = s_Schedule.GetEvents()[s_ScheduleIndex];
	
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
