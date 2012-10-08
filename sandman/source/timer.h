#pragma once

#include <stdint.h>

// Types
//

// Functions
//

// Get the current time in ticks.
//
uint64_t TimerGetTicks();

// Get the elapsed time in milliseconds between to times.
//
// p_StartTicks:	Start time in ticks.
// p_EndTicks:		End time in ticks.
//
float TimerGetElapsedMilliseconds(uint64_t const p_StartTicks, uint64_t const p_EndTicks);
