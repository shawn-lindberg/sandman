#pragma once

// Types
//

// Functions
//

// Get the current time in ticks.
//
__int64 TimerGetTicks();

// Get the elapsed time in milliseconds between to times.
//
// p_StartTicks:	Start time in ticks.
// p_EndTicks:		End time in ticks.
//
float TimerGetElapsedMilliseconds(__int64 const p_StartTicks, __int64 const p_EndTicks);