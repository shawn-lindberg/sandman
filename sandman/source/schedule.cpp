#include "schedule.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "control.h"
#include "logger.h"
#include "sound.h"
#include "timer.h"

#define DATADIR		AM_DATADIR
#define CONFIGDIR	AM_CONFIGDIR

// Constants
//


// Types
//

// A schedule event.
struct ScheduleEvent
{

	// Constants.
	enum {
		
		CONTROL_NAME_CAPACITY = 32,
	};
	
	// Delay in seconds before this entry occurs (since the last).
	unsigned int		m_DelaySec;
	
	// The control to manipulate.
	char				m_ControlName[CONTROL_NAME_CAPACITY];
	
	// The action for the control.
	Control::Actions	m_Action;
};

// Locals
//

// This will contain the schedule once it has been parsed in.
static ScheduleEvent* s_ScheduleEvents = NULL;
static unsigned int s_ScheduleMaxEvents = 0;
static unsigned int s_ScheduleNumEvents = 0;

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

template<class T>
T const& Min(T const& p_A, T const& p_B)
{
	return (p_A < p_B) ? p_A : p_B;
}

// Skip whitespace.
// 
// p_InputString:	The string whose whitespace will be skipped.
// 
// returns:			A pointer to the next non-whitespace character.
// 
static char* SkipWhitespace(char* p_InputString)
{
	char* l_OutputString = p_InputString;
	
	while (isspace(*l_OutputString) != 0)
	{
		l_OutputString++;
	}
	
	return l_OutputString;
}
		
// Load the schedule from a file.
// 
static bool ScheduleLoad()
{
	// Open the schedule file.
	FILE* l_ScheduleFile = fopen(CONFIGDIR "sandman.sched", "r");
	
	if (l_ScheduleFile == NULL)
	{
		return false;
	}
	
	// First, we have to count the number of actions in the schedule.
	s_ScheduleMaxEvents = 0;
	bool l_HaveSeenStart = false;
	
	// Read each line in turn.
	unsigned int const l_LineBufferCapacity = 128;
	char l_LineBuffer[l_LineBufferCapacity];
	
	fpos_t l_FirstActionFilePosition;
	
	while (fgets(l_LineBuffer, l_LineBufferCapacity, l_ScheduleFile) != NULL)
	{
		// Skip comments.
		if (l_LineBuffer[0] == '#')
		{
			continue;
		}
	
		// Skip whitespace.
		char* l_LineText = SkipWhitespace(l_LineBuffer);
		
		// Until we see the start command, don't begin counting.
		if (l_HaveSeenStart == false) 
		{
			static char const* s_StartText = "start";
			if (strncmp(l_LineText, s_StartText, strlen(s_StartText)) == 0)
			{
				l_HaveSeenStart = true;
				
				// Save the file position so we can re-parse from here.
				fgetpos(l_ScheduleFile, &l_FirstActionFilePosition);
			}
			
			continue;
		}

		// Once we see the end command, stop counting.
		static char const* s_EndText = "end";
		if (strncmp(l_LineText, s_EndText, strlen(s_EndText)) == 0)
		{
			break;
		}
		
		// Count the line.
		s_ScheduleMaxEvents++;
	}
	
	// Now we can allocate memory for the schedule.
	size_t l_AllocationSize = s_ScheduleMaxEvents * sizeof(ScheduleEvent);
	s_ScheduleEvents = reinterpret_cast<ScheduleEvent*>(malloc(l_AllocationSize));
	
	if (s_ScheduleEvents == NULL)
	{
		// Close the file.
		fclose(l_ScheduleFile);
		return false;
	}
	
	// Now we parse in the actual events.
	s_ScheduleNumEvents = 0;
	
	// Jump back to the first action.
	if (fsetpos(l_ScheduleFile, &l_FirstActionFilePosition) < 0)
	{
		// Close the file.
		fclose(l_ScheduleFile);
		return false;
	}
	
	while (fgets(l_LineBuffer, l_LineBufferCapacity, l_ScheduleFile) != NULL)
	{
		// Skip comments.
		if (l_LineBuffer[0] == '#')
		{
			continue;
		}
	
		// Skip whitespace.
		char* l_LineText = SkipWhitespace(l_LineBuffer);
		
		// Once we see the end command, stop.
		static char const* s_EndText = "end";
		if (strncmp(l_LineText, s_EndText, strlen(s_EndText)) == 0)
		{
			break;
		}

		// The delay is followed by a comma.
		char* l_Separator = strchr(l_LineText, ',');
		
		if (l_Separator == NULL)
		{
			continue;
		}
		
		// For now, modify the string to split it.
		char const* l_DelayString = l_LineText;
		l_LineText = l_Separator + 1;
		(*l_Separator) = '\0';
		
		// Skip whitespace.
		l_LineText = SkipWhitespace(l_LineText);
		
		// The control name is also followed by a comma.
		l_Separator = strchr(l_LineText, ',');
		
		if (l_Separator == NULL)
		{
			continue;
		}
		
		// For now, modify the string to split it.
		char const* l_ControlNameString = l_LineText;
		l_LineText = l_Separator + 1;
		(*l_Separator) = '\0';
		
		// Skip whitespace.
		l_LineText = SkipWhitespace(l_LineText);
		
		// There should be nothing after the direction.
		
		ScheduleEvent& l_NewEvent = s_ScheduleEvents[s_ScheduleNumEvents];
		
		// Parse the delay.
		l_NewEvent.m_DelaySec = atoi(l_DelayString);
		
		// Copy the control name.
		unsigned int const l_ControlNameStringLength = strlen(l_ControlNameString);
		unsigned int const l_AmountToCopy = 
				Min(static_cast<unsigned int>(ScheduleEvent::CONTROL_NAME_CAPACITY) - 1,
					l_ControlNameStringLength);
		strncpy(l_NewEvent.m_ControlName, l_ControlNameString, l_AmountToCopy);
		l_NewEvent.m_ControlName[l_AmountToCopy] = '\0';
		
		// Parse the direction.
		l_NewEvent.m_Action = Control::ACTION_STOPPED;
		
		char const* const l_UpString = "up";
		char const* const l_DownString = "down";
		
		if (strncmp(l_LineText, l_UpString, strlen(l_UpString)) == 0)
		{
			l_NewEvent.m_Action = Control::ACTION_MOVING_UP;
		}
		else if (strncmp(l_LineText, l_DownString, strlen(l_DownString)) == 0)
		{
			l_NewEvent.m_Action = Control::ACTION_MOVING_DOWN;
		}
		
		// Validate it.
		if (l_NewEvent.m_Action == Control::ACTION_STOPPED)
		{
			LoggerAddMessage("\"%s\" is not a valid control direction.  This entry will be ignored.", 
				l_LineText);
			continue;
		}
		
		// Keep it.
		s_ScheduleNumEvents++;
	}
	
	// Close the file.
	fclose(l_ScheduleFile);
		
	return true;
}

