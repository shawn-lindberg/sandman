#include "timer.h"

#if defined (_WIN32)
	#include <Windows.h>
#endif // defined (_WIN32)

#include <time.h>

// Functions
//

// Get the current time in ticks.
//
uint64_t TimerGetTicks()
{
	#if defined (_WIN32)

		// NOTE - STL 2011/10/31 - This can fail.
		LARGE_INTEGER l_Ticks;
		QueryPerformanceCounter(&l_Ticks);

		return l_Ticks.QuadPart;

	#elif defined (__linux__)

		// NOTE - STL 2012/09/23 - This can fail.
		timespec l_Time;
		clock_gettime(CLOCK_REALTIME, &l_Time);

		return l_Time.tv_nsec;

	#endif // defined (_WIN32)
}

// Get the elapsed time in milliseconds between to times.
//
// p_StartTicks:	Start time in ticks.
// p_EndTicks:		End time in ticks.
//
float TimerGetElapsedMilliseconds(uint64_t const p_StartTicks, uint64_t const p_EndTicks)
{
	#if defined (_WIN32)

		// Get tick frequency in ticks/second.
		// NOTE - STL 2011/10/31 - This can fail.
		LARGE_INTEGER l_Frequency;
		QueryPerformanceFrequency(&l_Frequency);

		float l_ElapsedTimeMS = (p_EndTicks - p_StartTicks) * (1000.0f / l_Frequency.QuadPart);
		return l_ElapsedTimeMS;

	#elif defined (__linux__)

		// Convert to milliseconds.
		float const l_ElapsedTimeMS = (p_EndTicks - p_StartTicks) / 1.0e-6f;
		return l_ElapsedTimeMS;

	#endif // defined (_WIN32)
}
