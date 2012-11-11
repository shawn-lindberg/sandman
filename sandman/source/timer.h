#pragma once

#include <stdint.h>

// Types
//

// Represents a point in time useful for elapsed time.
struct Time
{
	// The portion of the time in seconds.
	uint64_t m_Seconds;
	
	// The portion of the time in nanoseconds.
	uint64_t m_Nanoseconds;	
};

// Functions
//

// Get the current time.
//
// p_Time:	(Output) The current time.
//
void TimerGetCurrent(Time& p_Time);

// Get the elapsed time in milliseconds between to times.
//
// p_StartTime:	Start time.
// p_EndTime:	End time.
//
float TimerGetElapsedMilliseconds(Time const& p_StartTime, Time const& p_EndTime);