// Write the loaded schedule to the logger.
//
static void ScheduleLogLoaded()
{
	if (s_ScheduleEvents == NULL)
	{
		return;
	}
	
	// Now write out the schedule.
	LoggerAddMessage("The following schedule is loaded:");
	
	for (unsigned int l_EventIndex = 0; l_EventIndex < s_ScheduleNumEvents; l_EventIndex++)
	{
		ScheduleEvent const& l_Event = s_ScheduleEvents[l_EventIndex];
		
		// Split the delay into multiple units.
		unsigned int l_DelaySec = l_Event.m_DelaySec;
		
		unsigned int const l_DelayHours = l_DelaySec / 3600;
		l_DelaySec %= 3600;
		
		unsigned int const l_DelayMin = l_DelaySec / 60;
		l_DelaySec %= 60;
		
		char const* const l_UpString = "up";
		char const* const l_DownString = "down";

		char const* l_ActionString = (l_Event.m_Action == Control::ACTION_MOVING_UP) ? 
			l_UpString : l_DownString;
			
		// Print the event.
		LoggerAddMessage("\t+%01ih %02im %02is - %s -> %s", l_DelayHours, l_DelayMin, 
			l_DelaySec, l_Event.m_ControlName, l_ActionString);
	}
	
	LoggerAddMessage("");
}

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
	
	s_ScheduleEvents = NULL;
	s_ScheduleMaxEvents = 0;
	s_ScheduleNumEvents = 0;
	
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
}

// Uninitialize the schedule.
// 
void ScheduleUninitialize()
{
	if (s_ScheduleEvents != NULL)
	{
		free(s_ScheduleEvents);
		s_ScheduleEvents = NULL;
	}
}

// Start the schedule.
//
void ScheduleStart()
{
	// Make sure it's initialized.
	if (s_ScheduleEvents == NULL)
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
	if (s_ScheduleEvents == NULL)
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
	if (s_ScheduleEvents == NULL)
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

	float l_ElapsedTimeSec = TimerGetElapsedMilliseconds(s_ScheduleDelayStartTime, l_CurrentTime) / 1000.0f;

	// Time up?
	ScheduleEvent const& l_Event = s_ScheduleEvents[s_ScheduleIndex];
	
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
