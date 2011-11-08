#include "timer.h"

#include <Windows.h>

// Functions
//

// Get the current time in ticks.
//
__int64 TimerGetTicks()
{
	// NOTE - STL 2011/10/31 - This can fail.
	LARGE_INTEGER l_Ticks;
	QueryPerformanceCounter(&l_Ticks);

	return l_Ticks.QuadPart;
}

// Get the elapsed time in milliseconds between to times.
//
// p_StartTicks:	Start time in ticks.
// p_EndTicks:		End time in ticks.
//
float TimerGetElapsedMilliseconds(__int64 const p_StartTicks, __int64 const p_EndTicks)
{
	// Get tick frequency in ticks/second.
	// NOTE - STL 2011/10/31 - This can fail.
	LARGE_INTEGER l_Frequency;
	QueryPerformanceFrequency(&l_Frequency);

	float l_ElapsedTimeMS = (p_EndTicks - p_StartTicks) * (1000.0f / l_Frequency.QuadPart); 
	return l_ElapsedTimeMS;
}
