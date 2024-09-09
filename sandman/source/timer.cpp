#include "timer.h"

#if defined (_WIN32)
	#include <Windows.h>
#endif // defined (_WIN32)

#include <ctime>

// Functions
//

// Get the current time.
//
// time:	(Output) The current time.
//
void TimerGetCurrent(Time& time)
{
	#if defined (_WIN32)

		// NOTE - STL 2011/10/31 - This can fail.
		LARGE_INTEGER ticks;
		QueryPerformanceCounter(&ticks);

		// Get tick frequency in ticks/second.
		// NOTE - STL 2012/11/11 - It's inefficient to call this each time. 
		// NOTE - STL 2011/10/31 - This can fail.
		LARGE_INTEGER frequency;
		QueryPerformanceFrequency(&frequency);
	
		// Convert to our form.
		time.m_Seconds = ticks.QuadPart / frequency.QuadPart;
		time.m_Nanoseconds = ((ticks.QuadPart % frequency.QuadPart) * 1000000000) / 
			frequency.QuadPart;
		
	#elif defined (__linux__)

		// NOTE - STL 2012/09/23 - This can fail.
		timespec timeValue;
		clock_gettime(CLOCK_REALTIME, &timeValue);

		time.m_Seconds = timeValue.tv_sec;
		time.m_Nanoseconds = timeValue.tv_nsec;

	#endif // defined (_WIN32)
}

// Get the elapsed time in milliseconds between to times.
// Note: Will return -1 if the end time is less than the start time.
//
// startTime:	Start time.
// endTime:		End time.
//
float TimerGetElapsedMilliseconds(Time const& startTime, Time const& endTime)
{
	// For now just return a negative sentinel if the end time is before the start time.
	if (endTime < startTime)
	{
		return -1.0f;
	}

	// Calculate the elapsed time.
	Time elapsedTime;
	
	elapsedTime.m_Seconds = endTime.m_Seconds - startTime.m_Seconds;
	
	// Did the nanoseconds wrap into seconds?
	if (endTime.m_Nanoseconds < startTime.m_Nanoseconds)
	{
		// Borrow a second.
		elapsedTime.m_Seconds--;
		
		// Add a billion nanoseconds in exchange.
		elapsedTime.m_Nanoseconds = endTime.m_Nanoseconds + (1000000000 - startTime.m_Nanoseconds);
	}
	else
	{
		elapsedTime.m_Nanoseconds = endTime.m_Nanoseconds - startTime.m_Nanoseconds;
	}
	
	// Convert to milliseconds.
	float const elapsedTimeMS = (1.0e3f * elapsedTime.m_Seconds) +
										 (elapsedTime.m_Nanoseconds / 1.0e6f);

	return elapsedTimeMS;
}
