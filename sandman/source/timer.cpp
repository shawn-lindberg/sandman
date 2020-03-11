#include "timer.h"

#if defined (_WIN32)
	#include <Windows.h>
#endif // defined (_WIN32)

#include <time.h>

// Functions
//

// Get the current time.
//
// p_Time:	(Output) The current time.
//
void TimerGetCurrent(Time& p_Time)
{
	#if defined (_WIN32)

		// NOTE - STL 2011/10/31 - This can fail.
		LARGE_INTEGER l_Ticks;
		QueryPerformanceCounter(&l_Ticks);

		// Get tick frequency in ticks/second.
		// NOTE - STL 2012/11/11 - It's inefficient to call this each time. 
		// NOTE - STL 2011/10/31 - This can fail.
		LARGE_INTEGER l_Frequency;
		QueryPerformanceFrequency(&l_Frequency);
	
		// Convert to our form.
		p_Time.m_Seconds = l_Ticks.QuadPart / l_Frequency.QuadPart;
		p_Time.m_Nanoseconds = ((l_Ticks.QuadPart % l_Frequency.QuadPart) * 1000000000) / 
			l_Frequency.QuadPart;
		
	#elif defined (__linux__)

		// NOTE - STL 2012/09/23 - This can fail.
		timespec l_Time;
		clock_gettime(CLOCK_REALTIME, &l_Time);

		p_Time.m_Seconds = l_Time.tv_sec;
		p_Time.m_Nanoseconds = l_Time.tv_nsec;

	#endif // defined (_WIN32)
}

// Get the elapsed time in milliseconds between to times.
//
// p_StartTime:	Start time.
// p_EndTime:		End time.
//
float TimerGetElapsedMilliseconds(Time const& p_StartTime, Time const& p_EndTime)
{
	// Calculate the elapsed time.
	Time l_ElapsedTime;
	
	l_ElapsedTime.m_Seconds = p_EndTime.m_Seconds - p_StartTime.m_Seconds;
	
	// Did the nanoseconds wrap into seconds?
	if (p_EndTime.m_Nanoseconds < p_StartTime.m_Nanoseconds)
	{
		// Borrow a second.
		l_ElapsedTime.m_Seconds--;
		
		// Add a billion nanoseconds in exchange.
		l_ElapsedTime.m_Nanoseconds = p_EndTime.m_Nanoseconds + (1000000000 - p_StartTime.m_Nanoseconds);
	}
	else
	{
		l_ElapsedTime.m_Nanoseconds = p_EndTime.m_Nanoseconds - p_StartTime.m_Nanoseconds;
	}
	
	// Convert to milliseconds.
	float const l_ElapsedTimeMS = (1.0e3f * l_ElapsedTime.m_Seconds) + 
		(l_ElapsedTime.m_Nanoseconds / 1.0e6f);
	return l_ElapsedTimeMS;
}
