#pragma once

#include <stdint.h>

// Types
//

// Represents a point in time useful for elapsed time.
struct Time
{
	// The portion of the time in seconds.
	uint64_t m_Seconds = 0;
	
	// The portion of the time in nanoseconds.
	uint64_t m_Nanoseconds = 0;	

	friend auto operator<(const Time& p_Left, const Time& p_Right)
	{
		if (p_Left.m_Seconds != p_Right.m_Seconds)
		{
			return p_Left.m_Seconds < p_Right.m_Seconds;
		}

		return p_Left.m_Nanoseconds < p_Right.m_Nanoseconds;
	}

	friend auto operator>(const Time& p_Left, const Time& p_Right)
	{
		return p_Right < p_Left;
	}

};

// Functions
//

// Get the current time.
//
// p_Time:	(Output) The current time.
//
void TimerGetCurrent(Time& p_Time);

// Get the elapsed time in milliseconds between two times.
// Note: Will return -1 if the end time is less than the start time.
//
// p_StartTime:	Start time.
// p_EndTime:		End time.
//
float TimerGetElapsedMilliseconds(Time const& p_StartTime, Time const& p_EndTime);
