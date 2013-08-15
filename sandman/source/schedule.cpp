#include "schedule.h"

#include <limits.h>
#include <string.h>

#include "control.h"
#include "logger.h"
#include "sound.h"
#include "timer.h"

#define DATADIR	AM_DATADIR

// Constants
//


// Types
//

// A schedule event.
struct ScheduleEvent
{
	// Delay in seconds before this entry occurs (since the last).
	unsigned int		m_DelaySec;
	
	// The control to manipulate.
	char				m_ControlName[32];
	
	// The action for the control.
	Control::Actions	m_Action;
};

// Locals
//

// Hardcoded schedule for now.
static ScheduleEvent s_Schedule[] =
{
	{ 30,	"knee",	Control::ACTION_MOVING_DOWN	},
	{ 0,	"head",	Control::ACTION_MOVING_UP	},
	{ 45,	"head",	Control::ACTION_MOVING_DOWN	},
	{ 0,	"knee",	Control::ACTION_MOVING_UP	},
};
static unsigned int s_ScheduleNumEvents = sizeof(s_Schedule) / sizeof(ScheduleEvent);

// The array of controls.
static Control* s_Controls = NULL;

// The number of controls.
static unsigned int s_NumControls = 0;

// The current spot in the schedule.
static unsigned int s_ScheduleIndex = UINT_MAX;

// The time the delay for the next event began.
static Time s_ScheduleDelayStartTime;

// Functions
//

// Initialize the schedule.
//
// p_Controls:		The array of controls.
// p_NumControls:	The number of controls.
//
void ScheduleInitialize(Control* p_Controls, unsigned int p_NumControls)
{
	s_Controls = p_Controls;
	s_NumControls = p_NumControls;
	
	s_ScheduleIndex = UINT_MAX;
}

// Start the schedule.
//
void ScheduleStart()
{
	// Make sure it's not running.
	if (s_ScheduleIndex != UINT_MAX)
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
	// Make sure it's running.
	if (s_ScheduleIndex == UINT_MAX)
	{
		return;
	}
	
	s_ScheduleIndex = UINT_MAX;
	
	// Queue the sound.
	SoundAddToQueue(DATADIR "audio/sched_stop.wav");
	
	LoggerAddMessage("Schedule stopped.");
}

// Process the schedule.
//
void ScheduleProcess()
{
	// Running?
	if (s_ScheduleIndex == UINT_MAX)
	{
		return;
	}

	// Get elapsed time since delay start.
	Time l_CurrentTime;
	TimerGetCurrent(l_CurrentTime);

	float l_ElapsedTimeSec = TimerGetElapsedMilliseconds(s_ScheduleDelayStartTime, l_CurrentTime) / 1000.0f;

	// Time up?
	ScheduleEvent const& l_Event = s_Schedule[s_ScheduleIndex];
	
	if (l_ElapsedTimeSec < l_Event.m_DelaySec)
	{
		return;
	}
	
	// Move to the next event.
	s_ScheduleIndex = (s_ScheduleIndex + 1) % s_ScheduleNumEvents;
	
	// Set the new delay start time.
	TimerGetCurrent(s_ScheduleDelayStartTime);
	
	// Sanity check the event.
	if (l_Event.m_Action >= Control::NUM_ACTIONS)
	{
		LoggerAddMessage("Schedule moving to event %i.", s_ScheduleIndex);
		return;
	}

	// Try to find the control to perform the action.
	for (unsigned int l_ControlIndex = 0; l_ControlIndex < s_NumControls; l_ControlIndex++)
	{
		Control& l_Control = s_Controls[l_ControlIndex];
		
		if (strcmp(l_Control.GetName(), l_Event.m_ControlName) != 0)
		{
			continue;
		}
		
		// Perform the action.
		l_Control.SetDesiredAction(l_Event.m_Action);
		break;
	}
	
	LoggerAddMessage("Schedule moving to event %i.", s_ScheduleIndex);
}
