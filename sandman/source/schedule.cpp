#include "schedule.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "control.h"
#include "logger.h"
#include "sound.h"
#include "timer.h"
#include "xml.h"

#define DATADIR	AM_DATADIR
#define CONFIGDIR	AM_CONFIGDIR

// Constants
//


// Types
//

// A schedule event.
struct ScheduleEvent
{
	// Read a schedule event from XML. 
	//
	// p_Document:	The XML document that the node belongs to.
	// p_Node:		The XML node to read the event from.
	//	
	// Returns:		True if the event was read successfully, false otherwise.
	//
	bool ReadFromXML(xmlDocPtr p_Document, xmlNodePtr p_Node);
	
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

template<class T>
T const& Min(T const& p_A, T const& p_B)
{
	return (p_A < p_B) ? p_A : p_B;
}

// ScheduleEvent members

// Read a schedule event from XML. 
//
// p_Document:	The XML document that the node belongs to.
// p_Node:		The XML node to read the event from.
//	
// Returns:		True if the event was read successfully, false otherwise.
//
bool ScheduleEvent::ReadFromXML(xmlDocPtr p_Document, xmlNodePtr p_Node)
{
	// We must have a delay.
	static auto const* s_DelayNodeName = "DelaySec";
	auto* l_DelayNode = XMLFindNextNodeByName(p_Node->xmlChildrenNode, s_DelayNodeName);
	
	if (l_DelayNode == nullptr) 
	{
		return false;
	}
	
	// Read the delay from the node.
	m_DelaySec = XMLGetNodeTextAsInteger(p_Document, l_DelayNode);
	
	// We must also have a control action.
	static auto const* s_ControlActionNodeName = "ControlAction";
	auto* l_ControlActionNode = XMLFindNextNodeByName(p_Node->xmlChildrenNode, s_ControlActionNodeName);
	
	if (l_ControlActionNode == nullptr)
	{
		return false;
	}
	
	// Read the control action from the node.
	if (m_ControlAction.ReadFromXML(p_Document, l_ControlActionNode) == false) 
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
			
	auto* l_ScheduleDocument = xmlParseFile(s_ScheduleFileName);
	
	if (l_ScheduleDocument == nullptr) {		
		
		LoggerAddMessage("Failed to open the schedule file.\n");
		return false;
	}
	
	// Get the root element of the tree.
	auto const* l_RootNode = xmlDocGetRootElement(l_ScheduleDocument);
	
	if (l_RootNode == nullptr) {
		
		xmlFreeDoc(l_ScheduleDocument);
		LoggerAddMessage("Failed to get the root node of the schedule file. Maybe it is corrupt?\n");
		return false;
	}
	
	// Loop through all of the event nodes.
	static auto const* s_EventNodeName = "Event";
	XMLForEachNodeNamed(l_RootNode->xmlChildrenNode, s_EventNodeName, [&](xmlNodePtr p_Node)
	{								
		// Try to read the event.
		ScheduleEvent l_Event;
		if (l_Event.ReadFromXML(l_ScheduleDocument, p_Node) == false)
		{
			return;
		}
					
		// If we successfully read the event, add it to the list.
		s_ScheduleEvents.push_back(l_Event);
	});
	
	// Close the file.
	xmlFreeDoc(l_ScheduleDocument);
		
	return true;
}

// Write the loaded schedule to the logger.
//
static void ScheduleLogLoaded()
{
	// Now write out the schedule.
	LoggerAddMessage("The following schedule is loaded:");
	
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
	
	// Queue the sound.
	SoundAddToQueue(DATADIR "audio/sched_start.wav");
	
	LoggerAddMessage("Schedule started.");
}

// Stop the schedule.
//
void ScheduleStop()
{
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
	
	// Queue the sound.
	SoundAddToQueue(DATADIR "audio/sched_stop.wav");
	
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

	// Get elapsed time since delay start.
	Time l_CurrentTime;
	TimerGetCurrent(l_CurrentTime);

	auto const l_ElapsedTimeSec = TimerGetElapsedMilliseconds(s_ScheduleDelayStartTime, l_CurrentTime) / 
		1000.0f;

	// Time up?
	auto& l_Event = s_ScheduleEvents[s_ScheduleIndex];
	
	if (l_ElapsedTimeSec < l_Event.m_DelaySec)
	{
		return;
	}
	
	// Move to the next event.
	auto const l_ScheduleEventCount = static_cast<unsigned int>(s_ScheduleEvents.size());
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
	
	LoggerAddMessage("Schedule moving to event %i.", s_ScheduleIndex);
}
